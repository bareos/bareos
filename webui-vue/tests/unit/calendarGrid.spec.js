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
  calendarDayBounds,
  calendarDaysBetween,
  calendarWeekBounds,
} from '../../src/utils/calendarGrid.js'

function inNewYorkTimeZone(callback) {
  const previousTimeZone = process.env.TZ
  process.env.TZ = 'America/New_York'
  try {
    callback()
  } finally {
    if (previousTimeZone === undefined) {
      delete process.env.TZ
    } else {
      process.env.TZ = previousTimeZone
    }
  }
}

describe('local calendar boundaries', () => {
  it('uses 23-hour local days across the spring DST transition', () => {
    inNewYorkTimeZone(() => {
      const { start, end } = calendarDayBounds(new Date(2026, 2, 8))

      expect(new Date(start).toString()).toContain('Mar 08 2026 00:00:00')
      expect(new Date(end).toString()).toContain('Mar 09 2026 00:00:00')
      expect(end - start).toBe(23 * 60 * 60 * 1000)
    })
  })

  it('uses 25-hour local days across the fall DST transition', () => {
    inNewYorkTimeZone(() => {
      const { start, end } = calendarDayBounds(new Date(2026, 10, 1))

      expect(new Date(start).toString()).toContain('Nov 01 2026 00:00:00')
      expect(new Date(end).toString()).toContain('Nov 02 2026 00:00:00')
      expect(end - start).toBe(25 * 60 * 60 * 1000)
    })
  })

  it('uses local calendar midnights for weeks spanning DST changes', () => {
    inNewYorkTimeZone(() => {
      const springWeek = calendarWeekBounds(new Date(2026, 2, 2))
      const fallWeek = calendarWeekBounds(new Date(2026, 9, 26))

      expect(springWeek.end - springWeek.start).toBe(167 * 60 * 60 * 1000)
      expect(fallWeek.end - fallWeek.start).toBe(169 * 60 * 60 * 1000)
      expect(new Date(springWeek.end).toString()).toContain('Mar 09 2026 00:00:00')
      expect(new Date(fallWeek.end).toString()).toContain('Nov 02 2026 00:00:00')
    })
  })

  it('counts calendar dates rather than elapsed 24-hour intervals', () => {
    inNewYorkTimeZone(() => {
      const springStart = new Date(2026, 2, 8)
      const springEnd = new Date(2026, 2, 10)
      const fallStart = new Date(2026, 10, 1)
      const fallEnd = new Date(2026, 10, 3)

      expect(calendarDaysBetween(springStart, springEnd)).toBe(2)
      expect(calendarDaysBetween(fallStart, fallEnd)).toBe(2)
    })
  })
})
