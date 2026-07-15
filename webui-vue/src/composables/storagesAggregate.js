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

import {
  directorCollection,
  normalisePool,
  normaliseVolume,
} from './useDirectorFetch.js'
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

function storageScopeKey(director, name) {
  return `${director}:${name}`
}

function decorateStorages(entries, director) {
  return directorCollection(entries).map(entry => ({
    ...entry,
    autochanger: entry.autochanger === '1' || entry.autochanger === true,
    enabled: entry.enabled !== '0' && entry.enabled !== false,
    director,
    scopeKey: storageScopeKey(director, entry.name ?? ''),
  }))
}

function decorateAutochangerStorages(entries, director) {
  return decorateStorages(entries, director)
    .filter(entry => entry.autochanger)
    .map(entry => ({
      ...entry,
      label: `${director} / ${entry.name}`,
    }))
}

function decorateVolumes(entries, director) {
  const raw = entries
  const collection = Array.isArray(raw)
    ? raw
    : Object.values(raw ?? {}).flat()

  return collection.map((entry) => {
    const volume = normaliseVolume(entry)
    return {
      ...volume,
      director,
      scopeKey: storageScopeKey(director, volume.volumename),
    }
  })
}

function decoratePools(entries, director, bytesByPool) {
  return directorCollection(entries).map(entry => {
    const pool = normalisePool(entry)
    return {
      ...entry,
      ...pool,
      totalbytes: bytesByPool[storageScopeKey(director, pool.name ?? '')] ?? 0,
      director,
      scopeKey: storageScopeKey(director, pool.name ?? ''),
    }
  })
}

function sortByNameAndDirector(entries, nameField) {
  return [...entries].sort((left, right) => {
    const nameCompare = String(left[nameField] ?? '').localeCompare(
      String(right[nameField] ?? '')
    )
    if (nameCompare !== 0) {
      return nameCompare
    }

    return String(left.director ?? '').localeCompare(String(right.director ?? ''))
  })
}

export function normaliseDirectorStoragesState(
  director,
  storagesData,
  poolsData,
  volumesData
) {
  const volumes = decorateVolumes(volumesData, director)
  const bytesByPool = Object.fromEntries(
    volumes.reduce((totals, volume) => {
      const key = storageScopeKey(director, volume.pool ?? '')
      totals.set(key, (totals.get(key) ?? 0) + (Number(volume.volbytes) || 0))
      return totals
    }, new Map())
  )

  return {
    storages: sortByNameAndDirector(decorateStorages(storagesData, director), 'name'),
    pools: sortByNameAndDirector(decoratePools(poolsData, director, bytesByPool), 'name'),
    volumes: sortByNameAndDirector(volumes, 'volumename'),
  }
}

export async function fetchAggregatedStoragesState(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const [storagesResult, poolsResult, volumesResult] = await Promise.all([
      client.call('list storages'),
      client.call('llist pools'),
      client.call('llist volumes'),
    ])

    return normaliseDirectorStoragesState(
      director,
      storagesResult?.storages,
      poolsResult?.pools,
      volumesResult?.volumes
    )
  })

  return {
    storages: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value.storages), 'name'),
    pools: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value.pools), 'name'),
    volumes: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value.volumes), 'volumename'),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load storages.'),
  }
}

export async function fetchAggregatedAutochangerStorages(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const storagesResult = await client.call('list storages')
    return decorateAutochangerStorages(storagesResult?.storages, director)
  })

  return {
    storages: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value), 'name'),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load autochangers.'),
  }
}
