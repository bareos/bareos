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
import { buildTimelineGroups } from '../../src/utils/jobTimeline.js'

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
