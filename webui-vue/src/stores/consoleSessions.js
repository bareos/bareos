/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import { computed, reactive } from 'vue'
import { defineStore } from 'pinia'
import { DEFAULT_DIRECTOR_NAME, useAuthStore } from './auth.js'

function defaultWsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${window.location.host}/ws`
}

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || defaultWsUrl()
const RAW_CMD_TIMEOUT_MS = 300_000
const COMPLETION_TIMEOUT_MS = 5_000
const COMPLETION_NOISE_PREFIXES = [
  'Automatically selected Catalog:',
  'Using Catalog ',
]
const COMPLETION_KEYWORDS = [
  { key: 'pool=', cmd: '.pool' },
  { key: 'nextpool=', cmd: '.pool' },
  { key: 'fileset=', cmd: '.fileset' },
  { key: 'client=', cmd: '.client' },
  { key: 'jobdefs=', cmd: '.jobdefs' },
  { key: 'job=', cmd: '.jobs' },
  { key: 'restore_job=', cmd: '.jobs type=R' },
  { key: 'level=', cmd: '.level' },
  { key: 'storage=', cmd: '.storage' },
  { key: 'schedule=', cmd: '.schedule' },
  { key: 'volume=', cmd: '.media' },
  { key: 'oldvolume=', cmd: '.media' },
  { key: 'volstatus=', cmd: '.volstatus' },
  { key: 'catalog=', cmd: '.catalogs' },
  { key: 'message=', cmd: '.msgs' },
  { key: 'profile=', cmd: '.profiles' },
  { key: 'actiononpurge=', cmd: '.actiononpurge' },
]

function createSession(director) {
  return reactive({
    director,
    status: 'disconnected',
    currentPrompt: '* ',
    output: [],
    outputLineOpen: false,
    cmd: '',
    history: [],
    historyIdx: -1,
    initialized: false,
  })
}

function createRuntime() {
  return {
    ws: null,
    cmdSeq: 0,
    pendingCmds: new Map(),
    completionRequests: new Map(),
  }
}

function getFirstKeyword(line) {
  const firstSpace = line.indexOf(' ')
  return firstSpace > 0 ? line.slice(0, firstSpace) : null
}

function getCompletionContext(line) {
  const currentPoint = Math.max(0, line.length - 1)
  const separatorIndex = Math.max(line.lastIndexOf(' ', currentPoint), line.lastIndexOf('=', currentPoint))
  const text = separatorIndex >= 0 ? line.slice(separatorIndex + 1) : line

  let previousKeyword = null
  if (separatorIndex >= 0) {
    let i = separatorIndex
    while (i >= 0 && (line[i] === ' ' || line[i] === '=')) {
      i -= 1
    }
    if (i >= 0) {
      let start = i
      while (start > 0 && line[start - 1] !== ' ') {
        start -= 1
      }
      previousKeyword = line.slice(start, i + 1)
      if (line[separatorIndex] === '=') {
        previousKeyword += '='
      }
    }
  }

  return {
    text,
    replaceStart: separatorIndex + 1,
    previousKeyword,
    firstKeyword: getFirstKeyword(line),
  }
}

function parseSimpleCompletionItems(text) {
  return [...new Set(
    String(text ?? '')
      .replace(/\r\n/g, '\n')
      .split('\n')
      .map(line => line.trim())
      .filter(line => (
        line
        && line !== 'You have messages.'
        && !COMPLETION_NOISE_PREFIXES.some(prefix => line.startsWith(prefix))
      ))
  )]
}

function parseHelpCompletionItems(text) {
  const matches = String(text ?? '').match(/([a-z_]+=|[a-z]+(?=\s|$))/g)
  return [...new Set(matches ?? [])]
}

function longestCommonPrefix(values) {
  if (!values.length) return ''
  let prefix = values[0]
  for (const value of values.slice(1)) {
    let index = 0
    while (index < prefix.length && index < value.length && prefix[index] === value[index]) {
      index += 1
    }
    prefix = prefix.slice(0, index)
    if (!prefix) break
  }
  return prefix
}

function replaceCompletionText(line, request, replacement) {
  return `${line.slice(0, request.replaceStart)}${replacement}`
}

function splitInteractivePrompt(text, promptKind) {
  const normalizedText = String(text ?? '').replace(/\r\n/g, '\n')
  const canCarryInlinePrompt = (
    promptKind === 'sub'
    || promptKind === 'select'
    || promptKind === 'more'
  )

  if (
    !normalizedText
    || normalizedText.endsWith('\n')
    || !canCarryInlinePrompt
  ) {
    return {
      outputText: normalizedText,
      promptText: '',
    }
  }

  const lastNewlineIndex = normalizedText.lastIndexOf('\n')
  const promptText = lastNewlineIndex >= 0
    ? normalizedText.slice(lastNewlineIndex + 1)
    : normalizedText
  const outputText = lastNewlineIndex >= 0
    ? normalizedText.slice(0, lastNewlineIndex + 1)
    : ''

  if (
    promptKind === 'more'
    && promptText !== '$ '
    && promptText !== '> '
    && !promptText.startsWith('Select ')
  ) {
    return {
      outputText: normalizedText,
      promptText: '',
    }
  }

  return {
    outputText,
    promptText,
  }
}

function buildCompletionRequest(command) {
  const context = getCompletionContext(command)
  const mapping = COMPLETION_KEYWORDS.find(item => item.key === context.previousKeyword)
  if (mapping) {
    return { ...context, source: mapping.cmd, parser: 'items' }
  }

  if (context.previousKeyword && context.firstKeyword) {
    return { ...context, source: `.help item=${context.firstKeyword}`, parser: 'help' }
  }

  return { ...context, source: '.help all', parser: 'items' }
}

function applyCompletionResult(session, director, appendLines, request, text) {
  const items = (
    request.parser === 'help'
      ? parseHelpCompletionItems(text)
      : parseSimpleCompletionItems(text)
  ).filter(item => item.startsWith(request.text))

  if (!items.length) {
    return
  }

  if (items.length === 1) {
    const item = items[0]
    const replacement = item.endsWith('=') ? item : `${item} `
    session.cmd = replaceCompletionText(session.cmd, request, replacement)
    return
  }

  const commonPrefix = longestCommonPrefix(items)
  if (commonPrefix.length > request.text.length) {
    session.cmd = replaceCompletionText(session.cmd, request, commonPrefix)
  }
  appendLines(director, items.join('\n'))
}

export const useConsoleSessionsStore = defineStore('consoleSessions', () => {
  const sessions = reactive({})
  const runtimes = new Map()

  const directors = computed(() => Object.keys(sessions))

  function getSession(director = DEFAULT_DIRECTOR_NAME) {
    const normalizedDirector = director || DEFAULT_DIRECTOR_NAME

    if (!sessions[normalizedDirector]) {
      sessions[normalizedDirector] = createSession(normalizedDirector)
      runtimes.set(normalizedDirector, createRuntime())
    }

    return sessions[normalizedDirector]
  }

  function getRuntime(director = DEFAULT_DIRECTOR_NAME) {
    getSession(director)
    return runtimes.get(director || DEFAULT_DIRECTOR_NAME)
  }

  function appendLines(director, text, cls = '') {
    const session = getSession(director)
    const normalizedText = String(text ?? '').replace(/\r\n/g, '\n')
    if (!normalizedText) {
      return
    }

    const appendLine = (line) => {
      const lastLine = session.output[session.output.length - 1]
      if (session.outputLineOpen && lastLine && lastLine.cls === cls) {
        lastLine.text += line
      } else {
        session.output.push({ text: line, cls })
      }
    }

    let start = 0
    for (let i = 0; i < normalizedText.length; i += 1) {
      if (normalizedText[i] !== '\n') {
        continue
      }

      appendLine(normalizedText.slice(start, i))
      session.outputLineOpen = false
      start = i + 1
    }

    const trailingLine = normalizedText.slice(start)
    if (trailingLine) {
      appendLine(trailingLine)
      session.outputLineOpen = true
    } else if (normalizedText.endsWith('\n')) {
      session.outputLineOpen = false
    }
  }

  function appendInfo(director, text) {
    appendLines(director, text, 'console-info')
  }

  function appendErr(director, text) {
    appendLines(director, text, 'console-err')
  }

  function appendCommand(director, text) {
    const session = getSession(director)
    session.output.push({
      text: `${session.currentPrompt}${text}`,
      cls: 'console-cmd',
    })
    session.outputLineOpen = false
  }

  function rejectAll(director, reason) {
    const runtime = getRuntime(director)
    for (const { timer, reject } of runtime.pendingCmds.values()) {
      clearTimeout(timer)
      reject?.(new Error(reason))
    }
    runtime.pendingCmds.clear()
    runtime.completionRequests.clear()
  }

  function disconnectSession(director, options = {}) {
    const session = getSession(director)
    const runtime = getRuntime(director)

    rejectAll(director, options.reason ?? 'Disconnected')
    runtime.ws?.close()
    runtime.ws = null
    session.status = 'disconnected'
    session.currentPrompt = '* '
    if (options.resetInitialized) {
      session.initialized = false
    }
  }

  function connectSession(director, credentials) {
    const session = getSession(director)
    const runtime = getRuntime(director)
    const auth = useAuthStore()
    const creds = credentials ?? auth.getCredentials()

    if (!creds?.password) {
      appendErr(director, 'Not logged in — cannot open console.')
      return false
    }

    if (
      runtime.ws
      && (
        runtime.ws.readyState === WebSocket.CONNECTING
        || runtime.ws.readyState === WebSocket.OPEN
      )
    ) {
      return true
    }

    session.status = 'connecting'
    appendInfo(director, 'Connecting to director…')

    const ws = new WebSocket(WS_URL)
    runtime.ws = ws

    ws.onopen = () => {
      ws.send(JSON.stringify({
        type: 'auth',
        mode: 'raw',
        username: creds.username,
        password: creds.password,
        director: director || creds.director || DEFAULT_DIRECTOR_NAME,
      }))
    }

    ws.onmessage = (event) => {
      let msg
      try {
        msg = JSON.parse(event.data)
      } catch {
        return
      }

      if (msg.type === 'auth_ok') {
        session.status = 'connected'
        appendInfo(
          director,
          `Connected to ${msg.director} — type 'help' for commands, click here to type.`
        )
        return
      }

      if (msg.type === 'auth_error') {
        session.status = 'error'
        appendErr(director, `Authentication error: ${msg.message}`)
        return
      }

      if (msg.type === 'raw_response') {
        const completionRequest = runtime.completionRequests.get(msg.id)
        if (completionRequest) {
          const entry = runtime.pendingCmds.get(msg.id)
          if (entry) {
            clearTimeout(entry.timer)
            runtime.pendingCmds.delete(msg.id)
          }
          runtime.completionRequests.delete(msg.id)
          applyCompletionResult(
            session,
            director,
            appendLines,
            completionRequest,
            msg.text
          )
        } else {
          const isStreamingChunk = msg.prompt === 'more'
          const {
            outputText,
            promptText,
          } = splitInteractivePrompt(msg.text, msg.prompt)

          if (!isStreamingChunk) {
            const entry = runtime.pendingCmds.get(msg.id)
            if (entry) {
              clearTimeout(entry.timer)
              runtime.pendingCmds.delete(msg.id)
            }
          }

          appendLines(director, outputText)
          if (isStreamingChunk && promptText) {
            session.currentPrompt = promptText
          } else if (!isStreamingChunk) {
            session.currentPrompt = promptText || (
              msg.prompt === 'select'
                ? 'Select: '
                : msg.prompt === 'sub'
                  ? (session.currentPrompt || '> ')
                  : '* '
            )
          }
        }

        return
      }

      if (msg.type === 'error') {
        const entry = runtime.pendingCmds.get(msg.id)
        if (entry) {
          clearTimeout(entry.timer)
          runtime.pendingCmds.delete(msg.id)
        }
        appendErr(director, msg.message)
      }
    }

    ws.onerror = () => {
      session.status = 'error'
      appendErr(director, `Cannot connect to proxy at ${WS_URL}`)
      rejectAll(director, 'WebSocket error')
    }

    ws.onclose = () => {
      if (session.status !== 'disconnected') {
        session.status = 'disconnected'
        appendInfo(director, 'Console disconnected.')
      }
      rejectAll(director, 'WebSocket closed')
      runtime.ws = null
    }

    return true
  }

  function clearOutput(director) {
    const session = getSession(director)
    session.output = []
    session.outputLineOpen = false
    appendInfo(director, 'Console cleared.')
  }

  function sendCommand(director, command, timeoutMs = RAW_CMD_TIMEOUT_MS) {
    const session = getSession(director)
    const runtime = getRuntime(director)

    if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN) {
      appendErr(director, 'Not connected to director.')
      return false
    }

    const id = String(++runtime.cmdSeq)
    const timer = setTimeout(() => {
      if (!runtime.pendingCmds.has(id)) {
        return
      }

      runtime.pendingCmds.delete(id)
      appendErr(director, 'Command timed out.')
    }, timeoutMs)

    runtime.pendingCmds.set(id, { timer })
    runtime.ws.send(JSON.stringify({ type: 'command', id, command, stream: true }))
    session.status = 'connected'
    return true
  }

  function requestCompletion(director, command) {
    const session = getSession(director)
    const runtime = getRuntime(director)

    if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN) {
      return false
    }

    session.cmd = command
    const request = buildCompletionRequest(command)
    const id = String(++runtime.cmdSeq)
    const timer = setTimeout(() => {
      runtime.completionRequests.delete(id)
      runtime.pendingCmds.delete(id)
    }, COMPLETION_TIMEOUT_MS)

    runtime.completionRequests.set(id, request)
    runtime.pendingCmds.set(id, { timer })
    runtime.ws.send(JSON.stringify({ type: 'command', id, command: request.source }))
    session.status = 'connected'
    return true
  }

  function disconnectAll(options = {}) {
    for (const director of directors.value) {
      disconnectSession(director, options)
    }
  }

  return {
    directors,
    getSession,
    appendInfo,
    appendErr,
    appendCommand,
    connectSession,
    disconnectSession,
    disconnectAll,
    clearOutput,
    sendCommand,
    requestCompletion,
  }
})
