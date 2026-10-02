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
  QUEUED_GROUP_THRESHOLD,
  resolveQueuedViewMode,
  extractCancelableGroupJobs,
} from '../../src/utils/runningJobsWidget.js'

describe('resolveQueuedViewMode', () => {
  it('uses the threshold when no manual override is set', () => {
    expect(resolveQueuedViewMode(0)).toBe('flat')
    expect(resolveQueuedViewMode(QUEUED_GROUP_THRESHOLD)).toBe('flat')
    expect(resolveQueuedViewMode(QUEUED_GROUP_THRESHOLD + 1)).toBe('grouped')
  })

  it('honours explicit flat/grouped overrides', () => {
    expect(resolveQueuedViewMode(999, 'flat')).toBe('flat')
    expect(resolveQueuedViewMode(1, 'grouped')).toBe('grouped')
  })
})

describe('extractCancelableGroupJobs', () => {
  it('returns jobs with their resolved ids in order', () => {
    const first = { id: 12, name: 'job-12' }
    const second = { jobid: 34, name: 'job-34' }

    expect(extractCancelableGroupJobs({ jobs: [first, second] })).toEqual([
      { job: first, id: 12 },
      { job: second, id: 34 },
    ])
  })

  it('drops jobs without a cancelable id', () => {
    const valid = { id: '55', name: 'valid' }

    expect(extractCancelableGroupJobs({
      jobs: [{ name: 'missing' }, { id: '' }, { id: null }, valid],
    })).toEqual([
      { job: valid, id: '55' },
    ])
  })

  it('handles missing groups', () => {
    expect(extractCancelableGroupJobs()).toEqual([])
  })
})
