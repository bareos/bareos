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
import { formatDirectorSessionsSummary } from '../../src/utils/directorSessions.js'

describe('director session summary', () => {
  const t = (message, values = {}) => (
    message.replace('{count}', String(values.count ?? ''))
  )

  it('shows a generic label when there are no director sessions', () => {
    expect(formatDirectorSessionsSummary([], t)).toBe('Director logins')
  })

  it('shows the single authenticated session identity', () => {
    expect(formatDirectorSessionsSummary([
      { username: 'admin', director: 'bareos-dir' },
    ], t)).toBe('admin @ bareos-dir')
  })

  it('shows a count when multiple director sessions are authenticated', () => {
    expect(formatDirectorSessionsSummary([
      { username: 'admin', director: 'bareos-dir' },
      { username: 'ops', director: 'bareos-dir-2' },
    ], t)).toBe('2 director logins')
  })
})
