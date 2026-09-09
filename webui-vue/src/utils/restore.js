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

import { restorePluginHints } from '../data/restorePluginHints.js'
import { resolveJobLevelCode } from './jobLevels.js'

export function getRestoreBrowserPlaceholder({
  browserError,
  loadingBrowser,
  buildingCache,
  hasSelectedJob,
}) {
  if (browserError) {
    return 'error'
  }

  if (buildingCache && hasSelectedJob) {
    return 'building-cache'
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
  filesetName,
  jobid,
  mergeJobs,
  mergeFilesets,
  sourceMode,
} = {}) {
  const nextQuery = { ...query }

  delete nextQuery.client
  delete nextQuery.director
  delete nextQuery.fileset
  delete nextQuery.jobid
  delete nextQuery.mergejobs
  delete nextQuery.mergefilesets
  delete nextQuery.mode

  if (clientName) {
    nextQuery.client = clientName
  }

  if (directorName) {
    nextQuery.director = directorName
  }

  if (filesetName) {
    nextQuery.fileset = filesetName
  }

  if (jobid !== null && jobid !== undefined && jobid !== '') {
    nextQuery.jobid = String(jobid)
  }

  if (typeof mergeJobs === 'boolean') {
    nextQuery.mergejobs = mergeJobs ? '1' : '0'
  }

  if (typeof mergeFilesets === 'boolean') {
    nextQuery.mergefilesets = mergeFilesets ? '1' : '0'
  }

  if (sourceMode === 'latest' || sourceMode === 'browse') {
    nextQuery.mode = sourceMode
  }

  return nextQuery
}

export function normaliseRestoreToggle(value, defaultValue = false) {
  if (typeof value === 'boolean') {
    return value
  }

  if (typeof value === 'number') {
    return value !== 0
  }

  if (typeof value !== 'string') {
    return defaultValue
  }

  const normalizedValue = value.trim().toLowerCase()
  if (['1', 'true', 'yes', 'on'].includes(normalizedValue)) {
    return true
  }

  if (['0', 'false', 'no', 'off'].includes(normalizedValue)) {
    return false
  }

  return defaultValue
}

export function buildRestoreBvfsJobidsCommand(jobid, {
  mergeJobs = true,
  mergeFilesets = true,
} = {}) {
  if (jobid === null || jobid === undefined || jobid === '') {
    return ''
  }

  if (!mergeJobs) {
    return ''
  }

  return `.bvfs_get_jobids jobid=${jobid}${mergeFilesets ? ' all' : ''}`
}

export function buildRestoreBvfsRestoreCommand({
  jobids,
  fileids = '',
  dirids = '',
  path,
}) {
  const parts = [`.bvfs_restore jobid=${jobids}`]

  if (fileids) {
    parts.push(`fileid=${fileids}`)
  }

  if (dirids) {
    parts.push(`dirid=${dirids}`)
  }

  parts.push(`path=${path}`)

  return parts.join(' ')
}

export function buildRestoreBackupOption(
  backup,
  {
    formatBytes = value => String(value),
    filesLabel = 'files',
    showClient = false,
  } = {}
) {
  const jobid = backup?.jobid ?? ''
  const name = backup?.name ?? ''
  const level = backup?.level ?? ''
  const starttime = backup?.starttime ?? ''
  const jobbytes = Number(backup?.jobbytes ?? 0)
  const jobfiles = Number(backup?.jobfiles ?? 0)
  const client = backup?.client ?? ''
  const fileset = backup?.fileset ?? ''
  const secondary = [
    jobid !== '' ? `#${jobid}` : '',
    showClient && client ? client : '',
    starttime,
    Number.isFinite(jobbytes) && jobbytes > 0 ? formatBytes(jobbytes) : '',
    Number.isFinite(jobfiles) && jobfiles > 0 ? `${jobfiles} ${filesLabel}` : '',
  ].filter(Boolean).join(' · ')

  return {
    value: jobid,
    label: name,
    name,
    jobid,
    level,
    levelCode: resolveJobLevelCode(level),
    starttime,
    client,
    fileset,
    secondary,
  }
}

