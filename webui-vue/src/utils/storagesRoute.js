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

export const AUTOCHANGER_STORAGE_QUERY_KEY = 'autochangerStorage'
export const AUTOCHANGER_DIRECTOR_QUERY_KEY = 'autochangerDirector'

export function buildStoragesTabQuery(query, tab) {
  const next = { ...query }
  delete next.tab

  if (tab && tab !== 'storages') {
    next.tab = tab
  }

  return next
}

export function resolveStoragesScopeDirector(query) {
  return typeof query?.scopeDirector === 'string' ? query.scopeDirector : ''
}

export function withStoragesScopeDirectorQuery(query, director) {
  const next = { ...query }
  const nextDirector = typeof director === 'string' ? director : ''

  delete next.scopeDirector

  if (nextDirector) {
    next.scopeDirector = nextDirector
  }

  return next
}

export function buildAutochangerSelectionQuery(query, storage) {
  const next = buildStoragesTabQuery(query, 'autochangers')

  delete next[AUTOCHANGER_STORAGE_QUERY_KEY]
  delete next[AUTOCHANGER_DIRECTOR_QUERY_KEY]

  if (storage?.name) {
    next[AUTOCHANGER_STORAGE_QUERY_KEY] = storage.name
  }

  if (storage?.director) {
    next[AUTOCHANGER_DIRECTOR_QUERY_KEY] = storage.director
  }

  return next
}

export function resolveAutochangerSelectionQuery(query) {
  if (typeof query?.[AUTOCHANGER_STORAGE_QUERY_KEY] !== 'string'
    || !query[AUTOCHANGER_STORAGE_QUERY_KEY]) {
    return null
  }

  return {
    name: query[AUTOCHANGER_STORAGE_QUERY_KEY],
    director: typeof query?.[AUTOCHANGER_DIRECTOR_QUERY_KEY] === 'string'
      ? query[AUTOCHANGER_DIRECTOR_QUERY_KEY]
      : '',
  }
}

export function resolveAutochangerSelection(storages, {
  storageName,
  directorName,
  activeDirectors = [],
} = {}) {
  if (!storageName) {
    return null
  }

  if (directorName) {
    const exactMatch = storages.find(storage => (
      storage.name === storageName && storage.director === directorName
    ))
    if (exactMatch) {
      return exactMatch
    }
  }

  if (activeDirectors.length === 1) {
    const scopedMatch = storages.find(storage => (
      storage.name === storageName && storage.director === activeDirectors[0]
    ))
    if (scopedMatch) {
      return scopedMatch
    }
  }

  const matches = storages.filter(storage => storage.name === storageName)
  return matches.length === 1 ? matches[0] : null
}
