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
    setActivePinia(createPinia())
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
      type: 'session',
      mode: 'json',
      director: 'prod-dir',
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'tls',
      }),
    })
    await vi.waitFor(() => {
      expect(socket.sent).toHaveLength(13)
    })

    const commandIds = new Map(
      socket.sent.slice(1).map((payload) => {
        const command = JSON.parse(payload)
        return [command.command, command.id]
      })
    )

    expect(commandIds.has('llist jobs days=1')).toBe(false)
    for (const [status, count] of [
      ['R', '1'],
      ['C', '2'],
      ['T', '7'],
      ['W', '1'],
      ['f', '1'],
    ]) {
      socket.onmessage?.({
        data: JSON.stringify({
          type: 'response',
          id: commandIds.get(`list jobs count days=1 jobstatus=${status}`),
          data: { jobs: [{ count }] },
        }),
      })
    }
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
            client: 'db-fd',
            start_time: '2026-03-23 09:00:00',
            files: '11',
            bytes: '2048',
            status: 'Is waiting for a mount request',
          }],
        },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('status database'),
        data: {
          database_status: {
            status: 'ok',
            checked_at: '2026-03-23 09:30:00',
            database: {
              engine: 'postgresql',
              name: 'bareos',
              total_bytes_available: true,
              total_bytes: 4096,
            },
            tables_available: true,
            tables: [
              { name: 'public.job', bytes: 2048 },
              { name: 'public.file', bytes: 1024 },
            ],
          },
        },
      }),
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('llist pools'),
        data: { pools: [{ name: 'Default', poolid: '1', numvols: '2' }] },
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandIds.get('llist volumes'),
        data: { volumes: [
          { volumename: 'Vol-0001', pool: 'Default', volbytes: '1024' },
          { volumename: 'Vol-0002', pool: 'Default', volbytes: '2048' },
        ]},
      }),
    })

    await expect(snapshotPromise).resolves.toMatchObject({
      director: 'prod-dir',
      jobsPast24hStatusCounts: {
        R: 1,
        C: 2,
        T: 7,
        W: 1,
        f: 1,
      },
      runningJobs: [
        expect.objectContaining({
          director: 'prod-dir',
          id: 8,
          client: 'db-fd',
          files: 11,
          bytes: 2048,
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
      databaseStatus: {
        status: 'ok',
        checkedAt: '2026-03-23 09:30:00',
        database: {
          engine: 'postgresql',
          name: 'bareos',
          totalBytesAvailable: true,
          totalBytes: 4096,
        },
        tablesAvailable: true,
      },
    })
  })

  it('merges multiple director snapshots into one dashboard view', () => {
    const aggregate = aggregateDirectorDashboardSnapshots([
      {
        director: 'prod-a',
        jobsPast24hStatusCounts: { R: 1, C: 2, T: 3, W: 4, f: 5 },
        runningJobs: [{ scopeKey: 'prod-a:2', director: 'prod-a', id: 2, status: 'R', starttime: '2026-03-23 09:00:00' }],
        databaseStatus: {
          director: 'prod-a',
          status: 'warning',
          checkedAt: '2026-03-23 10:00:00',
          database: { totalBytesAvailable: true, totalBytes: 1000 },
        },
        clientCount: 2,
        storageCount: 1,
        jobTotals: { jobs: 10, files: 100, bytes: 1000 },
      },
      {
        director: 'prod-b',
        jobsPast24hStatusCounts: { R: 10, C: 20, T: 30, W: 40, f: 50 },
        runningJobs: [],
        databaseStatus: {
          director: 'prod-b',
          status: 'ok',
          checkedAt: '2026-03-23 12:00:00',
          database: { totalBytesAvailable: true, totalBytes: 2000 },
        },
        clientCount: 3,
        storageCount: 2,
        jobTotals: { jobs: 20, files: 200, bytes: 2000 },
      },
    ])

    expect(aggregate.jobsPast24hStatusCounts).toEqual({
      R: 11,
      C: 22,
      T: 33,
      W: 44,
      f: 55,
    })
    expect(aggregate.clientCount).toBe(5)
    expect(aggregate.storageCount).toBe(3)
    expect(aggregate.jobTotals).toEqual({
      jobs: 30,
      files: 300,
      bytes: 3000,
    })
    expect(aggregate.databaseStatuses.map(s => s.director)).toEqual(['prod-a', 'prod-b'])
    expect(aggregate.databaseStatusSummary).toEqual({
      status: 'warning',
      checkedAt: '2026-03-23 12:00:00',
      totalBytes: 3000,
      directors: 2,
      availableDirectors: 2,
      unavailableDirectors: 0,
      warningDirectors: 1,
      errorDirectors: 0,
    })
  })
})