// Narrows a list of raw backup job records (as returned by either a
// per-client `llist backups` call or the client-agnostic `llist jobs`
// browse-all-clients fallback) down to the ones matching the optional
// fileset name and "at or before" start-time filters used by the restore
// Source panel. `beforeFilter` is compared as a string prefix against the
// catalog's `YYYY-MM-DD HH:MM:SS` starttime so a plain date (`YYYY-MM-DD`)
// or a full timestamp both work.
export function filterRestoreBackupsByCriteria(backups, {
  filesetFilter = '',
  beforeFilter = '',
} = {}) {
  const list = Array.isArray(backups) ? backups : []
  const normalizedFileset = typeof filesetFilter === 'string' ? filesetFilter.trim() : ''
  const normalizedBefore = typeof beforeFilter === 'string' ? beforeFilter.trim() : ''

  return list.filter((backup) => {
    if (normalizedFileset && String(backup?.fileset ?? '') !== normalizedFileset) {
      return false
    }

    if (normalizedBefore) {
      const starttime = String(backup?.starttime ?? '')
      if (!starttime || starttime > normalizedBefore) {
        return false
      }
    }

    return true
  })
}

export function buildRestoreClientFilesetOptions(backups) {
  const tuples = new Map()

  for (const backup of Array.isArray(backups) ? backups : []) {
    const client = String(backup?.client ?? '').trim()
    const fileset = String(backup?.fileset ?? '').trim()
    if (!client || !fileset) {
      continue
    }

    const key = `${client}\u0000${fileset}`
    const starttime = String(backup?.starttime ?? '')
    const previous = tuples.get(key)
    if (!previous || starttime > previous.latestStarttime) {
      tuples.set(key, {
        value: key,
        label: `${fileset}@${client}`,
        client,
        fileset,
        latestStarttime: starttime,
      })
    }
  }

  return [...tuples.values()].sort((left, right) => (
    left.client.localeCompare(right.client)
    || left.fileset.localeCompare(right.fileset)
  ))
}

export function buildRestoreBackupChainOptions(
  backups,
  jobids,
  {
    formatBytes = value => String(value),
  } = {}
) {
  const ids = Array.isArray(jobids)
    ? jobids
    : (typeof jobids === 'string' ? jobids.split(',') : [])
  const wantedJobIds = new Set(
    ids
      .map(jobid => String(jobid ?? '').trim())
      .filter(Boolean)
  )

  if (wantedJobIds.size === 0 || !Array.isArray(backups)) {
    return []
  }

  return backups
    .filter(backup => wantedJobIds.has(String(backup?.jobid ?? '').trim()))
    .map(backup => buildRestoreBackupOption(backup, { formatBytes }))
    .sort((left, right) => (
      left.starttime.localeCompare(right.starttime)
      || Number(left.jobid) - Number(right.jobid)
    ))
}

export function buildRestoreTimelinePoints(
  backups,
  {
    filesetFilter = '',
    formatBytes = value => String(value),
  } = {}
) {
  return filterRestoreBackupsByCriteria(backups, { filesetFilter })
    .map(backup => buildRestoreBackupOption(backup, { formatBytes }))
    .sort((left, right) => (
      left.starttime.localeCompare(right.starttime)
      || Number(left.jobid) - Number(right.jobid)
    ))
}

export function resolveRestoreTimelineSelection(points, selectedJobid) {
  if (!Array.isArray(points) || points.length === 0) {
    return null
  }

  if (selectedJobid !== null && selectedJobid !== undefined && selectedJobid !== '') {
    const selected = points.find(point => (
      String(point?.jobid ?? '') === String(selectedJobid)
    ))
    if (selected) {
      return selected
    }
  }

  return points[points.length - 1] ?? null
}

export function buildRestoreBackupChains(
  backups,
  {
    filesetFilter = '',
    formatBytes = value => String(value),
  } = {}
) {
  const points = buildRestoreTimelinePoints(backups, {
    filesetFilter,
    formatBytes,
  })
  const chains = []
  let currentChain = null

  for (const point of points) {
    const startsChain = resolveJobLevelCode(point.level) === 'F' || !currentChain
    if (startsChain) {
      currentChain = {
        value: String(point.jobid ?? `chain-${chains.length}`),
        label: point.starttime ? `Full #${point.jobid} · ${point.starttime}` : `Full #${point.jobid}`,
        rootJobid: point.jobid,
        rootStarttime: point.starttime,
        jobs: [],
      }
      chains.push(currentChain)
    }

    currentChain.jobs.push(point)
  }

  return chains.map((chain, index) => {
    const latestJob = chain.jobs[chain.jobs.length - 1] ?? null
    return {
      ...chain,
      index,
      latestJobid: latestJob?.jobid ?? null,
      latestStarttime: latestJob?.starttime ?? chain.rootStarttime,
      jobCount: chain.jobs.length,
    }
  })
}

