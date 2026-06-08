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
  aggregateDirectorDashboardSnapshots,
  fetchDirectorDashboardSnapshot,
} from '../../src/composables/directorAggregate.js'

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

describe('director aggregate dashboard helpers', () => {
  beforeEach(() => {
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('fetches a dashboard snapshot from the selected director', async () => {
    const snapshotPromise = fetchDirectorDashboardSnapshot({
      username: 'admin',
      password: 'secret',
      director: 'prod-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    expect(JSON.parse(socket.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'prod-dir',
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'tls',
      }),
    })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(8)
    })

    const commandIds = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('llist jobs days=1'),
        data: {
          jobs: [{
            jobid: '7',
            name: 'BackupClient1',
            clientname: 'bareos-fd',
            jobstatus: 'T',
            starttime: '2026-03-23 08:00:01',
            realendtime: '2026-03-23 08:12:44',
            duration: '0:12:43',
            jobfiles: '42',
            jobbytes: '2048',
            joberrors: '0',
          }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list jobs jobstatus=R'),
        data: {
          jobs: [{
            jobid: '8',
            name: 'BackupClient2',
            clientname: 'db-fd',
            jobstatus: 'R',
            starttime: '2026-03-23 09:00:00',
            jobfiles: '10',
            jobbytes: '1024',
          }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('llist jobs last'),
        data: {
          jobs: [{
            jobid: '8',
            name: 'BackupClient2',
            clientname: 'db-fd',
            jobstatus: 'R',
            starttime: '2026-03-23 09:00:00',
            jobbytes: '1024',
          }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list jobtotals'),
        data: {
          jobtotals: {
            jobs: '12',
            files: '1200',
            bytes: '40960',
          },
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list clients'),
        data: {
          clients: [{ name: 'bareos-fd' }, { name: 'db-fd' }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list storages'),
        data: {
          storages: [{ name: 'File' }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('status director'),
        data: {
          running: [{
            jobid: '8',
            name: 'BackupClient2',
            start_time: '2026-03-23 09:00:00',
            files: '11',
            bytes: '2048',
            status: 'Is waiting for a mount request',
          }],
        },
      }),
    })

    await expect(snapshotPromise).resolves.toMatchObject({
      director: 'prod-dir',
      jobsPast24h: [
        expect.objectContaining({
          director: 'prod-dir',
          id: 7,
          scopeKey: 'prod-dir:7',
        }),
      ],
      runningJobs: [
        expect.objectContaining({
          director: 'prod-dir',
          id: 8,
          files: 11,
          bytes: 2048,
          runtimeStatus: 'Is waiting for a mount request',
        }),
      ],
      recentJobs: [
        expect.objectContaining({
          director: 'prod-dir',
          id: 8,
          runtimeStatus: 'Is waiting for a mount request',
        }),
      ],
      clientCount: 2,
      storageCount: 1,
      jobTotals: {
        jobs: 12,
        files: 1200,
        bytes: 40960,
      },
    })
  })

  it('falls back to list jobs when restricted ACLs reject llist jobs', async () => {
    vi.setSystemTime(new Date('2026-03-24T10:00:00'))

    const snapshotPromise = fetchDirectorDashboardSnapshot({
      username: 'operator',
      password: 'secret',
      director: 'prod-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'tls',
      }),
    })

    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(8)
    })

    let commandIds = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'error',
        id: commandIds.get('llist jobs days=1'),
        message: 'ACL forbids llist jobs days=1',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list jobs jobstatus=R'),
        data: {
          jobs: [],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'error',
        id: commandIds.get('llist jobs last'),
        message: 'ACL forbids llist jobs last',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list jobtotals'),
        data: {
          jobtotals: {
            jobs: '10',
            files: '100',
            bytes: '1024',
          },
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list clients'),
        data: {
          clients: [{ name: 'bareos-fd' }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list storages'),
        data: {
          storages: [{ name: 'File' }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('status director'),
        data: {
          running: [],
        },
      }),
    })

    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(9)
    })

    commandIds = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('list jobs'),
        data: {
          jobs: [
            {
              jobid: '1',
              name: 'backup-bareos-fd',
              clientname: 'bareos-fd',
              jobstatus: 'T',
              starttime: '2026-03-24 09:00:00',
              duration: '0:00:01',
              jobfiles: '86',
              jobbytes: '145488',
            },
            {
              jobid: '2',
              name: 'backup-bareos-fd',
              clientname: 'bareos-fd',
              jobstatus: 'T',
              starttime: '2026-03-24 08:00:00',
              duration: '0:00:00',
              jobfiles: '0',
              jobbytes: '0',
            },
            {
              jobid: '3',
              name: 'RestoreFiles',
              clientname: 'bareos-fd',
              jobstatus: 'f',
              starttime: '2026-03-24 07:00:00',
              duration: '0:00:00',
              jobfiles: '0',
              jobbytes: '0',
            },
            {
              jobid: '4',
              name: 'older-job',
              clientname: 'bareos-fd',
              jobstatus: 'T',
              starttime: '2026-03-22 07:00:00',
              duration: '0:00:00',
              jobfiles: '0',
              jobbytes: '0',
            },
          ],
        },
      }),
    })

    await expect(snapshotPromise).resolves.toMatchObject({
      director: 'prod-dir',
      jobsPast24h: [
        expect.objectContaining({ id: 1, scopeKey: 'prod-dir:1' }),
        expect.objectContaining({ id: 2, scopeKey: 'prod-dir:2' }),
        expect.objectContaining({ id: 3, scopeKey: 'prod-dir:3' }),
      ],
      recentJobs: [
        expect.objectContaining({ id: 1, scopeKey: 'prod-dir:1' }),
        expect.objectContaining({ id: 3, scopeKey: 'prod-dir:3' }),
        expect.objectContaining({ id: 4, scopeKey: 'prod-dir:4' }),
      ],
      jobTotals: {
        jobs: 10,
        files: 100,
        bytes: 1024,
      },
    })
  })

  it('merges multiple director snapshots into one dashboard view', () => {
    const aggregate = aggregateDirectorDashboardSnapshots([
      {
        director: 'prod-a',
        jobsPast24h: [{ scopeKey: 'prod-a:1', director: 'prod-a', id: 1, status: 'T', starttime: '2026-03-23 08:00:00' }],
        runningJobs: [{ scopeKey: 'prod-a:2', director: 'prod-a', id: 2, status: 'R', starttime: '2026-03-23 09:00:00' }],
        recentJobs: [{ scopeKey: 'prod-a:3', director: 'prod-a', id: 3, status: 'W', starttime: '2026-03-23 10:00:00' }],
        clientCount: 2,
        storageCount: 1,
        jobTotals: { jobs: 10, files: 100, bytes: 1000 },
      },
      {
        director: 'prod-b',
        jobsPast24h: [{ scopeKey: 'prod-b:4', director: 'prod-b', id: 4, status: 'f', starttime: '2026-03-23 11:00:00' }],
        runningJobs: [],
        recentJobs: [{ scopeKey: 'prod-b:5', director: 'prod-b', id: 5, status: 'T', starttime: '2026-03-23 12:00:00' }],
        clientCount: 3,
        storageCount: 2,
        jobTotals: { jobs: 20, files: 200, bytes: 2000 },
      },
    ])

    expect(aggregate.jobsPast24h.map(job => job.scopeKey)).toEqual([
      'prod-b:4',
      'prod-a:1',
    ])
    expect(aggregate.recentJobs.map(job => job.scopeKey)).toEqual([
      'prod-b:5',
      'prod-a:3',
    ])
    expect(aggregate.clientCount).toBe(5)
    expect(aggregate.storageCount).toBe(3)
    expect(aggregate.jobTotals).toEqual({
      jobs: 30,
      files: 300,
      bytes: 3000,
    })
  })
})
