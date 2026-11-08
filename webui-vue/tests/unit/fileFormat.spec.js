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
import { formatMode, formatMtime } from '../../src/utils/fileFormat.js'

describe('formatMode', () => {
  it('returns an empty string without a mode', () => {
    expect(formatMode(0)).toBe('')
    expect(formatMode(undefined)).toBe('')
  })

  it('formats file type and permissions', () => {
    expect(formatMode(0o100644)).toBe('-rw-r--r--')
    expect(formatMode(0o040755)).toBe('drwxr-xr-x')
    expect(formatMode(0o120777)).toBe('lrwxrwxrwx')
  })

  it('formats setuid, setgid and sticky bits', () => {
    expect(formatMode(0o104755)).toBe('-rwsr-xr-x')
    expect(formatMode(0o102644)).toBe('-rw-r-Sr--')
    expect(formatMode(0o041777)).toBe('drwxrwxrwt')
  })
})

describe('formatMtime', () => {
  it('returns an empty string without a timestamp', () => {
    expect(formatMtime(0)).toBe('')
  })

  it('formats local time as YYYY-MM-DD HH:MM:SS', () => {
    const ts = new Date(2026, 4, 3, 7, 8, 9).getTime() / 1000
    expect(formatMtime(ts)).toBe('2026-05-03 07:08:09')
  })
})
