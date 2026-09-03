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
  JOB_STATUS_24H_CHART_SEGMENTS,
  buildJobStatus24hChartSegments,
} from '../../src/dashboard/jobStatus24hChart.js'

describe('JOB_STATUS_24H_CHART_SEGMENTS', () => {
  it('defines the fixed status order and colors used by the chart', () => {
    expect(JOB_STATUS_24H_CHART_SEGMENTS).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8' },
      { status: 'C', labelKey: 'Waiting', color: '#9e9e9e' },
      { status: 'T', labelKey: 'Successful', color: '#28a745' },
      { status: 'W', labelKey: 'Warning', color: '#ffc107' },
      { status: 'f', labelKey: 'Failed', color: '#dc3545' },
    ])
  })
})

describe('buildJobStatus24hChartSegments', () => {
  it('keeps the fixed status order while filtering zero-count slices', () => {
    expect(buildJobStatus24hChartSegments({
      T: 4,
      R: 2,
      f: 0,
      C: 1,
      W: 0,
    })).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8', count: 2 },
      { status: 'C', labelKey: 'Waiting', color: '#9e9e9e', count: 1 },
      { status: 'T', labelKey: 'Successful', color: '#28a745', count: 4 },
    ])
  })

  it('coerces numeric strings and ignores missing or invalid counts', () => {
    expect(buildJobStatus24hChartSegments({
      R: '3',
      C: undefined,
      T: 'not-a-number',
      W: null,
      f: 2,
    })).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8', count: 3 },
      { status: 'f', labelKey: 'Failed', color: '#dc3545', count: 2 },
    ])
  })
})
