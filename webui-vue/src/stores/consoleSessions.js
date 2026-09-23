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
import { defaultDirectorWsUrl } from '../utils/directorCommandSocket.js'

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || defaultDirectorWsUrl()
const RAW_CMD_TIMEOUT_MS = 300_000
const COMPLETION_TIMEOUT_MS = 5_000
const EXIT_DISCONNECT_TIMEOUT_MS = 1_500
const KEEPALIVE_INTERVAL_MS = 20_000
const COMPLETION_NOISE_PREFIXES = [
  'Automatically selected Catalog:',
  'Using Catalog ',
  'cwd is: ',
]
const CONSOLE_NOISE_LINES = new Set([
  'You have messages.',
])
const RESTORE_TREE_PROMPTS = new Set(['$ ', '> '])
const RESTORE_TREE_COMPLETION_COMMANDS = {
  ls: '.ls',
  cd: '.lsdir',
  add: '.ls',
  mark: '.ls',
  m: '.ls',
  delete: '.lsmark',
  unmark: '.lsmark',
}
const ANSI_ESCAPE_SEQUENCE_RE = /\x1B\[[0-?]*[ -/]*[@-~]/g
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
    selectionActive: false,
    selectionBusy: false,
    selectionText: '',
    selectionLines: [],
    cmd: '',
    cursorPos: 0,
    history: [],
    historyIdx: -1,
    initialized: false,
    // Last terminal size (rows/cols) reported by the browser's xterm.js
    // instance. Kept here (rather than only sent once) so it can be
    // resent to the Director as soon as a session (re)connects.
    terminalSize: null,
  })
}

function createRuntime() {
  return {
    ws: null,
    closing: false,
    keepaliveTimer: null,
    exitDisconnectTimer: null,
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
        && !CONSOLE_NOISE_LINES.has(line)
        && !COMPLETION_NOISE_PREFIXES.some(prefix => line.startsWith(prefix))
      ))
  )]
}

function filterConsoleNoiseText(text) {
  const normalizedText = String(text ?? '').replace(/\r\n/g, '\n')
  if (!normalizedText) {
    return ''
  }

  const filteredLines = normalizedText
    .split('\n')
    .filter(line => !CONSOLE_NOISE_LINES.has(line))

  return filteredLines.join('\n')
}

function normalizeSelectionText(text) {
  // The Director marks the selected line with a literal "> " prefix (in
  // addition to the ANSI reverse-video escape codes) so that plain-text
  // consumers — including screen readers and braille displays that don't
  // render ANSI attributes — can tell which item is selected. Stripping
  // the escape codes here is therefore sufficient to produce readable
  // plain text; the marker itself is already part of the raw text.
  return String(text ?? '')
    .replace(/\r\n/g, '\n')
    .replace(ANSI_ESCAPE_SEQUENCE_RE, '')
}

// Matches the Director's per-line selection marker: an optional plain-text
// indicator (e.g. "> ") immediately followed by the ANSI reverse-video
// start code. Anything before the escape code is captured as the line's
// indent so it can be re-applied without the raw ">" character, since the
// WebUI conveys "selected" visually via full-row highlighting and via
// aria-current instead of a text marker.
const ANSI_INVERSE_SELECTED_LINE_RE = /^([^\x1B]*)\x1B\[7m(.*)$/

function parseSelectionLines(text) {
  return String(text ?? '')
    .replace(/\r\n/g, '\n')
    .split('\n')
    .map(line => {
      const match = line.match(ANSI_INVERSE_SELECTED_LINE_RE)
      if (match) {
        const indent = match[1].replace(/[^ \t]/g, ' ')
        return {
          text: (indent + match[2]).replace(ANSI_ESCAPE_SEQUENCE_RE, ''),
          selected: true,
        }
      }
      return {
        text: line.replace(ANSI_ESCAPE_SEQUENCE_RE, ''),
        selected: false,
      }
    })
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
    const replacement = (
      item.endsWith('=')
      || request.suppressAppend
    ) ? item : `${item} `
    session.cmd = replaceCompletionText(session.cmd, request, replacement)
    session.cursorPos = session.cmd.length
    return
  }

  const commonPrefix = longestCommonPrefix(items)
  if (commonPrefix.length > request.text.length) {
    session.cmd = replaceCompletionText(session.cmd, request, commonPrefix)
    session.cursorPos = session.cmd.length
  }
  appendLines(director, items.join('\n'))
}

