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
      type: 'auth',
      mode: 'raw',
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
    expect(JSON.parse(socketB.sent[0])).toEqual({
      type: 'auth',
      mode: 'raw',
      username: 'admin',
      password: 'secret',
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
    })
    expect(completionB).toEqual({
      type: 'command',
      id: '1',
      command: 'list cl\t',
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'list clients',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir-b').cmd).toBe('list clients')
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
        text: 'list clients ',
        prompt: 'main',
      }),
    })

    expect(consoleSessions.getSession('bareos-dir').cmd).toBe('list clients ')
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
