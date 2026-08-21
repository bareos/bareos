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

import { describe, it, expect } from 'vitest'
import { midEllipsis } from '../../src/utils/strings.js'

describe('midEllipsis', () => {
  it('returns the original string when it fits within maxLen', () => {
    expect(midEllipsis('short', 10)).toBe('short')
    expect(midEllipsis('exact', 5)).toBe('exact')
  })

  it('truncates from the middle for even maxLen', () => {
    // maxLen=6: half=2, keeps 2 chars from start + ellipsis + 3 from end
    const result = midEllipsis('abcdefghij', 6)
    expect(result).toBe('ab\u2026hij')
    expect([...result].length).toBe(6)
  })

  it('truncates from the middle for odd maxLen', () => {
    // maxLen=7: half=3, keeps 3 chars from start + ellipsis + 3 from end
    const result = midEllipsis('abcdefghij', 7)
    expect(result).toBe('abc\u2026hij')
    expect([...result].length).toBe(7)
  })

  it('preserves the beginning of the string', () => {
    const result = midEllipsis('BackupClient1-long-suffix', 14)
    expect(result.startsWith('Backup')).toBe(true)
  })

  it('preserves the end of the string', () => {
    const result = midEllipsis('common-prefix-unique-suffix', 18)
    expect(result.endsWith('unique-suffix')).toBe(false) // too long
    expect(result.endsWith('uffix')).toBe(true)
  })

  it('result always contains the ellipsis character when truncated', () => {
    const result = midEllipsis('this-is-a-very-long-string', 10)
    expect(result).toContain('\u2026')
  })

  it('result length equals maxLen when truncated', () => {
    for (const len of [5, 6, 7, 10, 14, 18]) {
      const result = midEllipsis('abcdefghijklmnopqrstuvwxyz', len)
      expect([...result].length).toBe(len)
    }
  })

  it('works for typical job and client name patterns', () => {
    // Job names often share a prefix — the suffix is the interesting part
    const r1 = midEllipsis('BackupCatalog', 14)
    expect(r1).toBe('BackupCatalog') // fits

    const r2 = midEllipsis('BackupClientVeryLongName', 18)
    expect(r2.startsWith('Backup')).toBe(true)
    expect(r2.endsWith('Name')).toBe(true)
    expect([...r2].length).toBe(18)

    // Client names: common prefix like "bareos-fd" vs "bareos-fd-prod-dc1"
    const r3 = midEllipsis('bareos-fd-prod-datacenter-1', 14)
    expect(r3.startsWith('bareos')).toBe(true)
    expect(r3.endsWith('er-1')).toBe(true)
    expect([...r3].length).toBe(14)
  })
})