function isRestoreTreePrompt(session) {
  return RESTORE_TREE_PROMPTS.has(session.currentPrompt)
}

function isInteractivePrompt(prompt) {
  return !!prompt && prompt !== '* '
}

function updateSessionPrompt(session, promptKind, promptText, isStreamingChunk) {
  if (isStreamingChunk && promptText) {
    session.currentPrompt = promptText
  } else if (!isStreamingChunk) {
    session.currentPrompt = promptText || (
      promptKind === 'select'
        ? 'Select: '
        : promptKind === 'sub'
          ? (session.currentPrompt || '> ')
          : '* '
    )
  }
}

function applyRawConsoleResponse(session, director, appendLines, writeToTerminal, message) {
  if (message.prompt === 'select' || message.prompt === 'select_busy') {
    if (!session.selectionActive) {
      // Entering an interactive selection: switch the terminal to the
      // alternate screen buffer, mirroring bconsole's
      // TerminalSelectionScreenGuard (console.cc), so normal scrollback
      // is preserved underneath and restored automatically on exit.
      writeToTerminal(director, '\x1B[?1049h')
    }
    session.selectionActive = true
    session.selectionBusy = message.prompt === 'select_busy'
    session.selectionText = normalizeSelectionText(message.text)
    session.selectionLines = parseSelectionLines(message.text)
    session.currentPrompt = ''
    // Clear the screen and home the cursor before each redraw, mirroring
    // the BNET_START_SELECT handling in console.cc, then forward the raw
    // selection frame (with its real ANSI reverse-video/background-color
    // escape codes) so xterm.js renders the exact same highlighted menu
    // a native bconsole would.
    writeToTerminal(director, `\x1B[2J\x1B[H${message.text}`)
    return {
      outputText: '',
      promptText: '',
    }
  }

  if (session.selectionActive) {
    // Leaving interactive selection: return to the main screen buffer and
    // start normal output on a fresh line.
    writeToTerminal(director, '\x1B[?1049l\r\n')
  }

  session.selectionActive = false
  session.selectionBusy = false
  session.selectionText = ''
  session.selectionLines = []
  const isStreamingChunk = message.prompt === 'more'
  const {
    outputText,
    promptText,
  } = splitInteractivePrompt(message.text, message.prompt)

  appendLines(director, outputText, '', isStreamingChunk)
  updateSessionPrompt(session, message.prompt, promptText, isStreamingChunk)

  return {
    outputText,
    promptText,
  }
}

function buildRestoreTreeCompletionRequest(command) {
  const context = getCompletionContext(command)
  const restoreCommand = context.firstKeyword || command.trim()
  const helper = RESTORE_TREE_COMPLETION_COMMANDS[restoreCommand]
  if (!helper) {
    return null
  }

  const rawToken = context.text ?? ''
  const lastSlashIndex = rawToken.lastIndexOf('/')
  const fragment = lastSlashIndex >= 0 ? rawToken.slice(lastSlashIndex + 1) : rawToken
  const replaceStart = lastSlashIndex >= 0
    ? context.replaceStart + lastSlashIndex + 1
    : context.replaceStart

  return {
    ...context,
    text: fragment,
    replaceStart,
    source: fragment ? `${helper} ${fragment}*` : helper,
    parser: 'items',
    suppressAppend: true,
  }
}

