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

// Pure date-grid helpers shared by the Scheduler Preview (SchedulesPage.vue)
// and the Job Timeline (JobTimeline.vue), so both Month/Week/Day calendar
// UIs stay in lockstep instead of maintaining two independent copies of the
// same date math.

export function startOfToday() {
  const d = new Date()
  d.setHours(0, 0, 0, 0)
  return d
}

export function mondayOf(date) {
  const d = new Date(date)
  d.setHours(0, 0, 0, 0)
  const dow = d.getDay()
  d.setDate(d.getDate() - ((dow + 6) % 7))
  return d
}

export function firstOfMonth(date) {
  return new Date(date.getFullYear(), date.getMonth(), 1)
}

export function makeDateStr(y, m, d) {
  return `${y}-${String(m + 1).padStart(2, '0')}-${String(d).padStart(2, '0')}`
}

export function parseDateStr(dateStr) {
  const [y, m, d] = dateStr.split('-').map(Number)
  return new Date(y, m - 1, d)
}

// The 7 calendar dates of the week starting at `anchorMonday`.
export function weekDates(anchorMonday) {
  return Array.from({ length: 7 }, (_, i) => {
    const day = new Date(anchorMonday)
    day.setDate(day.getDate() + i)
    return day
  })
}

// The full 7-column grid for the month containing `anchorFirstOfMonth`,
// padded with `null` for the leading/trailing blank cells so the result
// length is always a multiple of 7.
export function monthGridDates(anchorFirstOfMonth) {
  const y = anchorFirstOfMonth.getFullYear()
  const m = anchorFirstOfMonth.getMonth()
  const firstWeekday = new Date(y, m, 1).getDay()
  const daysInMonth = new Date(y, m + 1, 0).getDate()
  const startOffset = (firstWeekday + 6) % 7

  const cells = []
  for (let i = 0; i < startOffset; i++) cells.push(null)
  for (let d = 1; d <= daysInMonth; d++) cells.push(new Date(y, m, d))
  while (cells.length % 7 !== 0) cells.push(null)
  return cells
}
