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
import { useDirectorStore } from '../../src/stores/director.js'

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

describe('director store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
  })

  it('sends director host and port during websocket auth', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
      host: 'dir.example.test',
      port: 19101,
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()

    expect(socket.sent).toHaveLength(1)
    expect(JSON.parse(socket.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
      host: 'dir.example.test',
      port: 19101,
    })
  })

  it('stores director transport from auth_ok', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'cleartext',
      }),
    })

    expect(director.status).toBe('connected')
    expect(director.transport).toBe('cleartext')
  })
})
