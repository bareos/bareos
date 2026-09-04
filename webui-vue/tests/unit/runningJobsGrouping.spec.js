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

import { describe, it, expect } from 'vitest'
import {
  isWaitingStatus,
  jobStatusBucket,
  jobClientBucket,
  groupRunningJobsBy,
  groupRunningJobsByStatus,
  groupRunningJobsByClient,
  elapsedRunningSecs,
} from '../../src/utils/runningJobsGrouping.js'

function job(overrides = {}) {
  return { id: 1, client: 'client-a', runtimeStatus: 'is running', ...overrides }
}

describe('isWaitingStatus', () => {
  it('detects waiting statuses case-insensitively', () => {
    expect(isWaitingStatus('is waiting on max Storage jobs')).toBe(true)
    expect(isWaitingStatus('Is Waiting execution')).toBe(true)
  })

  it('returns false for running/other statuses and non-strings', () => {
    expect(isWaitingStatus('is running')).toBe(false)
    expect(isWaitingStatus(undefined)).toBe(false)
    expect(isWaitingStatus(null)).toBe(false)
    expect(isWaitingStatus(42)).toBe(false)
  })
})

describe('jobStatusBucket / jobClientBucket', () => {
  it('falls back to Unknown for missing status/client', () => {
    expect(jobStatusBucket({})).toBe('Unknown')
    expect(jobStatusBucket({ runtimeStatus: '' })).toBe('Unknown')
    expect(jobClientBucket({})).toBe('Unknown')
    expect(jobClientBucket({ client: '' })).toBe('Unknown')
  })

  it('prefers runtimeStatus over status', () => {
    expect(jobStatusBucket({ runtimeStatus: 'is running', status: 'T' })).toBe('is running')
    expect(jobStatusBucket({ status: 'T' })).toBe('T')
  })

  it('reads client verbatim', () => {
    expect(jobClientBucket({ client: 'bareos-fd' })).toBe('bareos-fd')
  })
})

describe('groupRunningJobsBy', () => {
  it('groups and counts by key, sorted by descending count', () => {
    const jobs = [
      job({ id: 1, client: 'a' }),
      job({ id: 2, client: 'b' }),
      job({ id: 3, client: 'a' }),
      job({ id: 4, client: 'a' }),
    ]
    const groups = groupRunningJobsBy(jobs, jobClientBucket)
    expect(groups).toEqual([
      { key: 'a', count: 3, jobs: [jobs[0], jobs[2], jobs[3]] },
      { key: 'b', count: 1, jobs: [jobs[1]] },
    ])
  })

  it('breaks ties by key ascending for stable ordering', () => {
    const jobs = [job({ client: 'z' }), job({ client: 'a' })]
    const groups = groupRunningJobsBy(jobs, jobClientBucket)
    expect(groups.map(g => g.key)).toEqual(['a', 'z'])
  })

  it('handles an empty/undefined input', () => {
    expect(groupRunningJobsBy(undefined, jobClientBucket)).toEqual([])
    expect(groupRunningJobsBy([], jobClientBucket)).toEqual([])
  })
})

describe('groupRunningJobsByStatus', () => {
  it('groups by wait-reason/status text', () => {
    const jobs = [
      job({ id: 1, runtimeStatus: 'is running' }),
      job({ id: 2, runtimeStatus: 'is waiting on max Storage jobs' }),
      job({ id: 3, runtimeStatus: 'is waiting on max Storage jobs' }),
    ]
    const groups = groupRunningJobsByStatus(jobs)
    expect(groups).toEqual([
      { key: 'is waiting on max Storage jobs', count: 2, jobs: [jobs[1], jobs[2]] },
      { key: 'is running', count: 1, jobs: [jobs[0]] },
    ])
  })
})

describe('groupRunningJobsByClient', () => {
  it('returns all clients unchanged when under the topN threshold', () => {
    const jobs = [job({ client: 'a' }), job({ client: 'b' })]
    const groups = groupRunningJobsByClient(jobs, { topN: 10 })
    expect(groups.map(g => g.key)).toEqual(['a', 'b'])
  })

  it('folds clients beyond topN into a single Other bucket', () => {
    const jobs = [
      ...Array.from({ length: 5 }, (_, i) => job({ id: i, client: `c${i}` })),
      job({ id: 100, client: 'c0' }), // c0 now has 2, making it the top client
    ]
    const groups = groupRunningJobsByClient(jobs, { topN: 1 })
    expect(groups[0]).toEqual({ key: 'c0', count: 2, jobs: [jobs[0], jobs[5]] })
    expect(groups[1].key).toBe('Other')
    expect(groups[1].count).toBe(4)
  })
})

describe('elapsedRunningSecs', () => {
  // "now" corresponds to 03-Sep-26 16:05:30 local time.
  const now = new Date(2026, 8, 3, 16, 5, 30).getTime()

  it('parses the director dd-Mon-yy HH:MM start_time format', () => {
    const secs = elapsedRunningSecs(job({ starttime: '03-Sep-26 16:02' }), now)
    // 16:02 -> 16:05:30 is 3.5 minutes = 210s.
    expect(secs).toBe(210)
  })

  it('returns 0 for a missing starttime', () => {
    expect(elapsedRunningSecs(job({ starttime: undefined }), now)).toBe(0)
  })

  it('returns 0 for an unparseable starttime', () => {
    expect(elapsedRunningSecs(job({ starttime: 'not-a-date' }), now)).toBe(0)
  })

  it('never returns a negative value for a start time after "now"', () => {
    const secs = elapsedRunningSecs(job({ starttime: '03-Sep-26 16:10' }), now)
    expect(secs).toBe(0)
  })

  it('defaults "now" to the current time when not provided', () => {
    const secs = elapsedRunningSecs(job({ starttime: '03-Sep-26 16:02' }))
    expect(secs).toBeGreaterThanOrEqual(0)
  })
})