export const useConsoleSessionsStore = defineStore('consoleSessions', () => {
  const sessions = reactive({})
  const runtimes = new Map()
  // Per-director raw-text sink registered by the Console page once it has
  // mounted an xterm.js instance for that director's tab. Kept outside the
  // reactive session objects since it holds a plain function reference,
  // not session state.
  const terminalWriters = new Map()
  // Bounded per-director replay buffer of everything ever written to the
  // terminal (raw ANSI bytes, in order). Used to reproduce a director's
  // terminal contents when the Console page (re)mounts a terminal for it,
  // e.g. after switching tabs. Kept as plain arrays (not reactive state)
  // since only string concatenation/replay is needed, never rendering.
  const terminalLogs = new Map()
  const TERMINAL_LOG_MAX_CHARS = 200_000

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

  function setTerminalWriter(director, writer) {
    const normalizedDirector = director || DEFAULT_DIRECTOR_NAME
    if (writer) {
      terminalWriters.set(normalizedDirector, writer)
      // Replay everything written so far so a freshly mounted terminal
      // (e.g. after switching Director tabs) reproduces the existing
      // session content instead of starting blank.
      const log = terminalLogs.get(normalizedDirector)
      if (log && log.length) {
        writer(log.join(''))
      }
    } else {
      terminalWriters.delete(normalizedDirector)
    }
  }

  function writeToTerminal(director, text) {
    if (!text) {
      return
    }
    const normalizedDirector = director || DEFAULT_DIRECTOR_NAME

    let log = terminalLogs.get(normalizedDirector)
    if (!log) {
      log = []
      terminalLogs.set(normalizedDirector, log)
    }
    log.push(text)
    let logLength = log.reduce((total, chunk) => total + chunk.length, 0)
    while (logLength > TERMINAL_LOG_MAX_CHARS && log.length > 1) {
      logLength -= log.shift().length
    }

    terminalWriters.get(normalizedDirector)?.(text)
  }

  // Leaves interactive selection mode, restoring the terminal to the
  // main screen buffer if it was switched to the alternate one (see
  // applyRawConsoleResponse's 'select' handling above). Must be called
  // on every path that can end a selection -- not just the classic
  // "raw response with a non-select prompt" success path -- so an
  // error, a command timeout, or an unexpected WebSocket close never
  // leaves the user stuck on the alternate buffer with no way back to
  // their normal scrollback.
  function exitSelectionMode(session, director) {
    if (session.selectionActive) {
      writeToTerminal(director, '\x1B[?1049l\r\n')
    }
    session.selectionActive = false
    session.selectionBusy = false
    session.selectionText = ''
    session.selectionLines = []
  }

  // Sends the browser-reported terminal size to the Director using the
  // same symbolic protocol a native bconsole TTY client already uses (see
  // ua_dotcmds.cc's DotTerminalsizeCmd and
  // ua_tree_browser_internal.h's ParseTerminalResizeInput) — no proxy or
  // Director protocol changes are required.
  function setTerminalSize(director, rows, cols) {
    const session = getSession(director)
    if (!(rows > 0) || !(cols > 0)) {
      return
    }
    session.terminalSize = { rows, cols }
    sendTerminalSizeIfConnected(director)
  }

  function sendTerminalSizeIfConnected(director) {
    const session = getSession(director)
    const runtime = getRuntime(director)
    if (
      !session.terminalSize
      || !runtime.ws
      || runtime.ws.readyState !== WebSocket.OPEN
      || runtime.closing
    ) {
      return
    }

    const { rows, cols } = session.terminalSize
    if (session.selectionActive) {
      // Inside an interactive selection/dialog, size updates travel over
      // the same symbolic key-event channel as arrow keys etc.
      sendSelectionEvent(director, `resize:${rows}:${cols}`)
    } else {
      // At the main prompt, this is a normal (silent) dot-command — it
      // produces no output, exactly like the DotTerminalsizeCmd doc
      // comment describes ("Produces no output, since it is not meant to
      // be user-visible.").
      sendCommand(director, `.terminalsize ${rows} ${cols} color`)
    }
  }

  function appendLines(director, text, cls = '', continueLine = false) {
    const session = getSession(director)
    const normalizedText = filterConsoleNoiseText(text)
    if (!normalizedText) {
      return
    }

    // Forward the raw (ANSI-preserving) text verbatim to the mounted
    // xterm.js terminal, if any. xterm performs the actual ANSI/SGR
    // interpretation (colors, backgrounds, reverse video, cursor
    // movement) — this store no longer parses escape codes for display.
    writeToTerminal(director, normalizedText)

    if (!continueLine) {
      session.outputLineOpen = false
    }

    const appendLine = (line) => {
      const cleanLine = line.replace(ANSI_ESCAPE_SEQUENCE_RE, '')
      const lastLine = session.output[session.output.length - 1]
      if (session.outputLineOpen && lastLine && lastLine.cls === cls) {
        lastLine.text += cleanLine
      } else {
        session.output.push({ text: cleanLine, cls })
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
    const echoedLine = `${session.currentPrompt}${text}`
    session.output.push({
      text: echoedLine,
      cls: 'console-cmd',
    })
    session.outputLineOpen = false
    writeToTerminal(director, `${echoedLine}\n`)
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

  function clearKeepalive(director) {
    const runtime = getRuntime(director)
    if (runtime.keepaliveTimer) {
      clearInterval(runtime.keepaliveTimer)
      runtime.keepaliveTimer = null
    }
  }

  function startKeepalive(director) {
    const runtime = getRuntime(director)
    clearKeepalive(director)
    runtime.keepaliveTimer = setInterval(() => {
      if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN) {
        clearKeepalive(director)
        return
      }
      runtime.ws.send(JSON.stringify({ type: 'ping' }))
    }, KEEPALIVE_INTERVAL_MS)
  }

  function disconnectSession(director, options = {}) {
    const session = getSession(director)
    const runtime = getRuntime(director)

    clearKeepalive(director)
    clearTimeout(runtime.exitDisconnectTimer)
    runtime.exitDisconnectTimer = null
    rejectAll(director, options.reason ?? 'Disconnected')
    runtime.closing = true
    if (session.status !== 'disconnected') {
      appendInfo(director, 'Console disconnected.')
    }
    runtime.ws?.close()
    runtime.ws = null
    session.status = 'disconnected'
    session.currentPrompt = '* '
    exitSelectionMode(session, director)
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
      if (runtime.ws !== ws) {
        return
      }
      ws.send(JSON.stringify({
        type: 'session',
        mode: 'raw',
        director: director || creds.director || DEFAULT_DIRECTOR_NAME,
      }))
    }

    ws.onmessage = (event) => {
      if (runtime.ws !== ws) {
        return
      }

      let msg
      try {
        msg = JSON.parse(event.data)
      } catch {
        return
      }

      if (msg.type === 'auth_ok') {
        clearTimeout(runtime.exitDisconnectTimer)
        runtime.exitDisconnectTimer = null
        runtime.closing = false
        startKeepalive(director)
        session.status = 'connected'
        appendInfo(
          director,
          `Connected to ${msg.director} — type 'help' for commands, click here to type.`
        )
        // Mirror bconsole's initial SendTerminalSize() call so the
        // Director knows the browser's terminal size from the start of
        // the session (affects table/menu formatting width).
        sendTerminalSizeIfConnected(director)
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
          const isStreamingChunk = msg.prompt === 'more' || msg.prompt === 'select_busy'
          if (!isStreamingChunk) {
            const entry = runtime.pendingCmds.get(msg.id)
            if (entry) {
              clearTimeout(entry.timer)
              runtime.pendingCmds.delete(msg.id)
            }
          }

          applyRawConsoleResponse(session, director, appendLines, writeToTerminal, msg)
        }

        return
      }

      if (msg.type === 'error') {
        const entry = runtime.pendingCmds.get(msg.id)
        if (entry) {
          clearTimeout(entry.timer)
          runtime.pendingCmds.delete(msg.id)
        }
        exitSelectionMode(session, director)
        appendErr(director, msg.message)
      }
    }

    ws.onerror = () => {
      if (runtime.ws !== ws) {
        return
      }

      clearKeepalive(director)
      session.status = 'error'
      appendErr(director, `Cannot connect to proxy at ${WS_URL}`)
      rejectAll(director, 'WebSocket error')
      clearTimeout(runtime.exitDisconnectTimer)
      runtime.exitDisconnectTimer = null
      runtime.closing = false
    }

    ws.onclose = () => {
      if (runtime.ws !== ws) {
        return
      }

      clearKeepalive(director)
      if (session.status !== 'disconnected') {
        session.status = 'disconnected'
        if (!runtime.closing) {
          appendInfo(director, 'Console disconnected.')
        }
      }
      exitSelectionMode(session, director)
      rejectAll(director, 'WebSocket closed')
      clearTimeout(runtime.exitDisconnectTimer)
      runtime.exitDisconnectTimer = null
      runtime.closing = false
      runtime.ws = null
    }

    return true
  }

  function clearOutput(director) {
    const session = getSession(director)
    session.output = []
    session.outputLineOpen = false
    terminalLogs.set(director || DEFAULT_DIRECTOR_NAME, [])
    appendInfo(director, 'Console cleared.')
  }

  function sendCommand(director, command, timeoutMs = RAW_CMD_TIMEOUT_MS) {
    const session = getSession(director)
    const runtime = getRuntime(director)
    const previousPrompt = session.currentPrompt
    const normalizedCommand = String(command ?? '').trim().toLowerCase()

    if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN) {
      appendErr(director, 'Not connected to director.')
      return false
    }

    if (runtime.closing) {
      appendInfo(director, 'Console is disconnecting. Reconnect before sending commands.')
      return false
    }

    const id = String(++runtime.cmdSeq)
    const timer = setTimeout(() => {
      if (!runtime.pendingCmds.has(id)) {
        return
      }

      runtime.pendingCmds.delete(id)
      exitSelectionMode(session, director)
      appendErr(director, 'Command timed out.')
    }, timeoutMs)

    runtime.pendingCmds.set(id, { timer })
    runtime.ws.send(JSON.stringify({ type: 'command', id, command, stream: true }))
    if (normalizedCommand === 'exit'
      || normalizedCommand === 'quit'
      || normalizedCommand === '.quit') {
      runtime.closing = true
      session.status = 'disconnecting'
      appendInfo(director, 'Console disconnected.')
      clearTimeout(runtime.exitDisconnectTimer)
      runtime.exitDisconnectTimer = setTimeout(() => {
        if (runtime.ws !== null && runtime.closing) {
          runtime.ws.close()
        }
      }, EXIT_DISCONNECT_TIMEOUT_MS)
    }
    if (isInteractivePrompt(previousPrompt)) {
      session.currentPrompt = ''
    }
    if (!runtime.closing) {
      session.status = 'connected'
    }
    return true
  }

  function requestCompletion(director, command) {
    const session = getSession(director)
    const runtime = getRuntime(director)

    if (!runtime.ws || runtime.ws.readyState !== WebSocket.OPEN || runtime.closing) {
      return false
    }

    session.cmd = command
    session.cursorPos = command.length
    const request = (
      isRestoreTreePrompt(session)
        ? buildRestoreTreeCompletionRequest(command)
        : null
    ) || buildCompletionRequest(command)
    const id = String(++runtime.cmdSeq)
    const timer = setTimeout(() => {
      runtime.completionRequests.delete(id)
      runtime.pendingCmds.delete(id)
    }, COMPLETION_TIMEOUT_MS)

    runtime.completionRequests.set(id, request)
    runtime.pendingCmds.set(id, { timer })
    runtime.ws.send(JSON.stringify({
      type: 'command',
      id,
      command: request.source,
    }))
    session.status = 'connected'
    return true
  }

  function sendSelectionEvent(director, event) {
    const session = getSession(director)
    if (!session.selectionActive || session.selectionBusy) {
      return false
    }
    return sendCommand(director, event)
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
    sendSelectionEvent,
    requestCompletion,
    setTerminalWriter,
    setTerminalSize,
  }
})

