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
  buildVolumeTapeSegments,
  buildVolumeDetailsQuery,
  resolveVolumeDetailsDirectorOrigin,
  resolveVolumeDetailsJobOrigin,
  resolveVolumeDetailsPoolOrigin,
  resolveVolumeDetailsPoolsOrigin,
  volumeEncryptionKey,
  volumeFileIndexLabel,
  volumeHasEncryptionKey,
  volumeMediaPositionLabel,
  volumeUsageSegmentsFromResponse,
} from '../../src/utils/volumes.js'

describe('volume encryption helpers', () => {
  it('detects an encryption key from the presence flag', () => {
    expect(volumeHasEncryptionKey({ hasencryptionkey: '1' })).toBe(true)
    expect(volumeHasEncryptionKey({ HasEncryptionKey: true })).toBe(true)
    expect(volumeHasEncryptionKey({ hasencryptionkey: '0' })).toBe(false)
  })

  it('detects catalog encryption keys with different field casings', () => {
    expect(volumeEncryptionKey({ EncryptionKey: 'abc123' })).toBe('abc123')
    expect(volumeEncryptionKey({ encryptionkey: 'xyz789' })).toBe('xyz789')
  })

  it('treats empty and missing keys as not encrypted', () => {
    expect(volumeHasEncryptionKey({ encryptionkey: '   ' })).toBe(false)
    expect(volumeHasEncryptionKey({})).toBe(false)
  })

  it('builds volume detail queries with optional director origin', () => {
    expect(buildVolumeDetailsQuery({
      director: 'prod-a',
      directorTab: 'catalog',
      directorTarget: 'prod-b',
      jobId: 42,
      poolName: 'Full',
      poolsTab: 'volumes',
      poolsScopeDirector: 'prod-a',
    })).toEqual({
      director: 'prod-a',
      directorTab: 'catalog',
      directorTarget: 'prod-b',
      jobId: '42',
      poolName: 'Full',
      poolsTab: 'volumes',
      poolsScopeDirector: 'prod-a',
    })

    expect(buildVolumeDetailsQuery({
      director: 'prod-a',
      directorTab: 'status',
      directorTarget: 'prod-b',
    })).toEqual({
      director: 'prod-a',
      directorTarget: 'prod-b',
    })
  })

  it('resolves an optional director origin for volume details routes', () => {
    expect(resolveVolumeDetailsDirectorOrigin({
      directorTab: 'catalog',
      directorTarget: 'prod-b',
    })).toEqual({
      tab: 'catalog',
      targetDirector: 'prod-b',
    })

    expect(resolveVolumeDetailsDirectorOrigin({
      directorTarget: 'prod-b',
    })).toBeNull()
  })

  it('resolves an optional pool origin for volume details routes', () => {
    expect(resolveVolumeDetailsPoolOrigin({
      poolName: 'Full',
    })).toEqual({
      name: 'Full',
    })

    expect(resolveVolumeDetailsPoolOrigin({})).toBeNull()
  })

  it('resolves an optional job origin for volume details routes', () => {
    expect(resolveVolumeDetailsJobOrigin({
      jobId: '42',
    })).toEqual({
      id: '42',
    })

    expect(resolveVolumeDetailsJobOrigin({})).toBeNull()
  })

  it('resolves an optional storages origin for volume details routes', () => {
    expect(resolveVolumeDetailsPoolsOrigin({
      poolsTab: 'volumes',
      poolsScopeDirector: 'prod-a',
    })).toEqual({
      tab: 'volumes',
      scopeDirector: 'prod-a',
    })

    expect(resolveVolumeDetailsPoolsOrigin({})).toBeNull()
  })
})

