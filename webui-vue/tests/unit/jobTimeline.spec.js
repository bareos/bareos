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
import {
  buildCenteredTimelineDayBounds,
  buildCombinedTimelineGroups,
  buildTimelineLanes,
  distinctFilesetClientOptions,
} from '../../src/utils/jobTimeline.js'

describe('buildCenteredTimelineDayBounds', () => {
  it('builds a rolling 24-hour window centered on the current time', () => {
    const now = Date.parse('2026-09-18T20:30:00')

    expect(buildCenteredTimelineDayBounds(now)).toEqual({
      start: Date.parse('2026-09-18T08:30:00'),
      end: Date.parse('2026-09-19T08:30:00'),
    })
  })

  it('falls back to a 24-hour window for invalid durations', () => {
    const now = Date.parse('2026-09-18T20:30:00')

    expect(buildCenteredTimelineDayBounds(now, 0)).toEqual({
      start: Date.parse('2026-09-18T08:30:00'),
      end: Date.parse('2026-09-19T08:30:00'),
    })
  })

  it('keeps the center fixed while using the requested zoom window', () => {
    const now = Date.parse('2026-09-18T20:30:00')

    expect(buildCenteredTimelineDayBounds(now, 6 * 60 * 60 * 1000)).toEqual({
      start: Date.parse('2026-09-18T17:30:00'),
      end: Date.parse('2026-09-18T23:30:00'),
    })
  })
})

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

describe('buildCombinedTimelineGroups', () => {
  const start = Date.parse('2026-05-27T00:00:00')
  const end = Date.parse('2026-05-28T00:00:00')
  const now = Date.parse('2026-05-27T12:00:00')

  it('matches scheduler triggers to actual job lanes by director and job name', () => {
    const groups = buildCombinedTimelineGroups([
      {
        id: 1,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        director: 'bareos-dir',
        starttime: '2026-05-27 08:00:00',
        endtime: '2026-05-27 08:10:00',
      },
    ], [
      {
        job: 'BackupCatalog',
        schedule: 'WeeklyCycle',
        director: 'bareos-dir',
        runtime: Date.parse('2026-05-27T14:00:00') / 1000,
      },
    ], { start, end, now, multiDirectorTimeline: true })

    expect(groups).toHaveLength(1)
    expect(groups[0].lanes).toHaveLength(1)
    expect(groups[0].lanes[0].runs).toHaveLength(1)
    expect(groups[0].lanes[0].markers).toHaveLength(1)
    expect(groups[0].lanes[0].markers[0].name).toBe('BackupCatalog')
  })

  it('keeps future actual jobs hidden while showing future scheduler triggers', () => {
    const groups = buildCombinedTimelineGroups([
      {
        id: 1,
        name: 'FutureActualJob',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        director: 'bareos-dir',
        starttime: '2026-05-27 18:00:00',
        endtime: '2026-05-27 18:10:00',
      },
    ], [
      {
        job: 'FutureActualJob',
        schedule: 'WeeklyCycle',
        director: 'bareos-dir',
        runtime: Date.parse('2026-05-27T18:00:00') / 1000,
      },
    ], { start, end, now, multiDirectorTimeline: true })

    expect(groups).toHaveLength(1)
    expect(groups[0].lanes).toHaveLength(1)
    expect(groups[0].lanes[0].runs).toHaveLength(0)
    expect(groups[0].lanes[0].markers).toHaveLength(1)
  })

  it('shows unmatched scheduler triggers in scheduler-only lanes', () => {
    const groups = buildCombinedTimelineGroups([], [
      {
        job: 'NightlyBackup',
        schedule: 'Nightly',
        director: 'bareos-dir',
        runtime: Date.parse('2026-05-27T02:00:00') / 1000,
      },
    ], { start, end, now, multiDirectorTimeline: true })

    expect(groups).toHaveLength(1)
    expect(groups[0].lanes).toEqual([
      expect.objectContaining({
        type: 'schedule',
        name: 'NightlyBackup',
        schedule: 'Nightly',
        runs: [],
        markers: [expect.objectContaining({ name: 'NightlyBackup' })],
      }),
    ])
  })

  it('does not merge equal job names across directors', () => {
    const groups = buildCombinedTimelineGroups([
      {
        id: 1,
        name: 'BackupCatalog',
        client: 'bareos-fd',
        fileset: 'SelfTest',
        director: 'prod-a',
        starttime: '2026-05-27 08:00:00',
        endtime: '2026-05-27 08:10:00',
      },
    ], [
      {
        job: 'BackupCatalog',
        schedule: 'WeeklyCycle',
        director: 'prod-b',
        runtime: Date.parse('2026-05-27T09:00:00') / 1000,
      },
    ], { start, end, now, multiDirectorTimeline: true })

    expect(groups.map(group => group.director)).toEqual(['prod-a', 'prod-b'])
    expect(groups[0].lanes[0].runs).toHaveLength(1)
    expect(groups[0].lanes[0].markers).toHaveLength(0)
    expect(groups[1].lanes[0].runs).toHaveLength(0)
    expect(groups[1].lanes[0].markers).toHaveLength(1)
  })
})
