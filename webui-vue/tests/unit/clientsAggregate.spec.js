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

import { createPinia, setActivePinia } from 'pinia'
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

async function respondToClientAggregate(socket) {
  socket.open()
  socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
  await vi.waitFor(() => expect(socket.sent).toHaveLength(5))

  const commandIds = new Map(socket.sent.slice(1).map((payload) => {
    const command = JSON.parse(payload)
    return [command.command, command.id]
  }))
  const responses = [
    ['llist clients', { clients: [] }],
    ['.clients', { clients: [] }],
    ['llist jobs reverse limit=1000 sortby=starttime jobtype=B', { jobs: [] }],
    ['status scheduler days=-31,1', { schedules: [], preview: [] }],
  ]
  for (const [command, data] of responses) {
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get(command),
        data,
      }),
    })
  }
}

describe('clients aggregate helpers', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('bypasses the cached clients aggregate when forced', async () => {
    const credentials = { username: 'admin', password: 'secret' }
    const initialRequest = fetchAggregatedClients(credentials, ['director-a'])
    await respondToClientAggregate(FakeWebSocket.instances[0])
    await initialRequest

    await fetchAggregatedClients(credentials, ['director-a'])
    expect(FakeWebSocket.instances).toHaveLength(1)

    const forcedRequest = fetchAggregatedClients(
      credentials,
      ['director-a'],
      { forceRefresh: true }
    )
    expect(FakeWebSocket.instances).toHaveLength(2)
    await respondToClientAggregate(FakeWebSocket.instances[1])
    await forcedRequest
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
      expect(socketA.sent).toHaveLength(5)
      expect(socketB.sent).toHaveLength(5)
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
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
        data: {
          jobs: [{ jobid: 7, client: 'alpha-fd', jobstatus: 'T', jobtype: 'B' }],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('status scheduler days=-31,1'),
        data: {
          schedules: [{
            name: 'Nightly',
            enabled: true,
            jobs: [{ name: 'Nightly', client: 'alpha-fd', enabled: true }],
          }],
          preview: [{ schedule: 'Nightly', runtime: Math.floor(Date.now() / 1000) - 3600 }],
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
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
        data: {
          jobs: [{ jobid: 8, clientname: 'beta-fd', jobstatus: 'E', jobtype: 'B' }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('status scheduler days=-31,1'),
        data: {
          schedules: [],
          preview: [],
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
      recentBackups: [
        expect.objectContaining({
          id: 7,
          client: 'alpha-fd',
          status: 'T',
          type: 'B',
          director: 'prod-a',
        }),
        expect.objectContaining({
          id: 8,
          client: 'beta-fd',
          status: 'E',
          type: 'B',
          director: 'prod-b',
        }),
      ],
      scheduledBackups: [
        expect.objectContaining({
          client: 'alpha-fd',
          job: 'Nightly',
          schedule: 'Nightly',
          director: 'prod-a',
        }),
      ],
      recentBackupErrors: [],
      directorErrors: [],
    })
  })

  it('keeps clients when recent backup history cannot be loaded', async () => {
    const loading = fetchAggregatedClients(
      { username: 'admin', password: 'secret' },
      ['prod-a']
    )
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(5))

    const commands = new Map(socket.sent.slice(1).map((payload) => {
      const command = JSON.parse(payload)
      return [command.command, command.id]
    }))
    const respond = (command, data) => {
      socket.onmessage?.({
        data: JSON.stringify({
          type: 'response',
          id: commands.get(command),
          data,
        }),
      })
    }

    respond('llist clients', { clients: [{ name: 'alpha-fd' }] })
    respond('.clients', { clients: [{ name: 'alpha-fd', enabled: true }] })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'error',
        id: commands.get('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
        message: 'history request denied',
      }),
    })
    respond('status scheduler days=-31,1', { schedules: [], preview: [] })

    await expect(loading).resolves.toMatchObject({
      clients: [expect.objectContaining({ name: 'alpha-fd', director: 'prod-a' })],
      recentBackups: [],
      recentBackupErrors: [{
        director: 'prod-a',
        message: 'history request denied',
      }],
      directorErrors: [],
    })
  })
})
