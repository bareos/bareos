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

export function buildRestoreBackupOption(
  backup,
  {
    formatBytes = value => String(value),
    filesLabel = 'files',
  } = {}
) {
  const jobid = backup?.jobid ?? ''
  const name = backup?.name ?? ''
  const level = backup?.level ?? ''
  const starttime = backup?.starttime ?? ''
  const jobbytes = Number(backup?.jobbytes ?? 0)
  const jobfiles = Number(backup?.jobfiles ?? 0)
  const secondary = [
    jobid !== '' ? `#${jobid}` : '',
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
    secondary,
  }
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
      pluginNames: [...new Set(definitions.map(definition => definition.pluginName).filter(Boolean))],
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

export function getRestorePluginInfo({
  backups,
  pluginFilesets,
  selectedJobId,
  mergedJobids,
  mergeJobsets,
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

    if (mergeJobsets && mergedIds) {
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
    usesMergedJobs: mergeJobsets
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
