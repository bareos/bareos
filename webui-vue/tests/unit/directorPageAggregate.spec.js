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
  fetchAggregatedDirectorMessages,
  fetchAggregatedDirectorStatus,
} from '../../src/composables/directorPageAggregate.js'

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

function commandIds(socket) {
  return new Map(
    socket.sent.slice(1).map((payload) => {
      const command = JSON.parse(payload)
      return [command.command, command.id]
    })
  )
}

describe('director page aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges status data across multiple directors', async () => {
    const loading = fetchAggregatedDirectorStatus(
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

    const commandsA = commandIds(socketA)
    const commandsB = commandIds(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('status director'),
        data: {
          header: { director: 'prod-a-dir', jobs_run: 10 },
          scheduled: [{ name: 'Nightly', scheduled: '2026-04-30 01:00:00' }],
          running: [{ jobid: 11, name: 'ActiveJob', start_time: '2026-04-30 02:00:00' }],
          terminated: [{ jobid: 12, name: 'DoneJob', finished: '2026-04-30 00:30:00' }],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('status director'),
        data: {
          header: { director: 'prod-b-dir', jobs_run: 20 },
          scheduled: [{ name: 'Weekly', scheduled: '2026-05-01 01:00:00' }],
          running: [],
          terminated: [{ jobid: 22, name: 'OtherDoneJob', finished: '2026-04-29 23:59:00' }],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      snapshots: [
        expect.objectContaining({
          director: 'prod-a',
          header: expect.objectContaining({ director: 'prod-a-dir', jobs_run: 10 }),
          scheduledJobs: [expect.objectContaining({ scopeKey: 'prod-a:scheduled:Nightly' })],
          runningJobs: [expect.objectContaining({ scopeKey: 'prod-a:running:11' })],
          terminatedJobs: [expect.objectContaining({ scopeKey: 'prod-a:terminated:12' })],
        }),
        expect.objectContaining({
          director: 'prod-b',
          header: expect.objectContaining({ director: 'prod-b-dir', jobs_run: 20 }),
          scheduledJobs: [expect.objectContaining({ scopeKey: 'prod-b:scheduled:Weekly' })],
          runningJobs: [],
          terminatedJobs: [expect.objectContaining({ scopeKey: 'prod-b:terminated:22' })],
        }),
      ],
      directorErrors: [],
    })
  })

  it('merges messages and keeps director failures visible', async () => {
    const loading = fetchAggregatedDirectorMessages(
      { username: 'admin', password: 'secret' },
      ['prod-a', 'prod-b'],
      50
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()
    socketA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    socketB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await Promise.resolve()

    const commandsA = commandIds(socketA)
    const commandsB = commandIds(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('list log limit=50'),
        data: {
          log: [
            { logid: 1, time: '2026-04-30 02:00:00', logtext: 'Message A' },
            { logid: 2, time: '2026-04-30 01:00:00', logtext: 'Message B' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'error',
        id: commandsB.get('list log limit=50'),
        message: 'Access denied',
      }),
    })

    await expect(loading).resolves.toEqual({
      logEntries: [
        expect.objectContaining({
          scopeKey: 'prod-a:log:1',
          director: 'prod-a',
          logtext: 'Message A',
        }),
        expect.objectContaining({
          scopeKey: 'prod-a:log:2',
          director: 'prod-a',
          logtext: 'Message B',
        }),
      ],
      directorErrors: [
        { director: 'prod-b', message: 'Access denied' },
      ],
    })
  })
})
