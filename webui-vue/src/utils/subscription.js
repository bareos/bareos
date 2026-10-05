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

export function subscriptionRemainingSummary(unitSummary) {
  const rawRemaining = unitSummary?.remaining
  const parsedRemaining = Number(rawRemaining)

  if (!Number.isFinite(parsedRemaining)) {
    return {
      value: String(rawRemaining ?? ''),
      labelKey: 'Remaining',
      isOverLimit: false,
    }
  }

  if (parsedRemaining < 0) {
    return {
      value: String(Math.abs(parsedRemaining)),
      labelKey: 'Over Limit',
      isOverLimit: true,
    }
  }

  return {
    value: String(parsedRemaining),
    labelKey: 'Remaining',
    isOverLimit: false,
  }
}

export function subscriptionSnapshotAge(snapshot, reportTime, elapsedMs = 0) {
  const parse = (value) => {
    if (!/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(value ?? '')) return NaN
    const date = new Date(value.replace(' ', 'T') + 'Z')
    if (Number.isNaN(date.getTime())) return NaN
    return date.toISOString() === value.replace(' ', 'T') + '.000Z'
      ? date.getTime() : NaN
  }
  // Compare Director-local timestamps in the same civil time frame, not in
  // the browser timezone.
  const age = parse(reportTime) - parse(snapshot?.calculated_at) + elapsedMs
  if (!Number.isFinite(age) || age < 0) return null
  return { milliseconds: age, stale: age > 24 * 60 * 60 * 1000 }
}
