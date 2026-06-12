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
  directorListLoadErrorMessage,
  shouldAutoLoginAllDirectors,
  summarizeDirectorLoginAttempts,
} from '../../src/utils/directorLogin.js'

describe('director login helpers', () => {
  const t = (message, values = {}) => (
    message.replace('{message}', String(values.message ?? ''))
  )

  it('formats the list_directors load error with the concrete cause', () => {
    expect(directorListLoadErrorMessage(new Error('Cannot connect to proxy'), t))
      .toBe('Director list request failed: Cannot connect to proxy')
  })

  it('falls back to an unknown error message when needed', () => {
    expect(directorListLoadErrorMessage(null, t))
      .toBe('Director list request failed: Unknown error')
  })

  it('enables automatic all-director login only for the main login flow', () => {
    expect(shouldAutoLoginAllDirectors({
      availableDirectors: ['bareos-dir', 'site-b'],
    })).toBe(true)

    expect(shouldAutoLoginAllDirectors({
      isAddDirectorMode: true,
      availableDirectors: ['bareos-dir', 'site-b'],
    })).toBe(false)

    expect(shouldAutoLoginAllDirectors({
      requestedDirector: 'site-b',
      availableDirectors: ['bareos-dir', 'site-b'],
    })).toBe(false)
  })

  it('splits successful and failed director login attempts', () => {
    expect(summarizeDirectorLoginAttempts([
      { director: 'bareos-dir', success: true },
      { director: 'site-b', success: false, message: 'Authentication failed' },
      { director: 'site-c', success: false, message: 'Connection error' },
    ])).toEqual({
      successfulDirectors: ['bareos-dir'],
      failedAttempts: [
        { director: 'site-b', message: 'Authentication failed' },
        { director: 'site-c', message: 'Connection error' },
      ],
      failedDirectors: ['site-b', 'site-c'],
    })
  })
})