describe('volume tape usage helpers', () => {
  it('extracts volume usage segments from supported response shapes', () => {
    const segments = [{ jobid: 1 }]

    expect(volumeUsageSegmentsFromResponse(segments)).toBe(segments)
    expect(volumeUsageSegmentsFromResponse({ segments })).toBe(segments)
    expect(volumeUsageSegmentsFromResponse({ volumeusage: { segments } })).toBe(segments)
    expect(volumeUsageSegmentsFromResponse({})).toEqual([])
  })

  it('formats FileIndex and media position labels', () => {
    expect(volumeFileIndexLabel({ firstindex: 1, lastindex: 10 })).toBe('1–10')
    expect(volumeFileIndexLabel({ firstindex: 7, lastindex: 7 })).toBe('7')
    expect(volumeFileIndexLabel({})).toBe('—')

    expect(volumeMediaPositionLabel({
      startfile: 0,
      startblock: 10,
      endfile: 0,
      endblock: 99,
    })).toBe('0:10–0:99')
    expect(volumeMediaPositionLabel({})).toBe('—')
  })

  it('orders tape segments by physical media position', () => {
    const segments = buildVolumeTapeSegments([
      { jobmediaid: 3, jobid: 30, startfile: 1, startblock: 0, jobbytes: 10 },
      { jobmediaid: 1, jobid: 10, startfile: 0, startblock: 100, jobbytes: 10 },
      { jobmediaid: 2, jobid: 20, startfile: 0, startblock: 200, jobbytes: 10 },
    ])

    expect(segments.map(segment => segment.jobid)).toEqual(['10', '20', '30'])
  })

  it('uses JobMedia bytes for percentages and merges job metadata', () => {
    const segments = buildVolumeTapeSegments([
      {
        jobmediaid: 1,
        jobid: 10,
        firstindex: 1,
        lastindex: 10,
        startfile: 0,
        startblock: 100,
        jobbytes: 25,
      },
      {
        jobmediaid: 2,
        jobid: 20,
        firstindex: 1,
        lastindex: 5,
        startfile: 0,
        startblock: 200,
        jobbytes: 75,
      },
    ], [
      { jobid: 10, name: 'BackupA', client: 'a-fd', jobstatus: 'T' },
      { jobid: 20, name: 'BackupB', client: 'b-fd', jobstatus: 'W' },
    ])

    expect(segments[0]).toMatchObject({
      jobid: '10',
      name: 'BackupA',
      client: 'a-fd',
      status: 'T',
      fileIndexLabel: '1–10',
      pct: 25,
    })
    expect(segments[1].pct).toBe(75)
  })

  it('falls back to FileIndex spans when JobMedia bytes are missing', () => {
    const segments = buildVolumeTapeSegments([
      { jobmediaid: 1, jobid: 10, firstindex: 1, lastindex: 10 },
      { jobmediaid: 2, jobid: 20, firstindex: 1, lastindex: 30 },
    ])

    expect(segments[0].pct).toBe(25)
    expect(segments[1].pct).toBe(75)
  })

  it('builds lanes for five parallel overlapping JobMedia ranges', () => {
    const segments = buildVolumeTapeSegments([
      { jobmediaid: 1, jobid: 101, firstindex: 1, lastindex: 10, startblock: 100, endblock: 500, jobbytes: 10 },
      { jobmediaid: 2, jobid: 102, firstindex: 1, lastindex: 10, startblock: 200, endblock: 600, jobbytes: 10 },
      { jobmediaid: 3, jobid: 103, firstindex: 1, lastindex: 10, startblock: 300, endblock: 700, jobbytes: 10 },
      { jobmediaid: 4, jobid: 104, firstindex: 1, lastindex: 10, startblock: 400, endblock: 800, jobbytes: 10 },
      { jobmediaid: 5, jobid: 105, firstindex: 1, lastindex: 10, startblock: 500, endblock: 900, jobbytes: 10 },
    ])

    expect(segments.every(segment => segment.hasOverlaps)).toBe(true)
    expect(segments.map(segment => segment.colorIndex)).toEqual([0, 1, 2, 3, 4])
    expect(segments[0]).toMatchObject({ leftPct: 0, widthPct: 50 })
    expect(segments[4]).toMatchObject({ leftPct: 50, widthPct: 50 })
  })

  it('uses stable colors for repeated ranges from the same job', () => {
    const segments = buildVolumeTapeSegments([
      { jobmediaid: 1, jobid: 101, startblock: 100, endblock: 200, jobbytes: 10 },
      { jobmediaid: 2, jobid: 102, startblock: 300, endblock: 400, jobbytes: 10 },
      { jobmediaid: 3, jobid: 101, startblock: 500, endblock: 600, jobbytes: 10 },
    ])

    expect(segments.map(segment => segment.colorIndex)).toEqual([0, 1, 0])
    expect(segments.some(segment => segment.hasOverlaps)).toBe(false)
  })
})
