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
import {
  fetchAggregatedAutochangerStorages,
  fetchAggregatedStoragesState,
} from '../../src/composables/storagesAggregate.js'

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

describe('storages aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges storages, pools, and volumes across multiple directors', async () => {
    const loading = fetchAggregatedStoragesState(
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
    await Promise.resolve()

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
        id: commandsA.get('list storages'),
        data: {
          storages: [{ name: 'store-a', autochanger: '0', enabled: '1' }],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('llist pools'),
        data: {
          pools: [{
            name: 'Full',
            numvols: '1',
            maxvols: '10',
            prunablevolumes: '1',
            prunablejobs: '2',
            prunablebytes: '33',
          }],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('llist volumes'),
        data: {
          volumes: [{ volumename: 'Vol-A', pool: 'Full', volbytes: '11' }],
        },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list storages'),
        data: {
          storages: [{ name: 'store-b', autochanger: '1', enabled: '0' }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist pools'),
        data: {
          pools: [{
            name: 'Full',
            numvols: '2',
            maxvols: '20',
            prunablevolumes: '0',
            prunablejobs: '0',
            prunablebytes: '0',
          }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist volumes'),
        data: {
          volumes: [{ volumename: 'Vol-B', pool: 'Full', volbytes: '22' }],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      storages: [
        expect.objectContaining({
          scopeKey: 'prod-a:store-a',
          director: 'prod-a',
          name: 'store-a',
          enabled: true,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:store-b',
          director: 'prod-b',
          name: 'store-b',
          autochanger: true,
          enabled: false,
        }),
      ],
      pools: [
        expect.objectContaining({
          scopeKey: 'prod-a:Full',
          director: 'prod-a',
          totalbytes: 11,
          prunablevolumes: 1,
          prunablejobs: 2,
          prunablebytes: 33,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:Full',
          director: 'prod-b',
          totalbytes: 22,
          prunablevolumes: 0,
          prunablejobs: 0,
          prunablebytes: 0,
        }),
      ],
      volumes: [
        expect.objectContaining({
          scopeKey: 'prod-a:Vol-A',
          director: 'prod-a',
          volumename: 'Vol-A',
          volbytes: 11,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:Vol-B',
          director: 'prod-b',
          volumename: 'Vol-B',
          volbytes: 22,
        }),
      ],
      directorErrors: [],
    })
  })

  it('merges autochanger storages across multiple directors', async () => {
    const loading = fetchAggregatedAutochangerStorages(
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
    await Promise.resolve()

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
        id: commandsA.get('list storages'),
        data: {
          storages: [
            { name: 'file-a', autochanger: '0', enabled: '1' },
            { name: 'tape-a', autochanger: '1', enabled: '1' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list storages'),
        data: {
          storages: [
            { name: 'tape-b', autochanger: '1', enabled: '0' },
          ],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      storages: [
        expect.objectContaining({
          scopeKey: 'prod-a:tape-a',
          director: 'prod-a',
          label: 'prod-a / tape-a',
          name: 'tape-a',
          autochanger: true,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:tape-b',
          director: 'prod-b',
          label: 'prod-b / tape-b',
          name: 'tape-b',
          autochanger: true,
          enabled: false,
        }),
      ],
      directorErrors: [],
    })
  })
})
