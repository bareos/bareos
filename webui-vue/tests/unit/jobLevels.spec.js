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
  resolveJobLevelCode,
  resolveJobLevelInfo,
} from '../../src/utils/jobLevels.js'

describe('job level helpers', () => {
  it('maps known full and truncated Bareos level strings', () => {
    expect(resolveJobLevelCode('F')).toBe('F')
    expect(resolveJobLevelCode('Incremental')).toBe('I')
    expect(resolveJobLevelCode('Increme')).toBe('I')
    expect(resolveJobLevelCode('Incr')).toBe('I')
    expect(resolveJobLevelCode('Differential')).toBe('D')
    expect(resolveJobLevelCode('Differe')).toBe('D')
    expect(resolveJobLevelCode('Diff')).toBe('D')
    expect(resolveJobLevelCode('Virtual Full')).toBe('V')
    expect(resolveJobLevelCode('Virtual')).toBe('V')
    expect(resolveJobLevelCode('VFul')).toBe('V')
    expect(resolveJobLevelCode('Base')).toBe('B')
  })

  it('returns badge metadata for normalized levels', () => {
    expect(resolveJobLevelInfo('Increme')).toMatchObject({
      badge: 'I',
      color: 'teal',
      labelKey: 'Incremental',
    })
    expect(resolveJobLevelInfo('Differe')).toMatchObject({
      badge: 'D',
      color: 'orange',
      labelKey: 'Differential',
    })
  })
})
