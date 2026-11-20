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
import { projectScheduleSelection } from '../../src/utils/scheduleSelection.js'

describe('projectScheduleSelection', () => {
  it('preserves custom keys while the active options temporarily exclude them', () => {
    const customKeys = new Set(['director-a:Daily'])

    expect(projectScheduleSelection(
      [{ key: 'director-b:Hourly' }],
      'custom',
      customKeys
    )).toEqual([])
    expect(projectScheduleSelection(
      [{ key: 'director-a:Daily' }, { key: 'director-a:Weekly' }],
      'custom',
      customKeys
    )).toEqual(['director-a:Daily'])
  })

  it('projects All and None intent onto newly available options', () => {
    const options = [{ key: 'a' }, { key: 'b' }]
    const customKeys = new Set()

    expect(projectScheduleSelection(options, 'none', customKeys)).toEqual([])
    expect(projectScheduleSelection(options, 'all', customKeys)).toEqual(['a', 'b'])
  })
})
