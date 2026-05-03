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
import { buildDirectorOptions } from '../../src/utils/director.js'

describe('buildDirectorOptions', () => {
  it('collapses stale selections when only one director is configured', () => {
    expect(buildDirectorOptions({
      availableDirectors: ['bareos-dir'],
      selectedDirectors: ['old-dir-a', 'old-dir-b'],
      currentDirector: 'bareos-dir',
      fallbackDirector: 'old-dir-a',
    })).toEqual([{ label: 'bareos-dir', value: 'bareos-dir' }])
  })

  it('keeps all configured directors in multi-director setups', () => {
    expect(buildDirectorOptions({
      availableDirectors: ['bareos-dir', 'bareos-dir-2'],
      selectedDirectors: ['bareos-dir-2'],
      currentDirector: 'bareos-dir',
    })).toEqual([
      { label: 'bareos-dir', value: 'bareos-dir' },
      { label: 'bareos-dir-2', value: 'bareos-dir-2' },
    ])
  })
})
