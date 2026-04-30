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

import {
  AUTOCHANGER_DIRECTOR_QUERY_KEY,
  AUTOCHANGER_STORAGE_QUERY_KEY,
} from './storagesRoute.js'
import { buildVolumeDetailsQuery } from './volumes.js'

export function buildPoolDetailsQuery({
  director,
  volumeName,
  volumeQuery,
} = {}) {
  const query = resolvePoolDetailsVolumeQuery(volumeQuery)

  if (director && !query.director) {
    query.director = director
  }

  if (volumeName && !query.volumeName) {
    query.volumeName = volumeName
  }

  return query
}

export function buildPoolVolumeDetailsQuery({
  director,
  poolName,
  volumeName,
  poolQuery,
} = {}) {
  const query = resolvePoolDetailsVolumeQuery(poolQuery)

  if (director) {
    query.director = director
  }

  if (poolName) {
    query.poolName = poolName
  }

  if (volumeName) {
    query.volumeName = volumeName
  } else {
    delete query.volumeName
  }

  return query
}

export function resolvePoolDetailsVolumeOrigin(query) {
  if (typeof query?.volumeName !== 'string' || !query.volumeName) {
    return null
  }

  return {
    name: query.volumeName,
  }
}

export function resolvePoolDetailsVolumeQuery(query) {
  const nextQuery = buildVolumeDetailsQuery({
    director: typeof query?.director === 'string' ? query.director : '',
    directorTab: typeof query?.directorTab === 'string' ? query.directorTab : '',
    directorTarget: typeof query?.directorTarget === 'string' ? query.directorTarget : '',
    jobId: typeof query?.jobId === 'string' ? query.jobId : '',
    poolName: typeof query?.poolName === 'string' ? query.poolName : '',
  })

  if (typeof query?.volumeName === 'string' && query.volumeName) {
    nextQuery.volumeName = query.volumeName
  }

  if (typeof query?.[AUTOCHANGER_STORAGE_QUERY_KEY] === 'string'
    && query[AUTOCHANGER_STORAGE_QUERY_KEY]) {
    nextQuery[AUTOCHANGER_STORAGE_QUERY_KEY] = query[AUTOCHANGER_STORAGE_QUERY_KEY]
  }

  if (typeof query?.[AUTOCHANGER_DIRECTOR_QUERY_KEY] === 'string'
    && query[AUTOCHANGER_DIRECTOR_QUERY_KEY]) {
    nextQuery[AUTOCHANGER_DIRECTOR_QUERY_KEY] = query[AUTOCHANGER_DIRECTOR_QUERY_KEY]
  }

  return nextQuery
}
