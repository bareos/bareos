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

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { fetchAggregatedClients } from '../../src/composables/clientsAggregate.js'

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

describe('clients aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges clients across multiple directors', async () => {
    const loading = fetchAggregatedClients(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a', 'prod-b']
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()

    socketA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    socketB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => {
      expect(socketA.sent).toHaveLength(3)
      expect(socketB.sent).toHaveLength(3)
    })

    const socketCommands = (socket) => new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    const commandsA = socketCommands(socketA)
    const commandsB = socketCommands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('llist clients'),
        data: {
          clients: [
            { name: 'alpha-fd', uname: '26.0.0 (27Apr26) Fedora,x86_64-pc-linux-gnu' },
          ],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('.clients'),
        data: {
          clients: [{ name: 'alpha-fd', enabled: false }],
        },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist clients'),
        data: {
          clients: [
            { name: 'beta-fd', uname: '26.0.0 (27Apr26) Debian,x86_64-pc-linux-gnu' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('.clients'),
        data: {
          clients: [{ name: 'beta-fd', enabled: true }],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      clients: [
        expect.objectContaining({
          scopeKey: 'prod-a:alpha-fd',
          director: 'prod-a',
          name: 'alpha-fd',
          enabled: false,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:beta-fd',
          director: 'prod-b',
          name: 'beta-fd',
          enabled: true,
        }),
      ],
      directorErrors: [],
    })
  })
})
