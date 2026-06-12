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
  canReuseDirectorCredentials,
  getDirectorReuseResult,
  isSuccessfulDirectorReuse,
} from '../../src/utils/directorSessionReuse.js'

describe('director session reuse helpers', () => {
  it('allows reuse only for add-director flows targeting another director', () => {
    expect(canReuseDirectorCredentials({
      isAddDirectorMode: true,
      sourceDirector: 'bareos-dir',
      targetDirector: 'bareos-dir-2',
    })).toBe(true)

    expect(canReuseDirectorCredentials({
      isAddDirectorMode: false,
      sourceDirector: 'bareos-dir',
      targetDirector: 'bareos-dir-2',
    })).toBe(false)

    expect(canReuseDirectorCredentials({
      isAddDirectorMode: true,
      sourceDirector: 'bareos-dir',
      targetDirector: 'bareos-dir',
    })).toBe(false)
  })

  it('finds the reuse result for a director', () => {
    expect(getDirectorReuseResult({
      results: [
        { director: 'bareos-dir', status: 'already_authenticated' },
        { director: 'bareos-dir-2', status: 'authenticated' },
      ],
    }, 'bareos-dir-2')).toEqual({
      director: 'bareos-dir-2',
      status: 'authenticated',
    })
  })

  it('recognizes successful reuse statuses', () => {
    expect(isSuccessfulDirectorReuse({
      results: [{ director: 'bareos-dir-2', status: 'authenticated' }],
    }, 'bareos-dir-2')).toBe(true)

    expect(isSuccessfulDirectorReuse({
      results: [{ director: 'bareos-dir-2', status: 'already_authenticated' }],
    }, 'bareos-dir-2')).toBe(true)

    expect(isSuccessfulDirectorReuse({
      results: [{ director: 'bareos-dir-2', status: 'authentication_failed' }],
    }, 'bareos-dir-2')).toBe(false)
  })
})