export function resolveRestoreBackupChain(chains, selectedJobid) {
  if (!Array.isArray(chains) || chains.length === 0) {
    return null
  }

  if (selectedJobid !== null && selectedJobid !== undefined && selectedJobid !== '') {
    const selectedChain = chains.find(chain => (
      Array.isArray(chain?.jobs)
      && chain.jobs.some(job => String(job?.jobid ?? '') === String(selectedJobid))
    ))
    if (selectedChain) {
      return selectedChain
    }
  }

  return chains[chains.length - 1] ?? null
}

export function resolveAdjacentRestoreBackupChain(chains, currentChain, direction) {
  if (!Array.isArray(chains) || chains.length === 0 || !currentChain) {
    return null
  }

  const currentIndex = chains.findIndex(chain => chain?.value === currentChain?.value)
  if (currentIndex === -1) {
    return null
  }

  const offset = direction === 'older' ? -1 : 1
  return chains[currentIndex + offset] ?? null
}

// Resolves the newest backup job matching a client's already-loaded backup
// list, narrowed to a specific fileset (required -- restoring "the latest
// backup" only makes sense for a single client+fileset tuple) and an
// optional "at or before" date, for the Restore page's "Latest Backup"
// selection mode. Returns the jobid, or null if no fileset filter is given
// or nothing matches. Backups are compared by starttime (falling back to
// jobid as a tie-breaker), independent of any pre-existing sort order in
// the input list.
export function resolveLatestRestoreBackup(backups, {
  filesetFilter = '',
  beforeFilter = '',
} = {}) {
  const normalizedFileset = typeof filesetFilter === 'string' ? filesetFilter.trim() : ''
  if (!normalizedFileset) {
    return null
  }

  const matches = filterRestoreBackupsByCriteria(backups, {
    filesetFilter: normalizedFileset,
    beforeFilter,
  })

  if (matches.length === 0) {
    return null
  }

  const sortKey = backup => (
    `${String(backup?.starttime ?? '')}#${String(backup?.jobid ?? '').padStart(12, '0')}`
  )

  const latest = matches.reduce((best, candidate) => (
    sortKey(candidate) > sortKey(best) ? candidate : best
  ))

  return latest?.jobid ?? null
}

// True if a Full-level backup exists for the given fileset at or before the
// given start time -- used by the Restore page's "Latest Backup" mode to
// decide whether to show a "no Full backup found in this chain" warning.
// The actual restore chain merging (.bvfs_get_jobids ... all) is still
// performed server-side exactly as for a manually-picked job; this is only
// a best-effort, client-side hint for the user.
export function hasRestoreFullBackupInChain(backups, {
  filesetFilter = '',
  uptoStarttime = '',
} = {}) {
  const matches = filterRestoreBackupsByCriteria(backups, {
    filesetFilter,
    beforeFilter: uptoStarttime,
  })

  return matches.some(backup => resolveJobLevelCode(backup?.level) === 'F')
}

// Builds the sorted, de-duplicated `{ label, value }` options for the
// restore Source panel's fileset filter from the catalog's `list filesets`
// response.
export function buildRestoreFilesetOptions(filesets) {
  const names = new Set()

  for (const [, fileset] of normalizeRestoreFilesetEntries(filesets)) {
    const filesetName = fileset?.name ?? ''
    if (filesetName) {
      names.add(filesetName)
    }
  }

  return [...names]
    .sort((left, right) => left.localeCompare(right))
    .map(name => ({ label: name, value: name }))
}

export function resolveRestoreBackupOption(options, jobid) {
  if (!Array.isArray(options) || jobid === null || jobid === undefined || jobid === '') {
    return null
  }

  const normalizedJobid = String(jobid)
  return options.find(option => String(option?.value ?? '') === normalizedJobid) ?? null
}

function extractRestoreFilesetDescription(filesetText) {
  const match = String(filesetText ?? '').match(/^\s*Description\s*=\s*"([^"]*)"/m)
  return match?.[1] ?? ''
}

function extractRestorePluginDefinitionsFromFilesetText(filesetText) {
  const lines = String(filesetText ?? '').split('\n')
  const definitions = []
  let currentDefinition = null

  for (const line of lines) {
    const pluginMatch = line.match(/^\s*Plugin\s*=\s*"([^"]*)"/)
    if (pluginMatch) {
      if (currentDefinition) {
        definitions.push(currentDefinition)
      }
      currentDefinition = pluginMatch[1]
      continue
    }

    const continuationMatch = line.match(/^\s*"([^"]*)"/)
    if (continuationMatch && currentDefinition !== null) {
      currentDefinition += continuationMatch[1]
      continue
    }

    if (currentDefinition) {
      definitions.push(currentDefinition)
      currentDefinition = null
    }
  }

  if (currentDefinition) {
    definitions.push(currentDefinition)
  }

  return definitions
}

