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
})
