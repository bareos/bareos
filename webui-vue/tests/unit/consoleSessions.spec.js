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

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useConsoleSessionsStore } from '../../src/stores/consoleSessions.js'

class FakeWebSocket {
  static instances = []
  static CONNECTING = 0
  static OPEN = 1
  static CLOSING = 2
  static CLOSED = 3

  constructor(url) {
    this.url = url
    this.readyState = FakeWebSocket.CONNECTING
    this.sent = []
    this.onopen = null
    this.onmessage = null
    this.onerror = null
    this.onclose = null
    FakeWebSocket.instances.push(this)
  }

  send(payload) {
    this.sent.push(payload)
  }

  open() {
    this.readyState = FakeWebSocket.OPEN
    this.onopen?.()
  }

  close() {
    this.readyState = FakeWebSocket.CLOSED
    this.onclose?.()
  }
}

describe('console session store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  it('opens isolated raw sessions per director', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir-a', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
    consoleSessions.connectSession('bareos-dir-b', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-b',
    })

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()

    expect(JSON.parse(socketA.sent[0])).toEqual({
      type: 'session',
      mode: 'raw',
      director: 'bareos-dir-a',
    })
    expect(JSON.parse(socketB.sent[0])).toEqual({
      type: 'session',
      mode: 'raw',
      director: 'bareos-dir-b',
    })

    socketA.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-a' }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-b' }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '99',
        text: 'status output',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir-a').status).toBe('connected')
    expect(consoleSessions.getSession('bareos-dir-b').status).toBe('connected')
    expect(
      consoleSessions.getSession('bareos-dir-b').output.map(line => line.text)
    ).toContain('status output')
    expect(
      consoleSessions.getSession('bareos-dir-a').output.map(line => line.text)
    ).not.toContain('status output')
  })

  it('routes commands and completions through the selected director session', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir-a', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
    consoleSessions.connectSession('bareos-dir-b', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-b',
    })

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()
    socketA.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-a' }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-b' }),
    })

    consoleSessions.sendCommand('bareos-dir-a', 'list jobs')
    consoleSessions.requestCompletion('bareos-dir-b', 'list cl')

    const commandA = JSON.parse(socketA.sent[1])
    const completionB = JSON.parse(socketB.sent[1])

    expect(commandA).toEqual({
      type: 'command',
      id: '1',
      command: 'list jobs',
      stream: true,
    })
    expect(completionB).toEqual({
      type: 'command',
      id: '1',
      command: '.help item=list',
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'basefiles | clients | jobs | pools',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir-b').cmd).toBe('list clients ')
    expect(consoleSessions.getSession('bareos-dir-a').cmd).toBe('')
  })

  it('preserves trailing spaces returned by command completion', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.requestCompletion('bareos-dir', 'list cl')

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'clients',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir').cmd).toBe('list clients ')
  })

  it('replaces Director-owned selection snapshots and sends key events', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> 1: Alpha\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.selectionActive).toBe(true)
    expect(session.selectionText).toContain('> 1: Alpha')

    expect(consoleSessions.sendSelectionEvent('bareos-dir', 'key:down')).toBe(true)
    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: 'key:down',
      stream: true,
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n  1: Alpha\n> 2: Beta\n',
        prompt: 'select',
      }),
    })
    expect(session.selectionText).toContain('> 2: Beta')
    expect(session.output.map(line => line.text)).not.toContain('> 1: Alpha')
  })

  it('normalizes terminal inverse selection markers for browser display', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> \u001B[7m1: Alpha\u001B[0m\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    const selectionText = consoleSessions.getSession('bareos-dir').selectionText
    expect(selectionText).toContain('> 1: Alpha')
    expect(selectionText).toContain('  2: Beta')
    expect(selectionText).not.toMatch(/\x1B/)

    const selectionLines = consoleSessions.getSession('bareos-dir').selectionLines
    expect(selectionLines).toContainEqual({ text: '  1: Alpha', selected: true })
    expect(selectionLines).toContainEqual({ text: '  2: Beta', selected: false })
  })

  it('forwards raw ANSI output verbatim to the registered terminal writer', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '\u001B[31mERROR\u001B[0m \u001B[32mOK\u001B[0m\n',
        prompt: 'main',
      }),
    })

    // The store no longer parses ANSI/SGR codes itself — it forwards the
    // raw bytes verbatim so xterm.js can interpret them, exactly as a
    // real terminal emulator would.
    expect(written).toContain('\u001B[31mERROR\u001B[0m \u001B[32mOK\u001B[0m\n')

    // The plain-text (ANSI-stripped) representation is still kept in
    // `output` for non-rendering consumers (e.g. completion parsing).
    const line = consoleSessions.getSession('bareos-dir').output.find(entry => entry.text === 'ERROR OK')
    expect(line).toBeTruthy()
    expect(line.segments).toBeUndefined()
  })

  it('preserves raw ANSI background/foreground codes used for terminal frames', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    const frame = '\u001B[97;44m  Bright-white text on blue background  \u001B[0m\n'
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: frame,
        prompt: 'main',
      }),
    })

    expect(written).toContain(frame)
  })

  it('wraps interactive selection frames in alternate-screen and clear sequences', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    written.length = 0
    const frame = 'Select:\n> \u001B[7m1: Alpha\u001B[0m\n  2: Beta\n'
    socket.onmessage?.({
      data: JSON.stringify({ type: 'raw_response', id: '1', text: frame, prompt: 'select' }),
    })

    // Entering selection mode switches to the alternate screen buffer
    // (mirroring bconsole's TerminalSelectionScreenGuard), then clears
    // and redraws the frame (mirroring the BNET_START_SELECT handling in
    // console.cc) — this makes each arrow-key redraw overwrite in place
    // instead of scrolling.
    expect(written).toEqual(['\u001B[?1049h', `\u001B[2J\u001B[H${frame}`])

    written.length = 0
    socket.onmessage?.({
      data: JSON.stringify({ type: 'raw_response', id: '2', text: 'Done\n', prompt: 'main' }),
    })

    // Leaving selection mode returns to the main screen buffer before
    // any further normal output is written.
    expect(written[0]).toBe('\u001B[?1049l\r\n')
    expect(written[1]).toBe('Done\n')
  })

  it('restores the main screen buffer if a command times out mid-selection', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> 1: Alpha\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.selectionActive).toBe(true)

    written.length = 0
    consoleSessions.sendSelectionEvent('bareos-dir', 'key:down')
    vi.advanceTimersByTime(300_000)

    // A pending command timing out mid-selection must not leave the
    // terminal stuck on the alternate screen buffer.
    expect(session.selectionActive).toBe(false)
    expect(written).toContain('\u001B[?1049l\r\n')
  })

  it('restores the main screen buffer if the Director reports an error mid-selection', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> 1: Alpha\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.selectionActive).toBe(true)

    written.length = 0
    socket.onmessage?.({
      data: JSON.stringify({ type: 'error', id: '1', message: 'boom' }),
    })

    expect(session.selectionActive).toBe(false)
    expect(written).toContain('\u001B[?1049l\r\n')
  })

  it('restores the main screen buffer if the WebSocket closes unexpectedly mid-selection', () => {
    const consoleSessions = useConsoleSessionsStore()
    const written = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => written.push(text))

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> 1: Alpha\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.selectionActive).toBe(true)

    written.length = 0
    socket.onclose?.()

    expect(session.selectionActive).toBe(false)
    expect(written).toContain('\u001B[?1049l\r\n')
  })

  it('replays prior terminal output to a newly registered writer', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'first line\n',
        prompt: 'main',
      }),
    })

    // Simulate switching away from and back to this director's tab: a
    // fresh terminal instance registers its writer and should receive
    // the full prior output in one shot instead of starting blank.
    const replayed = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => replayed.push(text))

    expect(replayed.join('')).toContain('first line')

    // Further live output continues to reach the newly registered writer.
    replayed.length = 0
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '2',
        text: 'second line\n',
        prompt: 'main',
      }),
    })
    expect(replayed).toEqual(['second line\n'])
  })

  it('clears the replay buffer when the console output is cleared', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({ type: 'raw_response', id: '1', text: 'stale line\n', prompt: 'main' }),
    })

    consoleSessions.clearOutput('bareos-dir')

    const replayed = []
    consoleSessions.setTerminalWriter('bareos-dir', (text) => replayed.push(text))
    expect(replayed.join('')).not.toContain('stale line')
    expect(replayed.join('')).toContain('Console cleared.')
  })

  it('uses value completion commands for known argument keywords', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.requestCompletion('bareos-dir', 'list jobs client=ba')

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: '.client',
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'test2-fd\ntest-client-fd\nbareos-fd\n',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir').cmd).toBe('list jobs client=bareos-fd ')
  })

  it('uses restore-tree helper commands for cd completion', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    session.currentPrompt = '$ '

    consoleSessions.requestCompletion('bareos-dir', 'cd /ho')

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: '.lsdir ho*',
    })
  })

  it('lists restore-tree directory candidates for empty cd completion', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    session.currentPrompt = '$ '

    consoleSessions.requestCompletion('bareos-dir', 'cd ')

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: '.lsdir',
    })
  })

  it('updates restore tree input from .lsdir completion results', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    session.currentPrompt = '$ '

    consoleSessions.requestCompletion('bareos-dir', 'cd /ho')
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'home/\n',
        prompt: 'other',
      }),
    })

    expect(session.cmd).toBe('cd /home/')
  })

  it('lists ambiguous completion candidates in the console output', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.requestCompletion('bareos-dir', 'list cl')
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'client=\nclients\n',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir').cmd).toBe('list client')
    expect(
      consoleSessions.getSession('bareos-dir').output.map(line => line.text)
    ).toContain('client=')
    expect(
      consoleSessions.getSession('bareos-dir').output.map(line => line.text)
    ).toContain('clients')
  })

  it('moves interactive restore prompts onto the live input line', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.sendCommand('bareos-dir', 'restore')
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: [
          'To select the JobIds, you have the following choices:',
          ' 1: List last 20 Jobs run',
          '13: Cancel',
          'Select item:  (1-13): ',
        ].join('\n'),
        prompt: 'sub',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.currentPrompt).toBe('Select item:  (1-13): ')
    expect(session.output.map(line => line.text)).toContain(
      'To select the JobIds, you have the following choices:'
    )
    expect(session.output.map(line => line.text)).not.toContain(
      'Select item:  (1-13): '
    )
  })

  it('clears stale interactive prompts while restore work is running', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    session.currentPrompt = 'Select FileSet resource (1-2): '

    consoleSessions.sendCommand('bareos-dir', '1')

    expect(session.currentPrompt).toBe('')
  })

  it('moves streamed restore prompts onto the live input line', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    consoleSessions.sendCommand('bareos-dir', 'find *')

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: 'find *',
      stream: true,
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '/home/\n/home/pai/\n$ ',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '',
        prompt: 'sub',
      }),
    })

    expect(session.output.map(line => line.text)).toContain('/home/')
    expect(session.output.map(line => line.text)).toContain('/home/pai/')
    expect(session.currentPrompt).toBe('$ ')
    expect(session.output.map(line => line.text)).not.toContain('$ ')
  })

  it('keeps streamed restore progress on one output line', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.sendCommand('bareos-dir', '5')

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Building directory tree for JobId(s) 1 ...  ',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '+',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '+',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '\n86 files inserted into the tree.\n$ ',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '',
        prompt: 'sub',
      }),
    })

    const session = consoleSessions.getSession('bareos-dir')
    expect(session.output.map(line => line.text)).toContain(
      'Building directory tree for JobId(s) 1 ...  ++'
    )
    expect(session.output.map(line => line.text)).toContain(
      '86 files inserted into the tree.'
    )
  })

  it('keeps info messages on separate lines', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(consoleSessions.getSession('bareos-dir').output.map(line => line.text)).toEqual([
      'Connecting to director…',
      "Connected to bareos-dir — type 'help' for commands, click here to type.",
    ])
  })

  it('sends keepalive pings while the console is connected', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(socket.sent).toHaveLength(1)

    vi.advanceTimersByTime(20_000)
    expect(JSON.parse(socket.sent[1])).toEqual({ type: 'ping' })

    vi.advanceTimersByTime(20_000)
    expect(JSON.parse(socket.sent[2])).toEqual({ type: 'ping' })

    consoleSessions.disconnectSession('bareos-dir', { reason: 'Disconnected' })
    vi.advanceTimersByTime(20_000)
    expect(socket.sent).toHaveLength(3)
  })

  it('sends the terminal size as a silent .terminalsize command on connect', () => {
    const consoleSessions = useConsoleSessionsStore()

    // Reported before connecting, mirroring the terminal having already
    // been fitted by the time the WebSocket session is established.
    consoleSessions.setTerminalSize('bareos-dir', 40, 120)

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: '.terminalsize 40 120 color',
      stream: true,
    })

    // The command is sent silently — it must not appear as a visible
    // command echo in the session output.
    const session = consoleSessions.getSession('bareos-dir')
    expect(session.output.some(line => line.text.includes('.terminalsize'))).toBe(false)
  })

  it('sends a resize: pseudo-input instead while a selection is active', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'Select:\n> \u001B[7m1: Alpha\u001B[0m\n  2: Beta\n',
        prompt: 'select',
      }),
    })

    consoleSessions.setTerminalSize('bareos-dir', 30, 100)

    const lastSent = JSON.parse(socket.sent[socket.sent.length - 1])
    expect(lastSent.command).toBe('resize:30:100')
  })

  it('suppresses standalone director message notifications in console output', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    consoleSessions.sendCommand('bareos-dir', 'messages')
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'You have no messages.\nYou have messages.\n',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir').output.map(line => line.text)).toContain(
      'You have no messages.'
    )
    expect(consoleSessions.getSession('bareos-dir').output.map(line => line.text)).not.toContain(
      'You have messages.'
    )
  })

  it('disconnects all tracked director sessions', () => {
    const consoleSessions = useConsoleSessionsStore()

    consoleSessions.connectSession('bareos-dir-a', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
    consoleSessions.connectSession('bareos-dir-b', {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-b',
    })

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()
    socketA.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-a' }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir-b' }),
    })

    consoleSessions.disconnectAll({ reason: 'Disconnected', resetInitialized: true })

    expect(consoleSessions.getSession('bareos-dir-a').status).toBe('disconnected')
    expect(consoleSessions.getSession('bareos-dir-b').status).toBe('disconnected')
    expect(socketA.readyState).toBe(FakeWebSocket.CLOSED)
    expect(socketB.readyState).toBe(FakeWebSocket.CLOSED)
  })

  it('can reconnect and continue after exit closes the console session', () => {
    const consoleSessions = useConsoleSessionsStore()

    const credentials = {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    }

    consoleSessions.connectSession('bareos-dir', credentials)
    const firstSocket = FakeWebSocket.instances[0]
    firstSocket.open()
    firstSocket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(consoleSessions.sendCommand('bareos-dir', 'exit')).toBe(true)
    expect(JSON.parse(firstSocket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: 'exit',
      stream: true,
    })

    firstSocket.onclose?.()
    expect(consoleSessions.getSession('bareos-dir').status).toBe('disconnected')

    consoleSessions.connectSession('bareos-dir', credentials)
    const secondSocket = FakeWebSocket.instances[1]
    secondSocket.open()
    secondSocket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(consoleSessions.sendCommand('bareos-dir', 'status director')).toBe(true)
    expect(JSON.parse(secondSocket.sent[1])).toEqual({
      type: 'command',
      id: '2',
      command: 'status director',
      stream: true,
    })
  })

  it('does not send follow-up commands while exit is disconnecting', () => {
    const consoleSessions = useConsoleSessionsStore()

    const credentials = {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    }

    consoleSessions.connectSession('bareos-dir', credentials)
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(consoleSessions.sendCommand('bareos-dir', 'exit')).toBe(true)
    expect(consoleSessions.getSession('bareos-dir').status).toBe('disconnecting')

    expect(consoleSessions.sendCommand('bareos-dir', 'status director')).toBe(false)
    expect(socket.sent).toHaveLength(2)
  })

  it('forces disconnect after exit if server does not close the socket', () => {
    const consoleSessions = useConsoleSessionsStore()

    const credentials = {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    }

    consoleSessions.connectSession('bareos-dir', credentials)
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    expect(consoleSessions.sendCommand('bareos-dir', 'exit')).toBe(true)
    expect(consoleSessions.getSession('bareos-dir').status).toBe('disconnecting')

    vi.advanceTimersByTime(1_500)
    expect(consoleSessions.getSession('bareos-dir').status).toBe('disconnected')
    expect(socket.readyState).toBe(FakeWebSocket.CLOSED)
  })

  it('ignores late close events from a stale socket after reconnect', () => {
    const consoleSessions = useConsoleSessionsStore()

    const credentials = {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    }

    consoleSessions.connectSession('bareos-dir', credentials)
    const firstSocket = FakeWebSocket.instances[0]
    firstSocket.open()
    firstSocket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    firstSocket.close = function closeWithoutEvent() {
      this.readyState = FakeWebSocket.CLOSED
    }

    consoleSessions.disconnectSession('bareos-dir', { reason: 'Disconnected' })
    consoleSessions.connectSession('bareos-dir', credentials)

    const secondSocket = FakeWebSocket.instances[1]
    secondSocket.open()
    secondSocket.onmessage?.({
      data: JSON.stringify({ type: 'auth_ok', director: 'bareos-dir' }),
    })

    firstSocket.onclose?.()

    expect(consoleSessions.getSession('bareos-dir').status).toBe('connected')

    const sent = consoleSessions.sendCommand('bareos-dir', 'status director')
    expect(sent).toBe(true)
    expect(JSON.parse(secondSocket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: 'status director',
      stream: true,
    })
  })
})