/**
 * Apply a keydown event to console session editing state (cmd + cursorPos).
 * Returns true if the event was handled (caller should call preventDefault),
 * false if it should be passed through (e.g. Tab, Enter, Ctrl+C handled by
 * the caller, or unrecognised shortcuts).
 *
 * @param {object} session  - reactive session with { cmd, cursorPos }
 * @param {{ key: string, ctrlKey?: boolean, altKey?: boolean }} event
 * @returns {boolean}
 */
export function applyConsoleKey(session, event) {
  const { key, ctrlKey = false, altKey = false } = event

  if (key === 'ArrowLeft') {
    if (session.cursorPos > 0) session.cursorPos--
    return true
  }
  if (key === 'ArrowRight') {
    if (session.cursorPos < session.cmd.length) session.cursorPos++
    return true
  }
  if (key === 'Home' || (ctrlKey && key === 'a')) {
    session.cursorPos = 0
    return true
  }
  if (key === 'End' || (ctrlKey && key === 'e')) {
    session.cursorPos = session.cmd.length
    return true
  }
  if (key === 'Backspace') {
    if (session.cursorPos > 0) {
      session.cmd = session.cmd.slice(0, session.cursorPos - 1) + session.cmd.slice(session.cursorPos)
      session.cursorPos--
    }
    return true
  }
  if (key === 'Delete') {
    if (session.cursorPos < session.cmd.length) {
      session.cmd = session.cmd.slice(0, session.cursorPos) + session.cmd.slice(session.cursorPos + 1)
    }
    return true
  }
  if (ctrlKey && key === 'k') {
    session.cmd = session.cmd.slice(0, session.cursorPos)
    return true
  }
  if (ctrlKey && key === 'u') {
    session.cmd = session.cmd.slice(session.cursorPos)
    session.cursorPos = 0
    return true
  }
  if (key.length === 1 && !ctrlKey && !altKey) {
    session.cmd = session.cmd.slice(0, session.cursorPos) + key + session.cmd.slice(session.cursorPos)
    session.cursorPos++
    return true
  }
  return false
}
