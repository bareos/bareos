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

import { describe, expect, it } from 'vitest'
import { buildDailyRunSummary, buildTimelineGroups, distinctJobNames } from '../../src/utils/jobTimeline.js'

describe('job timeline helpers', () => {
  it('groups multi-director timelines by director without prefixing each client label', () => {
    const groups = buildTimelineGroups([
      {
        id: 1,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        director: 'prod-a',
        starttime: '2026-05-27 10:00:00',
        endtime: '2026-05-27 10:01:00',
      },
      {
        id: 2,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        director: 'prod-b',
        starttime: '2026-05-27 11:00:00',
        endtime: '2026-05-27 11:01:00',
      },
    ], {
      start: Date.parse('2026-05-27T00:00:00'),
      now: Date.parse('2026-05-27T23:59:59'),
      multiDirectorTimeline: true,
    })

    expect(groups).toHaveLength(2)
    expect(groups.map(group => group.director)).toEqual(['prod-a', 'prod-b'])
    expect(groups[0].clientSpans).toEqual([
      { client: 'bareos-fd', director: 'prod-a', label: 'bareos-fd', startRow: 0, rowCount: 1 },
    ])
    expect(groups[1].clientSpans).toEqual([
      { client: 'bareos-fd', director: 'prod-b', label: 'bareos-fd', startRow: 0, rowCount: 1 },
    ])
  })

  it('keeps single-director timelines as one grouped section and sorts rows', () => {
    const groups = buildTimelineGroups([
      {
        id: 2,
        name: 'ZBackup',
        client: 'client-b',
        director: 'prod-a',
        starttime: '2026-05-27 11:00:00',
        endtime: '2026-05-27 11:01:00',
      },
      {
        id: 1,
        name: 'ABackup',
        client: 'client-a',
        director: 'prod-a',
        starttime: '2026-05-27 10:00:00',
        endtime: '2026-05-27 10:01:00',
      },
    ], {
      start: Date.parse('2026-05-27T00:00:00'),
      now: Date.parse('2026-05-27T23:59:59'),
      multiDirectorTimeline: false,
    })

    expect(groups).toHaveLength(1)
    expect(groups[0].rows.map(row => `${row.client}:${row.name}`)).toEqual([
      'client-a:ABackup',
      'client-b:ZBackup',
    ])
    expect(groups[0].clientSpans).toEqual([
      { client: 'client-a', director: 'prod-a', label: 'client-a', startRow: 0, rowCount: 1 },
      { client: 'client-b', director: 'prod-a', label: 'client-b', startRow: 1, rowCount: 1 },
    ])
  })
})

describe('buildDailyRunSummary', () => {
  const jobs = [
    { id: 1, name: 'BackupA', client: 'c1', status: 'T', starttime: '2026-05-27 10:00:00' },
    { id: 2, name: 'BackupB', client: 'c1', status: 'f', starttime: '2026-05-27 11:00:00' },
    { id: 3, name: 'BackupC', client: 'c2', status: 'T', starttime: '2026-05-27 12:00:00' },
    { id: 4, name: 'BackupD', client: 'c2', status: 'T', starttime: '2026-05-28 01:00:00' },
  ]

  it('buckets jobs by their local start date and counts statuses worst-first', () => {
    const summary = buildDailyRunSummary(jobs, '2026-05-27')
    expect(summary.total).toBe(3)
    expect(summary.statuses).toEqual([
      { status: 'f', count: 1 },
      { status: 'T', count: 2 },
    ])
  })

  it('ignores jobs on other days', () => {
    const summary = buildDailyRunSummary(jobs, '2026-05-28')
    expect(summary.total).toBe(1)
    expect(summary.statuses).toEqual([{ status: 'T', count: 1 }])
  })

  it('honours a visibleJobNames filter', () => {
    const summary = buildDailyRunSummary(jobs, '2026-05-27', {
      visibleJobNames: new Set(['BackupA', 'BackupC']),
    })
    expect(summary.total).toBe(2)
    expect(summary.statuses).toEqual([{ status: 'T', count: 2 }])
  })

  it('returns an empty summary when nothing ran that day', () => {
    expect(buildDailyRunSummary(jobs, '2026-06-01')).toEqual({ total: 0, statuses: [] })
  })
})

describe('distinctJobNames', () => {
  it('returns sorted, de-duplicated job names', () => {
    const jobs = [
      { name: 'ZJob' }, { name: 'AJob' }, { name: 'ZJob' }, { name: '' }, { name: null },
    ]
    expect(distinctJobNames(jobs)).toEqual(['AJob', 'ZJob'])
  })

  it('returns an empty array for no jobs', () => {
    expect(distinctJobNames([])).toEqual([])
    expect(distinctJobNames(null)).toEqual([])
  })
})
