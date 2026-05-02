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

export function volumeEncryptionKey(volume) {
  if (!volume || typeof volume !== 'object') {
    return ''
  }

  return String(
    volume.encryptionkey
      ?? volume.EncryptionKey
      ?? volume.encrkey
      ?? volume.EncrKey
      ?? ''
  ).trim()
}

function volumeHasEncryptionKeyFlag(volume) {
  const flag = volume?.hasencryptionkey ?? volume?.HasEncryptionKey

  if (typeof flag === 'boolean') {
    return flag
  }

  if (typeof flag === 'number') {
    return flag !== 0
  }

  if (typeof flag === 'string') {
    const normalized = flag.trim().toLowerCase()
    return normalized === '1' || normalized === 'true' || normalized === 'yes'
  }

  return false
}

export function volumeHasEncryptionKey(volume) {
  return volumeHasEncryptionKeyFlag(volume)
    || volumeEncryptionKey(volume).length > 0
}

export function buildVolumeDetailsQuery({
  director,
  directorTab,
  directorTarget,
  jobId,
  poolName,
  storagesTab,
  storagesScopeDirector,
} = {}) {
  const query = {}

  if (director) {
    query.director = director
  }

  if (directorTab && directorTab !== 'status') {
    query.directorTab = directorTab
  }

  if (directorTarget) {
    query.directorTarget = directorTarget
  }

  if (jobId !== null && jobId !== undefined && jobId !== '') {
    query.jobId = String(jobId)
  }

  if (poolName) {
    query.poolName = poolName
  }

  if (storagesTab) {
    query.storagesTab = storagesTab
  }

  if (storagesScopeDirector) {
    query.storagesScopeDirector = storagesScopeDirector
  }

  return query
}

export function resolveVolumeDetailsDirectorOrigin(query) {
  if (typeof query?.directorTab !== 'string' || !query.directorTab) {
    return null
  }

  return {
    tab: query.directorTab,
    targetDirector: typeof query?.directorTarget === 'string' ? query.directorTarget : '',
  }
}

export function resolveVolumeDetailsPoolOrigin(query) {
  if (typeof query?.poolName !== 'string' || !query.poolName) {
    return null
  }

  return {
    name: query.poolName,
  }
}

export function resolveVolumeDetailsJobOrigin(query) {
  if (typeof query?.jobId !== 'string' || !query.jobId) {
    return null
  }

  return {
    id: query.jobId,
  }
}

export function resolveVolumeDetailsStoragesOrigin(query) {
  if (typeof query?.storagesTab !== 'string' || !query.storagesTab) {
    return null
  }

  return {
    tab: query.storagesTab,
    scopeDirector: typeof query?.storagesScopeDirector === 'string'
      ? query.storagesScopeDirector
      : '',
  }
}
