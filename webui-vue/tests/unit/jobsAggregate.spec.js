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
  fetchAggregatedJobsPage,
  sortJobsByPagination,
  usesDefaultJobsSorting,
} from '../../src/composables/jobsAggregate.js'

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

  it('sorts jobs by the selected pagination column', () => {
    expect(sortJobsByPagination([
      { id: 11, name: 'zeta', bytes: 25, duration: '00:00:05', status: 'T' },
      { id: 9, name: 'alpha', bytes: 10, duration: '00:00:05', status: 'E' },
      { id: 10, name: 'beta', bytes: 40, duration: '00:00:10', status: 'T' },
    ], { sortBy: 'name', descending: false }).map(job => job.name)).toEqual([
      'alpha',
      'beta',
      'zeta',
    ])

    expect(sortJobsByPagination([
      { id: 11, name: 'zeta', bytes: 25, duration: '00:00:05', status: 'T' },
      { id: 9, name: 'alpha', bytes: 10, duration: '00:00:05', status: 'E' },
      { id: 10, name: 'beta', bytes: 40, duration: '00:00:10', status: 'T' },
    ], { sortBy: 'speed', descending: true }).map(job => job.id)).toEqual([
      11,
      10,
      9,
    ])

    expect(usesDefaultJobsSorting({ sortBy: 'id', descending: true })).toBe(true)
    expect(usesDefaultJobsSorting({ sortBy: 'name', descending: false })).toBe(false)
  })

  it('merges and paginates jobs across multiple directors', async () => {
    const loading = fetchAggregatedJobsPage(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a', 'prod-b'],
      { page: 1, rowsPerPage: 2 },
      'T',
      'F'
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
        id: commandsA.get('list jobs count jobstatus=T joblevel=F'),
        data: { jobs: [{ count: '2' }] },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs count jobstatus=T joblevel=F'),
        data: { jobs: [{ count: '2' }] },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('status director'),
        data: {
          running: [
            { jobid: '10', status: 'is waiting for a mount request' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('status director'),
        data: {
          running: [],
        },
      }),
    })
    await Promise.resolve()
    const jobsCommandsA = socketCommands(socketA)
    const jobsCommandsB = socketCommands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsA.get('llist jobs reverse limit=2 offset=0 jobstatus=T joblevel=F'),
        data: {
          jobs: [
            { jobid: '10', name: 'backup-a', clientname: 'fd-a', jobstatus: 'T', starttime: '2026-04-29 10:00:00' },
            { jobid: '9', name: 'backup-a-old', clientname: 'fd-a', jobstatus: 'T', starttime: '2026-04-29 08:00:00' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsB.get('llist jobs reverse limit=2 offset=0 jobstatus=T joblevel=F'),
        data: {
          jobs: [
            { jobid: '11', name: 'backup-b', clientname: 'fd-b', jobstatus: 'T', starttime: '2026-04-29 11:00:00' },
            { jobid: '8', name: 'backup-b-old', clientname: 'fd-b', jobstatus: 'T', starttime: '2026-04-29 07:00:00' },
          ],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({ scopeKey: 'prod-b:11', director: 'prod-b', id: 11 }),
        expect.objectContaining({
          scopeKey: 'prod-a:10',
          director: 'prod-a',
          id: 10,
          runtimeStatus: 'is waiting for a mount request',
        }),
      ],
      totalJobs: 4,
      directorErrors: [],
    })
  })

  it('merges multi-status jobs across multiple directors', async () => {
    const loading = fetchAggregatedJobsPage(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a', 'prod-b'],
      { page: 1, rowsPerPage: 3 },
      ['f', 'E']
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
        id: commandsA.get('list jobs count jobstatus=f'),
        data: { jobs: [{ count: '1' }] },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('list jobs count jobstatus=E'),
        data: { jobs: [{ count: '1' }] },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs count jobstatus=f'),
        data: { jobs: [{ count: '1' }] },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs count jobstatus=E'),
        data: { jobs: [{ count: '1' }] },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('status director'),
        data: {
          running: [],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('status director'),
        data: {
          running: [],
        },
      }),
    })
    await Promise.resolve()
    await Promise.resolve()
    await Promise.resolve()

    const jobsCommandsA = socketCommands(socketA)
    const jobsCommandsB = socketCommands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsA.get('llist jobs reverse limit=3 offset=0 jobstatus=f'),
        data: {
          jobs: [
            { jobid: '31', name: 'failed-a', clientname: 'fd-a', jobstatus: 'f', starttime: '2026-04-29 10:00:00' },
          ],
        },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsA.get('llist jobs reverse limit=3 offset=0 jobstatus=E'),
        data: {
          jobs: [
            { jobid: '30', name: 'error-a', clientname: 'fd-a', jobstatus: 'E', starttime: '2026-04-29 09:00:00' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsB.get('llist jobs reverse limit=3 offset=0 jobstatus=f'),
        data: {
          jobs: [
            { jobid: '41', name: 'failed-b', clientname: 'fd-b', jobstatus: 'f', starttime: '2026-04-29 12:00:00' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsB.get('llist jobs reverse limit=3 offset=0 jobstatus=E'),
        data: {
          jobs: [
            { jobid: '40', name: 'error-b', clientname: 'fd-b', jobstatus: 'E', starttime: '2026-04-29 11:00:00' },
          ],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({ scopeKey: 'prod-b:41', director: 'prod-b', id: 41, status: 'f' }),
        expect.objectContaining({ scopeKey: 'prod-b:40', director: 'prod-b', id: 40, status: 'E' }),
        expect.objectContaining({ scopeKey: 'prod-a:31', director: 'prod-a', id: 31, status: 'f' }),
      ],
      totalJobs: 4,
      directorErrors: [],
    })
  })

})
