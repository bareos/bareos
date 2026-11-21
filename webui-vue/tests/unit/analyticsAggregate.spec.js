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
import { fetchAggregatedAnalytics } from '../../src/composables/analyticsAggregate.js'

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
    setActivePinia(createPinia())
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
    await vi.waitFor(() => {
      expect(socketA.sent).toHaveLength(2)
      expect(socketB.sent).toHaveLength(2)
    })

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
        id: commandsA.get('list jobs limit=1000 offset=0 sortby=jobid'),
        data: {
          jobs: [{ jobid: 10, name: 'BackupCatalog', client: 'fd-a', jobbytes: 11, jobfiles: 3 }],
          meta: { range: { filtered: 0 } },
        },
      }),
    })
    socketB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: commandsB.get('list jobs limit=1000 offset=0 sortby=jobid'),
        data: {
          jobs: [{ jobid: 20, name: 'BackupCatalog', client: 'fd-a', jobbytes: 22, jobfiles: 6 }],
          meta: { range: { filtered: 0 } },
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

  it('includes jobs beyond one page for each director', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      2
    )
    const socket = FakeWebSocket.instances[0]
    const jobs = [
      { jobid: 1, name: 'Backup', jobbytes: 10 },
      { jobid: 2, name: 'Backup', jobbytes: 20 },
      { jobid: 3, name: 'Backup', jobbytes: 30 },
    ]
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      const offset = Number(request.command.match(/offset=(\d+)/)?.[1] ?? 0)
      const limit = Number(request.command.match(/limit=(\d+)/)?.[1] ?? 0)
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: request.id,
          data: { jobs: jobs.slice(offset, offset + limit), meta: { range: { filtered: 0 } } },
        }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.directorErrors).toEqual([])
    expect(result.jobs.map(job => job.id)).toEqual([3, 2, 1])
    expect(result.jobs.reduce((sum, job) => sum + job.bytes, 0)).toBe(60)
  })

  it('reports a later page failure instead of presenting partial totals', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      1
    )
    const socket = FakeWebSocket.instances[0]
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify(request.command.includes('limit=1 offset=0')
          ? {
              type: 'response', id: request.id,
              data: { jobs: [{ jobid: 1, name: 'Backup' }], meta: { range: { filtered: 0 } } },
            }
          : { type: 'error', id: request.id, message: 'Page unavailable' }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.jobs).toEqual([])
    expect(result.directorErrors).toEqual([
      { director: 'prod-a', message: 'Page unavailable' },
    ])
  })

  it('uses a single snapshot when Director ACLs filter a page', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' },
      ['prod-a'],
      2
    )
    const socket = FakeWebSocket.instances[0]
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      const data = request.command === 'list jobs sortby=jobid'
        ? { jobs: [
            { jobid: 3, name: 'Allowed', jobbytes: 30 },
            { jobid: 5, name: 'Allowed', jobbytes: 50 },
          ] }
        : { jobs: [], meta: { range: { filtered: 7 } } }
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify({ type: 'response', id: request.id, data }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.directorErrors).toEqual([])
    expect(result.jobs.map(job => job.id)).toEqual([5, 3])
  })

  it('restarts when an already-read row is deleted between pages', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' }, ['prod-a'], 2
    )
    const socket = FakeWebSocket.instances[0]
    const jobs = [1, 2, 3, 4, 5].map(jobid => ({ jobid, name: 'Backup' }))
    let requests = 0
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      const offset = Number(request.command.match(/offset=(\d+)/)[1])
      const limit = Number(request.command.match(/limit=(\d+)/)[1])
      const page = jobs.slice(offset, offset + limit)
      if (requests++ === 0) jobs.shift()
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: request.id,
          data: { jobs: page, meta: { range: { filtered: 0 } } },
        }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.directorErrors).toEqual([])
    expect(result.jobs.map(job => job.id)).toEqual([5, 4, 3, 2])
    expect(requests).toBeGreaterThan(2)
  })

  it('reports an error if the pagination anchor keeps moving', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' }, ['prod-a'], 1
    )
    const socket = FakeWebSocket.instances[0]
    let firstPage = true
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      const page = firstPage
        ? [{ jobid: 1, name: 'Backup' }]
        : [{ jobid: 2, name: 'Backup' }]
      firstPage = !firstPage
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: request.id,
          data: { jobs: page, meta: { range: { filtered: 0 } } },
        }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.jobs).toEqual([])
    expect(result.directorErrors).toEqual([
      { director: 'prod-a', message: 'Analytics jobs changed during pagination' },
    ])
  })

  it('reports missing pagination metadata instead of incomplete totals', async () => {
    const loading = fetchAggregatedAnalytics(
      { username: 'admin', password: 'secret' }, ['prod-a'], 2
    )
    const socket = FakeWebSocket.instances[0]
    socket.send = (payload) => {
      const request = JSON.parse(payload)
      if (request.type !== 'command') return
      queueMicrotask(() => socket.onmessage?.({
        data: JSON.stringify({
          type: 'response', id: request.id, data: { jobs: [] },
        }),
      }))
    }
    socket.open()
    socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    const result = await loading
    expect(result.jobs).toEqual([])
    expect(result.directorErrors).toEqual([
      { director: 'prod-a', message: 'Invalid analytics pagination metadata' },
    ])
  })

  it('rejects invalid page sizes', async () => {
    await expect(fetchAggregatedAnalytics({}, [], 0)).rejects.toThrow(RangeError)
  })
})
