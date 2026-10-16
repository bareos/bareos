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

import { formatBytes, formatDuration } from '../mock/index.js'
import { quoteDirectorString } from './directorStrings.js'

/**
 * The fields that `update volume=<v> frompool=yes` copies from the pool onto
 * the volume. Keep in sync with BareosDb::UpdateMediaDefaults()
 * (core/src/cats/sql_update.cc) and SetPoolDbrDefaultsInMediaDbr()
 * (core/src/dird/ua_db.cc).
 */
export const POOL_DEFAULT_FIELDS = [
  { id: 'volretention', volumeKey: 'volretention', poolKey: 'volretention', label: 'Retention', format: 'duration' },
  { id: 'voluseduration', volumeKey: 'voluseduration', poolKey: 'voluseduration', label: 'Use Duration', format: 'duration' },
  { id: 'maxvoljobs', volumeKey: 'maxvoljobs', poolKey: 'maxvoljobs', label: 'Max Jobs', format: 'countOrUnlimited' },
  { id: 'maxvolfiles', volumeKey: 'maxvolfiles', poolKey: 'maxvolfiles', label: 'Max Files', format: 'countOrUnlimited' },
  { id: 'maxvolbytes', volumeKey: 'maxvolbytes', poolKey: 'maxvolbytes', label: 'Max Bytes', format: 'bytesOrUnlimited' },
  { id: 'recycle', volumeKey: 'recycle', poolKey: 'recycle', label: 'Recycle', format: 'boolean' },
  { id: 'actiononpurge', volumeKey: 'actiononpurge', poolKey: 'actiononpurge', label: 'Action On Purge', format: 'actionOnPurge' },
  { id: 'recyclepoolid', volumeKey: 'recyclepoolid', poolKey: 'recyclepoolid', label: 'Recycle Pool', format: 'poolId' },
  { id: 'minblocksize', volumeKey: 'minblocksize', poolKey: 'minblocksize', label: 'Min Block Size', format: 'bytesOrUnset' },
  { id: 'maxblocksize', volumeKey: 'maxblocksize', poolKey: 'maxblocksize', label: 'Max Block Size', format: 'bytesOrUnset' },
]

/**
 * Catalog values arrive as strings, and "unset" is spelled `0`, `''`, `null`
 * and `undefined` interchangeably. Normalise everything to a number so that
 * `'0'`, `0` and a missing key all compare equal.
 */
export function normalisePoolDefaultValue(value) {
  if (value === null || value === undefined || value === '') {
    return null
  }

  if (typeof value === 'boolean') {
    return value ? 1 : 0
  }

  const numeric = Number(value)
  return Number.isFinite(numeric) ? numeric : null
}

/**
 * True when a field can be compared at all: both records must carry the key,
 * otherwise the director simply does not report it.
 */
export function isPoolDefaultComparable(field, volume, pool) {
  return Object.hasOwn(volume ?? {}, field.volumeKey)
    && Object.hasOwn(pool ?? {}, field.poolKey)
}

/**
 * Fields in which a volume deviates from its pool's defaults. Fields that
 * neither record reports are skipped rather than reported as equal.
 */
export function compareVolumeToPoolDefaults(volume, pool) {
  if (!volume || !pool) {
    return []
  }

  return POOL_DEFAULT_FIELDS
    .filter(field => isPoolDefaultComparable(field, volume, pool))
    .map(field => ({
      field,
      volumeValue: normalisePoolDefaultValue(volume[field.volumeKey]),
      poolValue: normalisePoolDefaultValue(pool[field.poolKey]),
    }))
    .filter(entry => entry.volumeValue !== entry.poolValue)
}

/**
 * Key identifying a pool across directors, matching the `scopeKey` convention
 * used by the aggregating fetchers.
 */
export function poolScopeKey(director, name) {
  return `${director ?? ''}\u0000${name ?? ''}`
}

/** Index pools by director and name for fast lookup during a drift scan. */
export function indexPoolsByScope(pools) {
  const index = new Map()
  for (const pool of pools ?? []) {
    index.set(poolScopeKey(pool.director, pool.name), pool)
  }
  return index
}

/**
 * Volumes whose settings deviate from their pool's defaults, each with the
 * list of differing fields. Volumes whose pool is not in `pools` are skipped:
 * without the pool record there is nothing to compare against.
 */
export function collectPoolDefaultDrift(volumes, pools) {
  const index = pools instanceof Map ? pools : indexPoolsByScope(pools)
  const drift = []

  for (const volume of volumes ?? []) {
    const pool = index.get(poolScopeKey(volume.director, volume.pool))
    if (!pool) {
      continue
    }

    const differences = compareVolumeToPoolDefaults(volume, pool)
    if (differences.length) {
      drift.push({ volume, pool, differences })
    }
  }

  return drift
}

/** Command applying the owning pool's defaults to a volume. */
export function buildApplyPoolDefaultsCommand(volumeName) {
  return `update volume=${quoteDirectorString(volumeName)} frompool=yes`
}

/**
 * Per-volume lookup of the fields deviating from the pool defaults, keyed by
 * `poolScopeKey(volume.director, volume.volumename)`. Used by the volume
 * tables to highlight the deviating cells without re-comparing on every
 * render.
 */
export function buildPoolDefaultDriftIndex(volumes, pools) {
  const poolIndex = pools instanceof Map ? pools : indexPoolsByScope(pools)
  const index = new Map()

  for (const entry of collectPoolDefaultDrift(volumes ?? [], poolIndex)) {
    index.set(
      poolScopeKey(entry.volume.director, entry.volume.volumename),
      new Map(entry.differences.map(diff => [diff.field.id, diff])),
    )
  }

  return index
}

/** The deviating fields of one volume, or an empty map when it conforms. */
export function volumeDriftFields(index, volume) {
  if (!(index instanceof Map) || !volume) {
    return new Map()
  }
  return index.get(poolScopeKey(volume.director, volume.volumename)) ?? new Map()
}

/**
 * Human-readable rendering of a normalised pool-default value. `translate` is
 * the i18n `t()` function; it defaults to the identity so that the module
 * stays usable outside a component.
 */
export function formatPoolDefaultValue(field, value, translate = (text) => text) {
  if (value === null || value === undefined) {
    return translate('unset')
  }

  switch (field?.format) {
    case 'duration':
      return value ? formatDuration(value) : translate('unset')
    case 'bytesOrUnlimited':
      return value ? formatBytes(value) : translate('unlimited')
    case 'bytesOrUnset':
      return value ? formatBytes(value) : translate('unset')
    case 'countOrUnlimited':
      return value ? String(value) : translate('unlimited')
    case 'boolean':
      return value ? translate('Yes') : translate('No')
    case 'poolId':
      return value ? String(value) : translate('unset')
    default:
      return String(value)
  }
}
