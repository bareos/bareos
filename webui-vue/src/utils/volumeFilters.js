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

export const VOLUME_FILTER_QUERY_KEYS = {
  pools: 'volPool',
  statuses: 'volStatus',
  mediatypes: 'volMediaType',
}

const VOLUME_FILTER_FIELDS = {
  pools: 'pool',
  statuses: 'volstatus',
  mediatypes: 'mediatype',
}

// Bareos resource names and volume statuses cannot contain commas.
function decodeList(value) {
  const raw = Array.isArray(value) ? value.join(',') : value
  if (typeof raw !== 'string' || !raw) {
    return []
  }
  return [...new Set(raw.split(',').map(item => item.trim()).filter(Boolean))]
}

export function emptyVolumeFilters() {
  return { pools: [], statuses: [], mediatypes: [] }
}

export function resolveVolumeFilters(query) {
  return Object.fromEntries(
    Object.entries(VOLUME_FILTER_QUERY_KEYS).map(([name, key]) => [name, decodeList(query?.[key])])
  )
}

export function buildVolumeFiltersQuery(query, filters) {
  const next = { ...query }
  for (const [name, key] of Object.entries(VOLUME_FILTER_QUERY_KEYS)) {
    const values = decodeList(filters?.[name] ?? [])
    if (values.length) {
      next[key] = values.join(',')
    } else {
      delete next[key]
    }
  }
  return next
}

export function volumeFiltersEqual(left, right) {
  return Object.keys(VOLUME_FILTER_QUERY_KEYS).every(name => (
    (left?.[name] ?? []).join(',') === (right?.[name] ?? []).join(',')
  ))
}

export function hasVolumeFilters(filters) {
  return Object.keys(VOLUME_FILTER_QUERY_KEYS).some(name => (filters?.[name]?.length ?? 0) > 0)
}

export function filterVolumes(volumes, filters) {
  const active = Object.entries(VOLUME_FILTER_FIELDS)
    .map(([name, field]) => [field, new Set(filters?.[name] ?? [])])
    .filter(([, values]) => values.size > 0)

  if (!active.length) {
    return volumes ?? []
  }

  return (volumes ?? []).filter(volume => (
    active.every(([field, values]) => values.has(String(volume?.[field] ?? '')))
  ))
}

/** Select options with a row count per value, sorted by value. */
export function volumeFilterOptions(volumes, name) {
  const field = VOLUME_FILTER_FIELDS[name]
  const counts = new Map()
  for (const volume of volumes ?? []) {
    const value = String(volume?.[field] ?? '')
    if (value) {
      counts.set(value, (counts.get(value) ?? 0) + 1)
    }
  }
  return [...counts.entries()]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([value, count]) => ({ label: `${value} (${count})`, value }))
}
