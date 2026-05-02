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
  fetchAggregatedAnalytics,
  fetchAggregatedAnalyticsHistory,
} from '../../src/composables/analyticsAggregate.js'

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

describe('analytics aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges jobs across multiple directors', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' },
      ['prod-a', 'prod-b']
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()
    socketA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    socketB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await Promise.resolve()

    const commands = (socket) => new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )
    const commandsA = commands(socketA)
    const commandsB = commands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('list jobs'),
        data: {
          jobs: [{ jobid: 10, name: 'BackupCatalog', client: 'fd-a', jobbytes: 11, jobfiles: 3 }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs'),
        data: {
          jobs: [{ jobid: 20, name: 'BackupCatalog', client: 'fd-a', jobbytes: 22, jobfiles: 6 }],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({
          scopeKey: 'prod-a:10',
          director: 'prod-a',
          id: 10,
          bytes: 11,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:20',
          director: 'prod-b',
          id: 20,
          bytes: 22,
        }),
      ],
      directorErrors: [],
    })
  })

  it('merges historical metrics across multiple directors', async () => {
    const loading = fetchAggregatedAnalyticsHistory(
      { username: 'admin', password: 'secret' },
      ['prod-a', 'prod-b'],
      { hours: 24, pool: 'Full' },
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()
    socketA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    socketB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await Promise.resolve()

    const commands = (socket) => new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )
    const commandsA = commands(socketA)
    const commandsB = commands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('list pools'),
        data: {
          pools: [{ name: 'Full' }, { name: 'Incremental' }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list pools'),
        data: {
          pools: [{ name: 'Full' }],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('.metrics kind=system hours=24 cf=last'),
        data: {
          available: true,
          data_source_names: ['total_bytes', 'total_files'],
          series: [
            { timestamp: 100, total_bytes: '100', total_files: '10' },
            { timestamp: 160, total_bytes: '200', total_files: '20' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('.metrics kind=system hours=24 cf=last'),
        data: {
          available: true,
          data_source_names: ['total_bytes', 'total_files'],
          series: [
            { timestamp: 100, total_bytes: '300', total_files: '30' },
            { timestamp: 160, total_bytes: '400', total_files: '40' },
          ],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('.metrics kind=pool pool="Full" hours=24 cf=last'),
        data: {
          available: true,
          data_source_names: ['total_bytes', 'total_files'],
          series: [
            { timestamp: 100, total_bytes: '50', total_files: '5' },
            { timestamp: 160, total_bytes: '60', total_files: '6' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('.metrics kind=pool pool="Full" hours=24 cf=last'),
        data: {
          available: true,
          data_source_names: ['total_bytes', 'total_files'],
          series: [
            { timestamp: 100, total_bytes: '70', total_files: '7' },
            { timestamp: 160, total_bytes: '80', total_files: '8' },
          ],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      poolNames: ['Full', 'Incremental'],
      systemHistory: {
        available: true,
        start: 0,
        end: 0,
        step: 0,
        dataSourceNames: ['total_bytes', 'total_files'],
        points: [
          { timestamp: 100, values: { total_bytes: 400, total_files: 40 } },
          { timestamp: 160, values: { total_bytes: 600, total_files: 60 } },
        ],
      },
      poolHistory: {
        available: true,
        start: 0,
        end: 0,
        step: 0,
        dataSourceNames: ['total_bytes', 'total_files'],
        points: [
          { timestamp: 100, values: { total_bytes: 120, total_files: 12 } },
          { timestamp: 160, values: { total_bytes: 140, total_files: 14 } },
        ],
      },
      directorErrors: [],
    })
  })
})
