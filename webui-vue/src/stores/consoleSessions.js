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

function createSession(director) {
  return reactive({
    director,
    status: 'disconnected',
    currentPrompt: '* ',
    output: [],
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
    completionIds: new Set(),
  }
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
    const lines = String(text ?? '').replace(/\r\n/g, '\n').split('\n')
    while (lines.length && lines[lines.length - 1].trim() === '') {
      lines.pop()
    }
    lines.forEach(line => session.output.push({ text: line, cls }))
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
  }

  function rejectAll(director, reason) {
    const runtime = getRuntime(director)
    for (const { timer, reject } of runtime.pendingCmds.values()) {
      clearTimeout(timer)
      reject?.(new Error(reason))
    }
    runtime.pendingCmds.clear()
    runtime.completionIds.clear()
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
        const entry = runtime.pendingCmds.get(msg.id)
        if (entry) {
          clearTimeout(entry.timer)
          runtime.pendingCmds.delete(msg.id)
        }

        if (runtime.completionIds.delete(msg.id)) {
          const lines = String(msg.text ?? '')
            .replace(/\r\n/g, '\n')
            .split('\n')
            .filter(line => line.trim())

          if (lines.length === 1) {
            session.cmd = lines[0]
          } else if (lines.length > 1) {
            appendLines(director, msg.text)
          }
        } else {
          appendLines(director, msg.text)
          session.currentPrompt = msg.prompt === 'select'
            ? 'Select: '
            : msg.prompt === 'sub'
              ? '> '
              : '* '
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
    runtime.ws.send(JSON.stringify({ type: 'command', id, command }))
    session.status = 'connected'
    return true
  }

  function requestCompletion(director, command) {
    const runtime = getRuntime(director)

    if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN) {
      return false
    }

    const id = String(++runtime.cmdSeq)
    const timer = setTimeout(() => {
      runtime.completionIds.delete(id)
      runtime.pendingCmds.delete(id)
    }, COMPLETION_TIMEOUT_MS)

    runtime.completionIds.add(id)
    runtime.pendingCmds.set(id, { timer })
    runtime.ws.send(JSON.stringify({ type: 'command', id, command: `${command}\t` }))
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
