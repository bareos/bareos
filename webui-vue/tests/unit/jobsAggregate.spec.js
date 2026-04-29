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
import { fetchAggregatedJobsPage } from '../../src/composables/jobsAggregate.js'

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

describe('jobs aggregate helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('merges and paginates jobs across multiple directors', async () => {
    const loading = fetchAggregatedJobsPage(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a', 'prod-b'],
      { page: 1, rowsPerPage: 2 },
      'T'
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()

    expect(JSON.parse(socketA.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'prod-a',
    })
    expect(JSON.parse(socketB.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'prod-b',
    })

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
        id: commandsA.get('llist jobs reverse limit=2 offset=0 jobstatus=T'),
        data: {
          jobs: [
            { jobid: '10', name: 'backup-a', clientname: 'fd-a', jobstatus: 'T', starttime: '2026-04-29 10:00:00' },
            { jobid: '9', name: 'backup-a-old', clientname: 'fd-a', jobstatus: 'T', starttime: '2026-04-29 08:00:00' },
          ],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('list jobs count jobstatus=T'),
        data: { jobs: [{ count: '2' }] },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist jobs reverse limit=2 offset=0 jobstatus=T'),
        data: {
          jobs: [
            { jobid: '11', name: 'backup-b', clientname: 'fd-b', jobstatus: 'T', starttime: '2026-04-29 11:00:00' },
            { jobid: '8', name: 'backup-b-old', clientname: 'fd-b', jobstatus: 'T', starttime: '2026-04-29 07:00:00' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs count jobstatus=T'),
        data: { jobs: [{ count: '2' }] },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({ scopeKey: 'prod-b:11', director: 'prod-b', id: 11 }),
        expect.objectContaining({ scopeKey: 'prod-a:10', director: 'prod-a', id: 10 }),
      ],
      totalJobs: 4,
      directorErrors: [],
    })
  })
})
