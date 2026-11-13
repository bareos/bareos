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
import {
  fetchAggregatedJobsPage,
  fetchAggregatedRecentJobsPage,
  fetchAggregatedTroubleJobs,
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
    setActivePinia(createPinia())
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
      'F',
      'B',
      'CommonJob',
      'common-fd'
    )

    const socketA = FakeWebSocket.instances[0]
    const socketB = FakeWebSocket.instances[1]
    socketA.open()
    socketB.open()

    expect(JSON.parse(socketA.sent[0])).toEqual({
      type: 'session',
      mode: 'json',
      director: 'prod-a',
    })
    expect(JSON.parse(socketB.sent[0])).toEqual({
      type: 'session',
      mode: 'json',
      director: 'prod-b',
    })

    socketA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    socketB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => {
      expect(socketA.sent).toHaveLength(4)
      expect(socketB.sent).toHaveLength(4)
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
        id: commandsA.get('llist jobs count jobstatus=T joblevel=F jobtype=B job="CommonJob" client="common-fd"'),
        data: { jobs: [{ count: '2' }] },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist jobs count jobstatus=T joblevel=F jobtype=B job="CommonJob" client="common-fd"'),
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
    const jobsCommandsA = socketCommands(socketA)
    const jobsCommandsB = socketCommands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsA.get('llist jobs reverse limit=2 offset=0 sortby=jobid jobstatus=T joblevel=F jobtype=B job="CommonJob" client="common-fd"'),
        data: {
          jobs: [
            { jobid: '10', name: 'CommonJob', clientname: 'common-fd', jobstatus: 'T', starttime: '2026-04-29 10:00:00' },
            { jobid: '9', name: 'CommonJob', clientname: 'common-fd', jobstatus: 'T', starttime: '2026-04-29 08:00:00' },
          ],
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsB.get('llist jobs reverse limit=2 offset=0 sortby=jobid jobstatus=T joblevel=F jobtype=B job="CommonJob" client="common-fd"'),
        data: {
          jobs: [
            { jobid: '11', name: 'CommonJob', clientname: 'common-fd', jobstatus: 'T', starttime: '2026-04-29 11:00:00' },
            { jobid: '8', name: 'CommonJob', clientname: 'common-fd', jobstatus: 'T', starttime: '2026-04-29 07:00:00' },
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
      truncated: false,
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
    await vi.waitFor(() => {
      expect(socketA.sent).toHaveLength(6)
      expect(socketB.sent).toHaveLength(6)
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
        id: commandsA.get('llist jobs count jobstatus=f'),
        data: { jobs: [{ count: '1' }] },
      }),
    })
    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsA.get('llist jobs count jobstatus=E'),
        data: { jobs: [{ count: '1' }] },
      }),
    })

    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist jobs count jobstatus=f'),
        data: { jobs: [{ count: '1' }] },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('llist jobs count jobstatus=E'),
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
    const jobsCommandsA = socketCommands(socketA)
    const jobsCommandsB = socketCommands(socketB)

    socketA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommandsA.get('llist jobs reverse limit=3 offset=0 sortby=jobid jobstatus=f'),
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
        id: jobsCommandsA.get('llist jobs reverse limit=3 offset=0 sortby=jobid jobstatus=E'),
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
        id: jobsCommandsB.get('llist jobs reverse limit=3 offset=0 sortby=jobid jobstatus=f'),
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
        id: jobsCommandsB.get('llist jobs reverse limit=3 offset=0 sortby=jobid jobstatus=E'),
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
      truncated: false,
      directorErrors: [],
    })
  })

  it('sends search server-side and keeps all rows for the All option', async () => {
    const loading = fetchAggregatedJobsPage(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a'],
      { page: 1, rowsPerPage: 0 },
      '',
      '',
      '',
      '',
      '',
      'fd-2'
    )

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(3)
    })

    const commands = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('llist jobs count search="fd-2"'),
        data: { jobs: [{ count: '2' }] },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('status director'),
        data: {
          running: [],
        },
      }),
    })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(4)
    })

    const jobsCommand = JSON.parse(socket.sent[3])
    expect(jobsCommand.command).toBe('llist jobs reverse limit=2 offset=0 search="fd-2"')

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: jobsCommand.id,
        data: {
          jobs: [
            { jobid: '10', name: 'backup-b', clientname: 'fd-2', jobstatus: 'T', starttime: '2026-04-29 10:00:00' },
            { jobid: '9', name: 'archive', clientname: 'fd-2', jobstatus: 'T', starttime: '2026-04-29 09:00:00' },
          ],
        },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({ scopeKey: 'prod-a:10', director: 'prod-a', id: 10, client: 'fd-2' }),
        expect.objectContaining({ scopeKey: 'prod-a:9', director: 'prod-a', id: 9, client: 'fd-2' }),
      ],
      totalJobs: 2,
      truncated: false,
      directorErrors: [],
    })
  })

  it.each([
    { counts: [600, 600], limits: [600, 600], rows: 1200, truncated: false },
    { counts: [1001, 2], limits: [1000, 2], rows: 1002, truncated: true },
    { counts: [1000, 1000], limits: [1000, 1000], rows: 2000, truncated: false },
  ])('reports fallback truncation per filter for $counts', async ({
    counts, limits, rows, truncated,
  }) => {
    const loading = fetchAggregatedJobsPage(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      { page: 1, rowsPerPage: 0, sortBy: 'duration', descending: true },
      ['f', 'E'],
    )
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(4))
    const commands = socket.sent.slice(1).map(payload => JSON.parse(payload))
    for (const [index, status] of ['f', 'E'].entries()) {
      const command = commands.find(entry => entry.command === `llist jobs count jobstatus=${status}`)
      socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: command.id, data: { jobs: [{ count: counts[index] }] },
        }),
      })
    }
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response', id: commands.find(entry => entry.command === 'status director').id,
        data: { running: [] },
      }),
    })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(6))
    for (const [index, status] of ['f', 'E'].entries()) {
      const command = socket.sent.slice(4).map(payload => JSON.parse(payload))
        .find(entry => entry.command === `llist jobs reverse limit=${limits[index]} offset=0 jobstatus=${status}`)
      expect(command).toBeDefined()
      socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: command.id,
          data: {
            jobs: Array.from({ length: limits[index] }, (_, row) => ({
              jobid: index * 10000 + row + 1, jobstatus: status, duration: '00:01:00',
            })),
          },
        }),
      })
    }
    const result = await loading
    expect(result.totalJobs).toBe(counts[0] + counts[1])
    expect(result.jobs).toHaveLength(rows)
    expect(result.truncated).toBe(truncated)
    expect(result.directorErrors).toEqual([])
  })

  it('keeps fallback rows and reports unavailable counts', async () => {
    const loading = fetchAggregatedJobsPage(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      { page: 1, rowsPerPage: 25, sortBy: 'duration', descending: true },
      'f',
    )
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(3))
    const commands = socket.sent.slice(1).map(payload => JSON.parse(payload))
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'error', id: commands.find(entry => entry.command === 'llist jobs count jobstatus=f').id,
        message: 'count denied',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response', id: commands.find(entry => entry.command === 'status director').id,
        data: { running: [] },
      }),
    })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(4))
    const command = JSON.parse(socket.sent[3])
    expect(command.command).toBe('llist jobs reverse limit=25 offset=0 jobstatus=f')
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response', id: command.id,
        data: { jobs: [{ jobid: 2, jobstatus: 'f' }, { jobid: 1, jobstatus: 'f' }] },
      }),
    })
    const result = await loading
    expect(result.jobs.map(job => job.id)).toEqual([2, 1])
    expect(result.directorErrors).toEqual([{
      director: 'prod-a', message: 'Failed to count jobs: count denied',
    }])
  })

  it('loads recent jobs with the same bounded server pagination', async () => {
    const loading = fetchAggregatedRecentJobsPage(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a'],
      { page: 2, rowsPerPage: 10, sortBy: 'starttime', descending: true }
    )

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(4)
    })

    const commands = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )
    expect(commands.has('llist jobs last current enabled reverse limit=20 offset=0 sortby=starttime')).toBe(true)
    expect(commands.has('llist jobs count last current enabled')).toBe(true)

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('llist jobs last current enabled reverse limit=20 offset=0 sortby=starttime'),
        data: {
          jobs: Array.from({ length: 20 }, (_, index) => ({
            jobid: String(20 - index),
            name: `Job-${20 - index}`,
            starttime: `2026-04-29 10:${String(59 - index).padStart(2, '0')}:00`,
          })),
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('llist jobs count last current enabled'),
        data: { jobs: [{ count: '25' }] },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('status director'),
        data: { running: [] },
      }),
    })

    const result = await loading
    expect(result).toMatchObject({
      totalJobs: 25,
      truncated: false,
      directorErrors: [],
    })
    expect(result.jobs).toHaveLength(10)
    expect(result.jobs[0]).toMatchObject({ id: 10 })
    expect(result.jobs.at(-1)).toMatchObject({ id: 1 })
  })

  it.each([
    ['status director', [2, 1], 2, []],
    ['llist jobs last current enabled reverse limit=2 offset=0 sortby=jobid', [], 0, [
      { director: 'prod-a', message: 'command denied' },
    ]],
    ['llist jobs count last current enabled', [], 0, [
      { director: 'prod-a', message: 'command denied' },
    ]],
  ])('handles denied recent-jobs command %s', async (deniedCommand, ids, totalJobs, directorErrors) => {
    const loading = fetchAggregatedRecentJobsPage(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      { page: 1, rowsPerPage: 2 }
    )
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => expect(socket.sent).toHaveLength(4))

    const responses = {
      'llist jobs last current enabled reverse limit=2 offset=0 sortby=jobid': {
        jobs: [
          { jobid: '2', name: 'Job-2', jobstatus: 'T' },
          { jobid: '1', name: 'Job-1', jobstatus: 'E' },
        ],
      },
      'llist jobs count last current enabled': { jobs: [{ count: '2' }] },
      'status director': { running: [] },
    }
    for (const payload of socket.sent.slice(1)) {
      const command = JSON.parse(payload)
      socket.onmessage?.({
        data: JSON.stringify(command.command === deniedCommand
          ? { type: 'error', id: command.id, message: 'command denied' }
          : { type: 'response', id: command.id, data: responses[command.command] }),
      })
    }

    const result = await loading
    expect(result.jobs.map(job => job.id)).toEqual(ids)
    expect(result).toMatchObject({ totalJobs, directorErrors, truncated: false })
  })

  it('loads trouble jobs with a bounded server-side prefilter', async () => {
    const loading = fetchAggregatedTroubleJobs(
      {
        username: 'admin',
        password: 'secret',
      },
      ['prod-a'],
      { limit: 2 }
    )

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(3)
    })

    const commands = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )
    expect(commands.has('llist jobs reverse limit=2 offset=0 sortby=jobid days=1 jobstatus=W,E,f,A')).toBe(true)
    expect(commands.has('llist jobs count days=1 jobstatus=W,E,f,A')).toBe(true)

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('llist jobs reverse limit=2 offset=0 sortby=jobid days=1 jobstatus=W,E,f,A'),
        data: {
          jobs: [
            { jobid: '20', name: 'warning-job', jobstatus: 'W' },
            { jobid: '19', name: 'error-job', jobstatus: 'E' },
          ],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commands.get('llist jobs count days=1 jobstatus=W,E,f,A'),
        data: { jobs: [{ count: '5' }] },
      }),
    })

    await expect(loading).resolves.toEqual({
      jobs: [
        expect.objectContaining({ scopeKey: 'prod-a:20', status: 'W' }),
        expect.objectContaining({ scopeKey: 'prod-a:19', status: 'E' }),
      ],
      totalJobs: 5,
      truncated: true,
      directorErrors: [],
    })
  })

})