function normalizeRestoreFilesetEntries(filesets) {
  if (!filesets || typeof filesets !== 'object') {
    return []
  }

  if (Array.isArray(filesets)) {
    return filesets.map((fileset) => {
      const filesetName = fileset?.name ?? fileset?.fileset ?? ''
      const pluginDefinitions = extractRestorePluginDefinitionsFromFilesetText(fileset?.filesettext)

      return [filesetName, {
        ...fileset,
        name: filesetName,
        description: fileset?.description ?? extractRestoreFilesetDescription(fileset?.filesettext),
        include: pluginDefinitions.length > 0 ? [{ plugin: pluginDefinitions }] : [],
      }]
    })
  }

  return Object.entries(filesets)
}

export function buildRestorePluginFilesetMap(filesets) {
  const pluginFilesets = new Map()

  for (const [name, fileset] of normalizeRestoreFilesetEntries(filesets)) {
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

export function parseRestorePluginDefinition(definition) {
  const raw = String(definition ?? '').trim()
  const separatorIndex = raw.indexOf(':')
  const pluginName = separatorIndex === -1 ? raw : raw.slice(0, separatorIndex)
  const optionKeys = [...raw.matchAll(/:([A-Za-z0-9_.-]+)=/g)]
    .map((match) => match[1])
    .filter(Boolean)

  return {
    raw,
    pluginName,
    optionKeys: [...new Set(optionKeys)],
  }
}

function normalizeRestorePluginHintAlias(value) {
  return String(value ?? '').trim().toLowerCase()
}

function extractRestorePluginDefinitionOption(definition, key) {
  const raw = String(definition?.raw ?? '')
  const match = raw.match(new RegExp(`:${key}=([^:]+)`, 'i'))
  return match?.[1]?.trim() ?? ''
}

const restorePluginHintAliases = new Map(
  Object.entries(restorePluginHints).flatMap(([hintId, hint]) => ([
    [normalizeRestorePluginHintAlias(hintId), hintId],
    ...(hint.aliases ?? []).map(alias => [normalizeRestorePluginHintAlias(alias), hintId]),
  ]))
)

export function resolveRestorePluginHintId(definition) {
  const moduleName = extractRestorePluginDefinitionOption(definition, 'module_name')
  const moduleMatch = restorePluginHintAliases.get(
    normalizeRestorePluginHintAlias(moduleName)
  )
  if (moduleMatch) {
    return moduleMatch
  }

  const directPluginName = normalizeRestorePluginHintAlias(definition?.pluginName)
  const directMatch = restorePluginHintAliases.get(directPluginName)
  if (directMatch) {
    return directMatch
  }

  return null
}

// Resolves a human-readable plugin name for display, preferring the
// module_name-derived hint (e.g. "VMware") over the generic plugin loader
// name (e.g. "bpipe", "python-fd", "grpc") that the FileSet actually invokes.
export function resolveRestorePluginDisplayName(definition) {
  const hintId = resolveRestorePluginHintId(definition)
  return (hintId && restorePluginHints[hintId]?.displayName) || definition?.pluginName || ''
}

export function buildRestorePluginOptionExample(pluginHint) {
  const options = Array.isArray(pluginHint?.options) ? pluginHint.options : []
  const preferredOptions = options.filter(option => option.status === 'required')
  const exampleOptions = (preferredOptions.length > 0 ? preferredOptions : options)
    .slice(0, 2)
    .map(option => `${option.name}=...`)

  return exampleOptions.join(pluginHint?.optionSeparator ?? ':')
}

export function getRestorePluginHints(pluginInfo) {
  if (!pluginInfo) {
    return []
  }

  const hintIds = [...new Set(
    (pluginInfo.definitions ?? [])
      .map(resolveRestorePluginHintId)
      .filter(Boolean)
  )]

  return hintIds.map((hintId) => ({
    id: hintId,
    ...restorePluginHints[hintId],
    example: buildRestorePluginOptionExample(restorePluginHints[hintId]),
  }))
}

export function getAllRestorePluginHints() {
  return Object.entries(restorePluginHints)
    .map(([hintId, hint]) => ({
      id: hintId,
      ...hint,
      example: buildRestorePluginOptionExample(hint),
    }))
    .sort((left, right) => left.displayName.localeCompare(right.displayName))
}

export function buildRestorePluginFilesetDetails(filesets) {
  const pluginFilesets = new Map()

  for (const [name, fileset] of normalizeRestoreFilesetEntries(filesets)) {
    const filesetName = fileset?.name ?? fileset?.fileset ?? name
    const includeEntries = Array.isArray(fileset?.include) ? fileset.include : []
    const definitions = includeEntries
      .flatMap((entry) => {
        if (Array.isArray(entry?.plugin)) {
          return entry.plugin
        }

        return entry?.plugin ? [entry.plugin] : []
      })
      .map(parseRestorePluginDefinition)

    pluginFilesets.set(filesetName, {
      filesetName,
      description: fileset?.description ?? '',
      hasPlugin: definitions.length > 0,
      definitions,
      pluginNames: [...new Set(definitions.map(resolveRestorePluginDisplayName).filter(Boolean))],
      optionKeys: [...new Set(definitions.flatMap(definition => definition.optionKeys))],
    })
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
  mergeJobs,
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

    if (mergeJobs && mergedIds) {
      return mergedIds.has(String(backup?.jobid ?? ''))
    }

    return String(backup?.jobid ?? '') === String(selectedJobId)
  })
}

export function getRestorePluginInfo({
  backups,
  pluginFilesets,
  selectedJobId,
  mergedJobids,
  mergeJobs,
}) {
  if (!selectedJobId) {
    return null
  }

  const mergedIds = typeof mergedJobids === 'string' && mergedJobids
    ? new Set(mergedJobids.split(',').filter(Boolean).map(String))
    : null

  const relevantBackups = (backups ?? []).filter((backup) => {
    if (!restoreBackupHasPluginOptions(backup)) {
      return false
    }

    if (mergeJobs && mergedIds) {
      return mergedIds.has(String(backup?.jobid ?? ''))
    }

    return String(backup?.jobid ?? '') === String(selectedJobId)
  })

  if (relevantBackups.length === 0) {
    return null
  }

  const filesetDetails = [...new Map(
    relevantBackups.map((backup) => [backup?.fileset, pluginFilesets.get(backup?.fileset)])
  ).values()].filter(detail => detail?.hasPlugin)

  if (filesetDetails.length === 0) {
    return null
  }

  const definitions = [...new Map(
    filesetDetails.flatMap((detail) => detail.definitions.map((definition) => [
      definition.raw,
      {
        ...definition,
        filesetName: detail.filesetName,
        filesetDescription: detail.description,
      },
    ]))
  ).values()]

  return {
    backups: relevantBackups,
    filesetNames: [...new Set(filesetDetails.map(detail => detail.filesetName))],
    pluginNames: [...new Set(filesetDetails.flatMap(detail => detail.pluginNames))],
    optionKeys: [...new Set(filesetDetails.flatMap(detail => detail.optionKeys))],
    definitions,
    usesMergedJobs: mergeJobs
      && relevantBackups.some(backup => String(backup?.jobid ?? '') !== String(selectedJobId)),
  }
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

function normaliseRestoreVersionJobIds(jobids) {
  const rawJobIds = Array.isArray(jobids)
    ? jobids
    : (typeof jobids === 'string' ? jobids.split(',') : [])

  return new Set(
    rawJobIds
      .map(jobid => String(jobid ?? '').trim())
      .filter(Boolean)
  )
}

export function filterRestoreVersionsByJobids(versions, jobids) {
  const dedupedVersions = dedupeRestoreVersions(versions)
  const allowedJobIds = normaliseRestoreVersionJobIds(jobids)

  if (allowedJobIds.size === 0) {
    return dedupedVersions
  }

  return dedupedVersions.filter(version => allowedJobIds.has(String(version?.jobid ?? '').trim()))
}

export function getRestoreVersionsLookupJobId(selectedJobId, mergedJobids) {
  if (selectedJobId !== null && selectedJobId !== undefined && selectedJobId !== '') {
    return String(selectedJobId).trim()
  }

  if (typeof mergedJobids !== 'string') {
    return '0'
  }

  const firstMergedJobId = mergedJobids
    .split(',')
    .map(jobid => String(jobid ?? '').trim())
    .find(Boolean)

  return firstMergedJobId || '0'
}
