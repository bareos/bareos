/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General
   Public License as published by the Free Software Foundation and
   included in the file LICENSE.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import { describe, expect, it } from 'vitest'
import {
  firstOfMonth,
  makeDateStr,
  mondayOf,
  monthGridDates,
  parseDateStr,
  weekDates,
} from '../../src/utils/calendarGrid.js'

function localDateStr(date) {
  return makeDateStr(date.getFullYear(), date.getMonth(), date.getDate())
}

describe('calendar grid helpers', () => {
  it('starts weeks on Monday and returns seven dates', () => {
    const dates = weekDates(mondayOf(new Date(2026, 5, 17)))

    expect(dates).toHaveLength(7)
    expect(dates.map(localDateStr)).toEqual([
      '2026-06-15',
      '2026-06-16',
      '2026-06-17',
      '2026-06-18',
      '2026-06-19',
      '2026-06-20',
      '2026-06-21',
    ])
  })

  it('creates a complete Monday-first month grid', () => {
    const dates = monthGridDates(firstOfMonth(new Date(2026, 5, 17)))

    expect(dates).toHaveLength(35)
    expect(localDateStr(dates[0])).toBe('2026-06-01')
    expect(localDateStr(dates[29])).toBe('2026-06-30')
    expect(dates.slice(30)).toEqual([null, null, null, null, null])
  })

  it('formats and parses local calendar dates', () => {
    const date = parseDateStr('2026-09-15')

    expect(localDateStr(date)).toBe('2026-09-15')
    expect(makeDateStr(2026, 8, 15)).toBe('2026-09-15')
  })
})
