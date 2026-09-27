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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.
*/

// Helpers to detect table columns that hold the very same value in every
// visible row. Such columns carry no information and can be collapsed into a
// single summary line above the table.

function defaultKeyOf(column) {
  if (typeof column.field === 'string') {
    return column.field
  }
  return column.name
}

export function columnValueOf(row, column) {
  if (!row || !column) {
    return undefined
  }
  if (typeof column.field === 'function') {
    return column.field(row)
  }
  return row[defaultKeyOf(column)]
}

export function normaliseCommonValue(value) {
  if (value === null || value === undefined) {
    return ''
  }
  if (typeof value === 'string') {
    return value.trim()
  }
  return String(value)
}

// Returns the distinct normalised values of a column across all rows. Stops
// early once more than `limit` distinct values were seen, because the callers
// only ever care about "exactly one" versus "more than one".
export function distinctColumnValues(rows, column, limit = 2) {
  const seen = new Set()
  for (const row of rows ?? []) {
    seen.add(normaliseCommonValue(columnValueOf(row, column)))
    if (seen.size > limit) {
      break
    }
  }
  return [...seen]
}

export function columnIsUniform(rows, column) {
  if (!Array.isArray(rows) || rows.length < 2) {
    return false
  }
  return distinctColumnValues(rows, column).length === 1
}

/*
 * Collects the columns whose rows all share one single value.
 *
 * `options.exclude` names columns that must never be collapsed (the row
 * identity, for instance). `options.format` maps a column name to a display
 * formatter for the collapsed value.
 */
export function collectCommonColumnValues(rows, columns, options = {}) {
  const exclude = new Set(options.exclude ?? [])
  const format = options.format ?? {}
  if (!Array.isArray(rows) || rows.length < 2) {
    return []
  }

  const result = []
  for (const column of columns ?? []) {
    if (!column || exclude.has(column.name)) {
      continue
    }
    const values = distinctColumnValues(rows, column)
    if (values.length !== 1) {
      continue
    }
    const raw = values[0]
    if (raw === '') {
      continue
    }
    const formatter = format[column.name]
    result.push({
      name: column.name,
      label: column.label ?? column.name,
      value: raw,
      display: formatter ? formatter(columnValueOf(rows[0], column), rows[0]) : raw,
    })
  }
  return result
}
