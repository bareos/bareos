/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
  buildVolumeFiltersQuery,
  emptyVolumeFilters,
  filterVolumes,
  hasVolumeFilters,
  resolveVolumeFilters,
  volumeFilterOptions,
  volumeFiltersEqual,
} from '../../src/utils/volumeFilters.js'

const volumes = [
  { volumename: 'F1', pool: 'Full', volstatus: 'Append', mediatype: 'File' },
  { volumename: 'F2', pool: 'Full', volstatus: 'Full', mediatype: 'File' },
  { volumename: 'I1', pool: 'Incremental', volstatus: 'Append', mediatype: 'File' },
  { volumename: 'T1', pool: 'Tape', volstatus: 'Full', mediatype: 'LTO' },
]

describe('volume filter helpers', () => {
  it('decodes filters from comma-joined and repeated query values', () => {
    expect(resolveVolumeFilters({
      volPool: 'Full, Incremental,Full',
      volStatus: ['Append', 'Full'],
      other: 'x',
    })).toEqual({
      pools: ['Full', 'Incremental'],
      statuses: ['Append', 'Full'],
      mediatypes: [],
    })
    expect(resolveVolumeFilters(undefined)).toEqual(emptyVolumeFilters())
  })

  it('encodes filters into the query and drops empty ones', () => {
    expect(buildVolumeFiltersQuery(
      { tab: 'volumes', volStatus: 'Append' },
      { pools: ['Full', 'Tape'], statuses: [], mediatypes: ['LTO'] }
    )).toEqual({ tab: 'volumes', volPool: 'Full,Tape', volMediaType: 'LTO' })
  })

  it('compares and detects active filters', () => {
    expect(volumeFiltersEqual(emptyVolumeFilters(), {})).toBe(true)
    expect(volumeFiltersEqual({ pools: ['Full'] }, { pools: ['Tape'] })).toBe(false)
    expect(hasVolumeFilters(emptyVolumeFilters())).toBe(false)
    expect(hasVolumeFilters({ mediatypes: ['LTO'] })).toBe(true)
  })

  it('filters with OR inside and AND across filter kinds', () => {
    expect(filterVolumes(volumes, emptyVolumeFilters())).toBe(volumes)
    expect(filterVolumes(volumes, { pools: ['Full', 'Incremental'], statuses: ['Append'] })
      .map(v => v.volumename)).toEqual(['F1', 'I1'])
    expect(filterVolumes(volumes, { mediatypes: ['LTO'] }).map(v => v.volumename)).toEqual(['T1'])
    expect(filterVolumes(null, { pools: ['Full'] })).toEqual([])
  })

  it('builds sorted select options with counts', () => {
    expect(volumeFilterOptions(volumes, 'statuses')).toEqual([
      { label: 'Append (2)', value: 'Append' },
      { label: 'Full (2)', value: 'Full' },
    ])
    expect(volumeFilterOptions(volumes, 'mediatypes').map(o => o.value)).toEqual(['File', 'LTO'])
  })
})
