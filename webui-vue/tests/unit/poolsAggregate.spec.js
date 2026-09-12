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
import { beforeEach, afterEach, describe, expect, it, vi } from 'vitest'
import { fetchAggregatedPools } from '../../src/composables/poolsAggregate.js'

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

  send(payload) { this.sent.push(payload) }
  open() {
    this.readyState = FakeWebSocket.OPEN
    this.onopen?.()
  }
  close() {
    this.readyState = FakeWebSocket.CLOSED
    this.onclose?.()
  }
}

/** Extract the command→id map from a socket's sent frames (skipping the auth frame). */
function commandMap(socket) {
  return new Map(
    socket.sent.slice(1).map(payload => {
      const { command, id } = JSON.parse(payload)
      return [command, id]
    })
  )
}

describe('poolsAggregate', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('aggregates pool bytes and volume counts across two directors', async () => {
    const loading = fetchAggregatedPools(
      { username: 'admin', password: 'secret' },
      ['dir-a', 'dir-b']
    )

    const [sockA, sockB] = FakeWebSocket.instances
    sockA.open()
    sockB.open()
    sockA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    sockB.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })

    // Wait for both sockets to send their commands (auth + llist pools + llist volumes).
    await vi.waitFor(() => {
      expect(sockA.sent).toHaveLength(3)
      expect(sockB.sent).toHaveLength(3)
    })

    const cmdsA = commandMap(sockA)
    const cmdsB = commandMap(sockB)

    // Director A: one pool with two volumes.
    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmdsA.get('llist pools'),
        data: { pools: [{ name: 'Full', numvols: '2' }] },
      }),
    })
    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmdsA.get('llist volumes'),
        data: {
          volumes: [
            { volumename: 'Vol-A1', pool: 'Full', volbytes: '100' },
            { volumename: 'Vol-A2', pool: 'Full', volbytes: '200' },
          ],
        },
      }),
    })

    // Director B: same pool name but different volumes.
    sockB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmdsB.get('llist pools'),
        data: { pools: [{ name: 'Full', numvols: '1' }, { name: 'Incremental', numvols: '3' }] },
      }),
    })
    sockB.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmdsB.get('llist volumes'),
        data: {
          volumes: [
            { volumename: 'Vol-B1', pool: 'Full',        volbytes: '50' },
            { volumename: 'Vol-B2', pool: 'Incremental', volbytes: '10' },
            { volumename: 'Vol-B3', pool: 'Incremental', volbytes: '20' },
            { volumename: 'Vol-B4', pool: 'Incremental', volbytes: '30' },
          ],
        },
      }),
    })

    const result = await loading

    expect(result.directorErrors).toHaveLength(0)

    // Two pools from dir-a:Full and dir-b:Full and dir-b:Incremental
    expect(result.pools).toHaveLength(3)

    const fullA = result.pools.find(p => p.scopeKey === 'dir-a:Full')
    expect(fullA).toBeDefined()
    expect(fullA.totalbytes).toBe(300)     // 100 + 200
    expect(fullA.totalvolumes).toBe(2)

    const fullB = result.pools.find(p => p.scopeKey === 'dir-b:Full')
    expect(fullB).toBeDefined()
    expect(fullB.totalbytes).toBe(50)
    expect(fullB.totalvolumes).toBe(1)

    const incremental = result.pools.find(p => p.scopeKey === 'dir-b:Incremental')
    expect(incremental).toBeDefined()
    expect(incremental.totalbytes).toBe(60)   // 10 + 20 + 30
    expect(incremental.totalvolumes).toBe(3)
  })

  it('returns zero bytes and volumes for a pool with no volumes', async () => {
    const loading = fetchAggregatedPools(
      { username: 'admin', password: 'secret' },
      ['dir-a']
    )

    const [sockA] = FakeWebSocket.instances
    sockA.open()
    sockA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })

    await vi.waitFor(() => expect(sockA.sent).toHaveLength(3))
    const cmds = commandMap(sockA)

    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist pools'),
        data: { pools: [{ name: 'Empty', numvols: '0' }] },
      }),
    })
    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist volumes'),
        data: { volumes: [] },
      }),
    })

    const result = await loading

    expect(result.pools).toHaveLength(1)
    expect(result.pools[0].totalbytes).toBe(0)
    expect(result.pools[0].totalvolumes).toBe(0)
  })

  it('returns a directorError and partial results when one director fails', async () => {
    const loading = fetchAggregatedPools(
      { username: 'admin', password: 'secret' },
      ['dir-ok', 'dir-fail']
    )

    const [sockOk, sockFail] = FakeWebSocket.instances
    sockOk.open()
    sockFail.open()
    sockOk.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
    // Simulate auth failure on dir-fail.
    sockFail.onmessage?.({ data: JSON.stringify({ type: 'auth_error', message: 'bad password' }) })

    await vi.waitFor(() => expect(sockOk.sent).toHaveLength(3))
    const cmds = commandMap(sockOk)

    sockOk.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist pools'),
        data: { pools: [{ name: 'Full', numvols: '1' }] },
      }),
    })
    sockOk.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist volumes'),
        data: { volumes: [{ volumename: 'V1', pool: 'Full', volbytes: '42' }] },
      }),
    })

    const result = await loading

    expect(result.pools).toHaveLength(1)
    expect(result.pools[0].totalbytes).toBe(42)
    expect(result.directorErrors).toHaveLength(1)
    expect(result.directorErrors[0].director).toBe('dir-fail')
  })

  it('exposes a sorted poolNames list of unique names', async () => {
    const loading = fetchAggregatedPools(
      { username: 'admin', password: 'secret' },
      ['dir-a']
    )

    const [sockA] = FakeWebSocket.instances
    sockA.open()
    sockA.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })

    await vi.waitFor(() => expect(sockA.sent).toHaveLength(3))
    const cmds = commandMap(sockA)

    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist pools'),
        data: {
          pools: [
            { name: 'Zebra', numvols: '1' },
            { name: 'Alpha', numvols: '1' },
            { name: 'Midway', numvols: '1' },
          ],
        },
      }),
    })
    sockA.onmessage?.({
      data: JSON.stringify({
        type: 'response',
        id: cmds.get('llist volumes'),
        data: { volumes: [] },
      }),
    })

    const result = await loading
    expect(result.poolNames).toEqual(['Alpha', 'Midway', 'Zebra'])
  })
})
