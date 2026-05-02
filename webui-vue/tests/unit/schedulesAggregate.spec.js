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
  fetchAggregatedSchedulesShow,
  fetchAggregatedSchedulesStatus,
} from '../../src/composables/schedulesAggregate.js'

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

describe('schedules aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges shown schedules across directors', async () => {
    const loading = fetchAggregatedSchedulesShow(
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
        id: commandsA.get('show schedules'),
        data: {
          schedules: { Nightly: { name: 'Nightly', enabled: true, run: ['Level=Full sun at 01:00'] } },
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('show schedules'),
        data: {
          schedules: { Nightly: { name: 'Nightly', enabled: false, run: ['Level=Diff mon-sat at 01:00'] } },
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      schedules: [
        expect.objectContaining({
          scopeKey: 'prod-a:Nightly',
          director: 'prod-a',
          displayName: 'prod-a / Nightly',
          enabled: true,
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:Nightly',
          director: 'prod-b',
          displayName: 'prod-b / Nightly',
          enabled: false,
        }),
      ],
      directorErrors: [],
    })
  })

  it('merges scheduler status and preview across directors', async () => {
    const loading = fetchAggregatedSchedulesStatus(
      { username: 'admin', password: 'secret' },
      ['prod-a', 'prod-b'],
      { from: 0, to: 1 }
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
        id: commandsA.get('status scheduler days=0,1'),
        data: {
          schedules: [{ name: 'Nightly', enabled: true, jobs: [{ name: 'BackupCatalog', enabled: true }] }],
          preview: [{ schedule: 'Nightly', runtime: 1_746_000_000 }],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('show schedules'),
        data: { schedules: { Nightly: { name: 'Nightly', enabled: true } } },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('status scheduler days=0,1'),
        data: {
          schedules: [],
          preview: [],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('show schedules'),
        data: { schedules: { Weekly: { name: 'Weekly', enabled: false } } },
      }),
    })

    await expect(loading).resolves.toEqual({
      schedulesData: [
        expect.objectContaining({
          scopeKey: 'prod-a:Nightly',
          director: 'prod-a',
          displayName: 'prod-a / Nightly',
          jobs: [expect.objectContaining({ name: 'BackupCatalog', director: 'prod-a' })],
        }),
        expect.objectContaining({
          scopeKey: 'prod-b:Weekly',
          director: 'prod-b',
          displayName: 'prod-b / Weekly',
          enabled: false,
        }),
      ],
      previewData: [
        expect.objectContaining({
          director: 'prod-a',
          scheduleKey: 'prod-a:Nightly',
          scheduleDisplay: 'prod-a / Nightly',
        }),
      ],
      directorErrors: [],
    })
  })
})
