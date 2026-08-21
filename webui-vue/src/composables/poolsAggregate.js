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

import { directorCollection, normalisePool, normaliseVolume } from './useDirectorFetch.js'
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

function scopeKey(director, name) {
  return `${director}:${name}`
}

function decorateVolumes(entries, director) {
  const collection = Array.isArray(entries)
    ? entries
    : Object.values(entries ?? {}).flat()

  return collection.map(entry => {
    const v = normaliseVolume(entry)
    return { ...v, director, scopeKey: scopeKey(director, v.volumename) }
  })
}

function decoratePools(entries, director, bytesByPool, volumesByPool) {
  return directorCollection(entries).map(entry => {
    const pool = normalisePool(entry)
    const key = scopeKey(director, pool.name ?? '')
    return {
      ...pool,
      director,
      scopeKey: key,
      totalbytes: bytesByPool[key] ?? 0,
      totalvolumes: volumesByPool[key] ?? 0,
    }
  })
}

/**
 * Fetch pools with aggregated byte and volume totals across all active
 * directors.  Returns { pools, poolNames, directorErrors }.
 */
export async function fetchAggregatedPools(credentials, directors) {
  const results = await runDirectorAggregates(
    credentials,
    directors,
    async ({ client, director }) => {
      const [poolsResult, volumesResult] = await Promise.all([
        client.call('llist pools'),
        client.call('llist volumes'),
      ])

      const volumes = decorateVolumes(volumesResult?.volumes, director)

      const bytesByPool = {}
      const volumesByPool = {}
      for (const v of volumes) {
        const key = scopeKey(director, v.pool ?? '')
        bytesByPool[key] = (bytesByPool[key] ?? 0) + (Number(v.volbytes) || 0)
        volumesByPool[key] = (volumesByPool[key] ?? 0) + 1
      }

      return decoratePools(poolsResult?.pools, director, bytesByPool, volumesByPool)
    }
  )

  const pools = fulfilledDirectorValues(results)
    .flatMap(v => v)
    .sort((a, b) => String(a.name ?? '').localeCompare(String(b.name ?? '')))

  return {
    pools,
    poolNames: [...new Set(pools.map(p => p.name))].sort(),
    directorErrors: directorAggregateErrors(
      results, directors, 'Failed to load pool data.'
    ),
  }
}
