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

function isTrueFlag(value) {
  return value === true || value === 1 || value === '1'
    || String(value ?? '').toLowerCase() === 'yes'
    || String(value ?? '').toLowerCase() === 'true'
}

function isFalseFlag(value) {
  return value === false || value === 0 || value === '0'
    || String(value ?? '').toLowerCase() === 'no'
    || String(value ?? '').toLowerCase() === 'false'
}

function configStorageEntries(showStoragesData) {
  if (!showStoragesData || typeof showStoragesData !== 'object') {
    return []
  }

  const entries = Array.isArray(showStoragesData)
    ? showStoragesData
    : Object.entries(showStoragesData).map(([name, entry]) => ({ name, ...entry }))

  return entries
    .filter(entry => entry && typeof entry === 'object')
    .map(entry => Object.fromEntries(
      Object.entries(entry).map(([key, value]) => [key.toLowerCase(), value])
    ))
    .filter(entry => entry.name)
}

/**
 * Merges the catalog storage rows (`list storages`, only name and
 * autochanger flag) with the Director configuration (`show storages`),
 * which provides address, port, media type and the enabled flag. `show`
 * omits values that are at their default (enabled=yes, autochanger=no).
 */
export function mergeStorageConfig(catalogEntries, showStoragesData) {
  const merged = new Map()

  for (const entry of directorCollection(catalogEntries)) {
    if (!entry?.name) {
      continue
    }
    merged.set(entry.name, { ...entry })
  }

  for (const config of configStorageEntries(showStoragesData)) {
    const current = merged.get(config.name) ?? { name: config.name }
    const next = { ...current, inconfig: true }
    if (config.address != null) next.address = String(config.address)
    if (config.port != null) next.port = String(config.port)
    if (config.mediatype != null) next.mediatype = String(config.mediatype)
    if (config.autochanger != null) next.autochanger = isTrueFlag(config.autochanger)
    next.enabled = config.enabled == null ? true : !isFalseFlag(config.enabled)
    merged.set(config.name, next)
  }

  return [...merged.values()]
}

function decorateStorages(entries, director) {
  return directorCollection(entries).map(entry => ({
    ...entry,
    autochanger: isTrueFlag(entry.autochanger),
    enabled: !isFalseFlag(entry.enabled),
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

export function normaliseDirectorStorages(director, storagesData, showStoragesData) {
  return sortByNameAndDirector(
    decorateStorages(mergeStorageConfig(storagesData, showStoragesData), director),
    'name'
  )
}

export function normaliseDirectorPoolsState(director, poolsData, volumesData) {
  const volumes = decorateVolumes(volumesData, director)
  const bytesByPool = Object.fromEntries(
    volumes.reduce((totals, volume) => {
      const key = storageScopeKey(director, volume.pool ?? '')
      totals.set(key, (totals.get(key) ?? 0) + (Number(volume.volbytes) || 0))
      return totals
    }, new Map())
  )

  return {
    pools: sortByNameAndDirector(decoratePools(poolsData, director, bytesByPool), 'name'),
    volumes: sortByNameAndDirector(volumes, 'volumename'),
  }
}

// `show` may be denied by the console ACL; the catalog data still works.
export async function fetchDirectorStorages(call, director) {
  const [storagesResult, showResult] = await Promise.all([
    call('list storages'),
    call('show storages').catch(() => null),
  ])
  return normaliseDirectorStorages(director, storagesResult?.storages, showResult?.storages)
}

export async function fetchDirectorPoolsState(call, director) {
  const [poolsResult, volumesResult] = await Promise.all([
    call('llist pools'),
    call('llist volumes'),
  ])
  return normaliseDirectorPoolsState(director, poolsResult?.pools, volumesResult?.volumes)
}

export async function fetchAggregatedStorages(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, ({ client, director }) => (
    fetchDirectorStorages(command => client.call(command), director)
  ))

  return {
    storages: sortByNameAndDirector(fulfilledDirectorValues(results).flat(), 'name'),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load storages.'),
  }
}

export async function fetchAggregatedPoolsState(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, ({ client, director }) => (
    fetchDirectorPoolsState(command => client.call(command), director)
  ))

  return {
    pools: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value.pools), 'name'),
    volumes: sortByNameAndDirector(fulfilledDirectorValues(results).flatMap(value => value.volumes), 'volumename'),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load pools.'),
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
