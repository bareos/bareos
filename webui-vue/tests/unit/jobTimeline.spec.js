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
import { buildTimelineLanes, distinctFilesetClientOptions } from '../../src/utils/jobTimeline.js'

describe('buildTimelineLanes', () => {
  it('groups multi-director timelines by director without prefixing each lane label', () => {
    const groups = buildTimelineLanes([
      {
        id: 1,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        director: 'prod-a',
        starttime: '2026-05-27 10:00:00',
        endtime: '2026-05-27 10:01:00',
      },
      {
        id: 2,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        fileset: 'SelfTest',
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
    expect(groups[0].lanes).toEqual([
      expect.objectContaining({ client: 'bareos-fd', fileset: 'SelfTest', director: 'prod-a' }),
    ])
    expect(groups[1].lanes).toEqual([
      expect.objectContaining({ client: 'bareos-fd', fileset: 'SelfTest', director: 'prod-b' }),
    ])
  })

  it('keeps single-director timelines as one grouped section, sorted by client then fileset', () => {
    const groups = buildTimelineLanes([
      {
        id: 2,
        name: 'ZBackup',
        client: 'client-b',
        fileset: 'FilesetZ',
        director: 'prod-a',
        starttime: '2026-05-27 11:00:00',
        endtime: '2026-05-27 11:01:00',
      },
      {
        id: 1,
        name: 'ABackup',
        client: 'client-a',
        fileset: 'FilesetA',
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
    expect(groups[0].lanes.map(lane => `${lane.client}:${lane.fileset}`)).toEqual([
      'client-a:FilesetA',
      'client-b:FilesetZ',
    ])
  })

  it('merges different job names sharing a fileset+client into a single lane', () => {
    const groups = buildTimelineLanes([
      {
        id: 1,
        name: 'Full-Backup',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        starttime: '2026-05-27 08:00:00',
        endtime: '2026-05-27 08:01:00',
      },
      {
        id: 2,
        name: 'Incremental-Backup',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        starttime: '2026-05-27 09:00:00',
        endtime: '2026-05-27 09:01:00',
      },
    ], {
      start: Date.parse('2026-05-27T00:00:00'),
      now: Date.parse('2026-05-27T23:59:59'),
      multiDirectorTimeline: false,
    })

    expect(groups).toHaveLength(1)
    expect(groups[0].lanes).toHaveLength(1)
    expect(groups[0].lanes[0].runs.map(run => run.name)).toEqual([
      'Full-Backup',
      'Incremental-Backup',
    ])
  })
})

describe('distinctFilesetClientOptions', () => {
  it('returns one option per fileset@client tuple, sorted by label', () => {
    const jobs = [
      { name: 'Full', client: 'client-b', fileset: 'FilesetZ' },
      { name: 'Incr', client: 'client-a', fileset: 'FilesetA' },
      { name: 'Full', client: 'client-b', fileset: 'FilesetZ' },
      { name: 'Full', client: '', fileset: '' },
    ]
    expect(distinctFilesetClientOptions(jobs)).toEqual([
      { key: 'FilesetA\u0000client-a', fileset: 'FilesetA', client: 'client-a', label: 'FilesetA@client-a' },
      { key: 'FilesetZ\u0000client-b', fileset: 'FilesetZ', client: 'client-b', label: 'FilesetZ@client-b' },
    ])
  })

  it('returns an empty array for no jobs', () => {
    expect(distinctFilesetClientOptions([])).toEqual([])
    expect(distinctFilesetClientOptions(null)).toEqual([])
  })
})
