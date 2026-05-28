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

export function getRestoreBrowserPlaceholder({
  browserError,
  loadingBrowser,
  hasSelectedJob,
}) {
  if (browserError) {
    return 'error'
  }

  if (loadingBrowser && hasSelectedJob) {
    return 'loading'
  }

  return 'empty'
}

export function canNavigateRestoreBrowser({
  browserReady,
  loadingBrowser,
}) {
  return browserReady && !loadingBrowser
}

export function pushRestoreBreadcrumb(navStack, {
  label,
  pathId,
}) {
  return [
    ...navStack,
    { label, pathId },
  ]
}

export function truncateRestoreBreadcrumbs(navStack, index) {
  return navStack.slice(0, index + 1)
}

export function resolveRestoreSourceClient(clients, {
  clientName,
  directorName,
  currentDirector,
} = {}) {
  if (!clientName) {
    return null
  }

  const matches = clients.filter(client => client.name === clientName)
  if (matches.length === 0) {
    return null
  }

  if (directorName) {
    const exactMatch = matches.find(client => client.director === directorName)
    if (exactMatch) {
      return exactMatch
    }

    return null
  }

  if (currentDirector) {
    const currentMatch = matches.find(client => client.director === currentDirector)
    if (currentMatch) {
      return currentMatch
    }
  }

  return matches[0]
}

export function resolveRestoreSourceDirector(activeDirectors, directorName) {
  if (typeof directorName !== 'string' || !directorName) {
    return ''
  }

  return activeDirectors.includes(directorName) ? directorName : ''
}

export function filterRestoreSourceClients(clients, directorName) {
  if (!directorName) {
    return clients
  }

  return clients.filter(client => client.director === directorName)
}

export function buildRestoreSourceQuery(query, {
  clientName,
  directorName,
  jobid,
} = {}) {
  const nextQuery = { ...query }

  delete nextQuery.client
  delete nextQuery.director
  delete nextQuery.jobid

  if (clientName) {
    nextQuery.client = clientName
  }

  if (directorName) {
    nextQuery.director = directorName
  }

  if (jobid !== null && jobid !== undefined && jobid !== '') {
    nextQuery.jobid = String(jobid)
  }

  return nextQuery
}

export function buildRestorePluginFilesetMap(filesets) {
  const pluginFilesets = new Map()

  if (!filesets || typeof filesets !== 'object') {
    return pluginFilesets
  }

  for (const [name, fileset] of Object.entries(filesets)) {
    const filesetName = fileset?.name ?? fileset?.fileset ?? name
    const includeEntries = Array.isArray(fileset?.include) ? fileset.include : []
    const hasPlugin = includeEntries.some((entry) => {
      const plugin = entry?.plugin
      if (Array.isArray(plugin)) {
        return plugin.length > 0
      }

      return !!plugin
    })

    pluginFilesets.set(filesetName, hasPlugin)
  }

  return pluginFilesets
}

export function restoreBackupHasPluginOptions(backup) {
  return backup?.pluginjob === true
    || backup?.pluginjob === 1
    || backup?.pluginjob === '1'
}

export function decorateRestoreBackupsWithPluginJobs(backups, pluginFilesets) {
  return (backups ?? []).map((backup) => ({
    ...backup,
    pluginjob: restoreBackupHasPluginOptions(backup)
      || pluginFilesets.get(backup?.fileset) === true,
  }))
}

export function shouldShowRestorePluginOptions({
  backups,
  selectedJobId,
  mergedJobids,
  mergeJobsets,
}) {
  if (!selectedJobId) {
    return false
  }

  const mergedIds = typeof mergedJobids === 'string' && mergedJobids
    ? new Set(mergedJobids.split(',').filter(Boolean).map(String))
    : null

  return (backups ?? []).some((backup) => {
    if (!restoreBackupHasPluginOptions(backup)) {
      return false
    }

    if (mergeJobsets && mergedIds) {
      return mergedIds.has(String(backup?.jobid ?? ''))
    }

    return String(backup?.jobid ?? '') === String(selectedJobId)
  })
}

function restoreVersionKey(version) {
  if (version?.fileid !== null && version?.fileid !== undefined && version?.fileid !== '') {
    return `fileid:${version.fileid}`
  }

  return [
    version?.jobid ?? '',
    version?.stat?.mtime ?? '',
    version?.stat?.size ?? '',
    version?.volumename ?? '',
    version?.md5 ?? '',
  ].join('\u0000')
}

export function dedupeRestoreVersions(versions) {
  const seen = new Set()

  return (versions ?? []).filter((version) => {
    const key = restoreVersionKey(version)
    if (seen.has(key)) {
      return false
    }

    seen.add(key)
    return true
  })
}
