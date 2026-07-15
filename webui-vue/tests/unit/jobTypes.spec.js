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
  resolveJobTypeCode,
  resolveJobTypeInfo,
} from '../../src/utils/jobTypes.js'

describe('job type helpers', () => {
  it('maps all known Bareos job type codes to badge metadata', () => {
    expect(resolveJobTypeInfo('B')).toMatchObject({ badge: 'B', labelKey: 'Backup' })
    expect(resolveJobTypeInfo('A')).toMatchObject({ badge: 'A', labelKey: 'Archive' })
    expect(resolveJobTypeInfo('V')).toMatchObject({ badge: 'V', labelKey: 'Verify' })
    expect(resolveJobTypeInfo('R')).toMatchObject({ badge: 'R', labelKey: 'Restore' })
    expect(resolveJobTypeInfo('D')).toMatchObject({ badge: 'D', labelKey: 'Admin' })
    expect(resolveJobTypeInfo('C')).toMatchObject({ badge: 'C', labelKey: 'Job Copy' })
    expect(resolveJobTypeInfo('c')).toMatchObject({ badge: 'C', labelKey: 'Copy' })
    expect(resolveJobTypeInfo('M')).toMatchObject({ badge: 'M', labelKey: 'Migrated Job' })
    expect(resolveJobTypeInfo('g')).toMatchObject({ badge: 'M', labelKey: 'Migration' })
    expect(resolveJobTypeInfo('O')).toMatchObject({ badge: 'O', labelKey: 'Consolidate' })
    expect(resolveJobTypeInfo('S')).toMatchObject({ badge: 'S', labelKey: 'Scan' })
    expect(resolveJobTypeInfo('U')).toMatchObject({ badge: 'U', labelKey: 'Console' })
    expect(resolveJobTypeInfo('I')).toMatchObject({ badge: 'I', labelKey: 'System' })
  })

  it('maps director status names back to job type codes', () => {
    expect(resolveJobTypeCode('Backup')).toBe('B')
    expect(resolveJobTypeCode('Archive')).toBe('A')
    expect(resolveJobTypeCode('Verify')).toBe('V')
    expect(resolveJobTypeCode('Restore')).toBe('R')
    expect(resolveJobTypeCode('Admin')).toBe('D')
    expect(resolveJobTypeCode('Copy')).toBe('c')
    expect(resolveJobTypeCode('Job Copy')).toBe('C')
    expect(resolveJobTypeCode('Migrate')).toBe('g')
    expect(resolveJobTypeCode('Migration')).toBe('g')
    expect(resolveJobTypeCode('Consolidate')).toBe('O')
    expect(resolveJobTypeCode('Scan')).toBe('S')
    expect(resolveJobTypeCode('Console')).toBe('U')
    expect(resolveJobTypeCode('System')).toBe('I')
  })
})
