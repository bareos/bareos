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

import { beforeEach, describe, expect, it, vi } from 'vitest'

const createDirectorCommandClient = vi.fn()

vi.mock('../../src/composables/directorAggregate.js', () => ({
  createDirectorCommandClient: (...args) => createDirectorCommandClient(...args),
}))

const { fetchCombinedTimelineData } = await import(
  '../../src/composables/combinedTimeline.js'
)

const OPTIONS = { jobDays: 7, schedulerRange: { from: 1, to: 3 } }

function makeClient(responses) {
  return {
    call: vi.fn(async (command) => {
      const match = Object.entries(responses).find(([prefix]) =>
        command.startsWith(prefix)
      )
      return match ? match[1] : {}
    }),
    disconnect: vi.fn(),
  }
}

describe('fetchCombinedTimelineData', () => {
  beforeEach(() => {
    createDirectorCommandClient.mockReset()
  })

  it('returns empty results without directors', async () => {
    const result = await fetchCombinedTimelineData({}, [], OPTIONS)

    expect(result).toEqual({ actualJobs: [], scheduledRuns: [], errors: [] })
    expect(createDirectorCommandClient).not.toHaveBeenCalled()
  })

  it('requests jobs, status and scheduler preview for each director', async () => {
    const client = makeClient({})
    createDirectorCommandClient.mockResolvedValue(client)

    await fetchCombinedTimelineData({ username: 'u' }, ['dir-a'], OPTIONS)

    expect(createDirectorCommandClient).toHaveBeenCalledWith({
      username: 'u',
      director: 'dir-a',
    })
    expect(client.call.mock.calls.map(([command]) => command)).toEqual([
      'llist jobs days=7',
      'status director',
      'status scheduler days=1,3',
    ])
    expect(client.disconnect).toHaveBeenCalledTimes(1)
  })

  it('tags jobs and scheduled runs with their director', async () => {
    createDirectorCommandClient.mockResolvedValue(
      makeClient({
        'llist jobs': { jobs: [{ jobid: '5', name: 'backup', jobstatus: 'T' }] },
        'status scheduler': {
          preview: [{ schedule: 'nightly', job: 'backup', runtime: 't1' }],
        },
      })
    )

    const result = await fetchCombinedTimelineData({}, ['dir-a'], OPTIONS)

    expect(result.actualJobs).toHaveLength(1)
    expect(result.actualJobs[0]).toMatchObject({
      id: 5,
      name: 'backup',
      status: 'T',
      director: 'dir-a',
    })
    expect(result.scheduledRuns).toEqual([
      expect.objectContaining({
        director: 'dir-a',
        scheduleKey: 'dir-a:nightly',
        scheduleDisplay: 'nightly',
        scopeKey: 'dir-a:preview:nightly:backup:t1',
      }),
    ])
    expect(result.errors).toEqual([])
  })

  it('overlays runtime status only when a job is running', async () => {
    createDirectorCommandClient.mockResolvedValue(
      makeClient({
        'llist jobs': {
          jobs: [
            { jobid: '1', name: 'a', jobstatus: 'R' },
            { jobid: '2', name: 'b', jobstatus: 'T' },
          ],
        },
        'status director': {
          running: [{ jobid: '1', status: 'is waiting for a mount request' }],
        },
      })
    )

    const result = await fetchCombinedTimelineData({}, ['dir-a'], OPTIONS)

    expect(result.actualJobs[0].runtimeStatus).toBe(
      'is waiting for a mount request'
    )
    expect(result.actualJobs[1].runtimeStatus).toBeUndefined()
  })

  it('keeps a missing scheduler preview as no scheduled runs', async () => {
    createDirectorCommandClient.mockResolvedValue(
      makeClient({ 'status scheduler': { preview: 'not-an-array' } })
    )

    const result = await fetchCombinedTimelineData({}, ['dir-a'], OPTIONS)

    expect(result.scheduledRuns).toEqual([])
  })

  it('merges directors and reports the failing one', async () => {
    createDirectorCommandClient.mockImplementation(async ({ director }) => {
      if (director === 'dir-b') {
        throw new Error('login failed')
      }
      return makeClient({
        'llist jobs': { jobs: [{ jobid: '9', name: 'x', jobstatus: 'T' }] },
      })
    })

    const result = await fetchCombinedTimelineData(
      {},
      ['dir-a', 'dir-b'],
      OPTIONS
    )

    expect(result.actualJobs.map(job => job.director)).toEqual(['dir-a'])
    expect(result.errors).toEqual(['login failed'])
  })

  it('throws when every director fails', async () => {
    createDirectorCommandClient.mockRejectedValue(new Error('no route'))

    await expect(
      fetchCombinedTimelineData({}, ['dir-a', 'dir-b'], OPTIONS)
    ).rejects.toThrow('no route')
  })

  it('disconnects the client even if a command fails', async () => {
    const client = makeClient({})
    client.call.mockRejectedValue(new Error('boom'))
    createDirectorCommandClient.mockResolvedValue(client)

    await expect(
      fetchCombinedTimelineData({}, ['dir-a'], OPTIONS)
    ).rejects.toThrow('boom')
    expect(client.disconnect).toHaveBeenCalledTimes(1)
  })
})
