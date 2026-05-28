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

import { jobStatusMap } from '../mock/index.js'
import { quoteDirectorString } from './directorStrings.js'
import { resolveJobTypeCode } from './jobTypes.js'

const JOB_LEVEL_FILTERS = new Set(['F', 'I', 'D', 'V', 'B'])
const JOB_TYPE_FILTERS = new Set(['B', 'A', 'V', 'R', 'D', 'C', 'c', 'M', 'g', 'O', 'S', 'U', 'I'])

export function normaliseJobId(value) {
  if (typeof value === 'number') {
    return Number.isInteger(value) && value > 0 ? value : null
  }

  if (typeof value !== 'string') {
    return null
  }

  const trimmedValue = value.trim()
  if (!/^\d+$/.test(trimmedValue)) {
    return null
  }

  const jobId = Number(trimmedValue)
  return Number.isSafeInteger(jobId) && jobId > 0 ? jobId : null
}

export function normaliseJobStatusFilter(value) {
  if (typeof value !== 'string') {
    return ''
  }

  return Object.hasOwn(jobStatusMap, value) ? value : ''
}

export function normaliseJobLevelFilter(value) {
  if (typeof value !== 'string') {
    return ''
  }

  return JOB_LEVEL_FILTERS.has(value) ? value : ''
}

export function normaliseJobTypeFilter(value) {
  if (typeof value !== 'string') {
    return ''
  }

  const normalized = resolveJobTypeCode(value) ?? value
  return JOB_TYPE_FILTERS.has(normalized) ? normalized : ''
}

export function normaliseJobStatusFilters(value) {
  const rawValues = Array.isArray(value)
    ? value
    : (typeof value === 'string' ? value.split(',') : [])
  const seen = new Set()
  const filters = []

  for (const rawValue of rawValues) {
    const status = normaliseJobStatusFilter(
      typeof rawValue === 'string' ? rawValue.trim() : rawValue
    )
    if (!status || seen.has(status)) {
      continue
    }

    seen.add(status)
    filters.push(status)
  }

  return filters
}

export function normaliseJobLevelFilters(value) {
  const rawValues = Array.isArray(value)
    ? value
    : (typeof value === 'string' ? value.split(',') : [])
  const seen = new Set()
  const filters = []

  for (const rawValue of rawValues) {
    const level = normaliseJobLevelFilter(
      typeof rawValue === 'string' ? rawValue.trim() : rawValue
    )
    if (!level || seen.has(level)) {
      continue
    }

    seen.add(level)
    filters.push(level)
  }

  return filters
}

export function normaliseJobTypeFilters(value) {
  const rawValues = Array.isArray(value)
    ? value
    : (typeof value === 'string' ? value.split(',') : [])
  const seen = new Set()
  const filters = []

  for (const rawValue of rawValues) {
    const type = normaliseJobTypeFilter(
      typeof rawValue === 'string' ? rawValue.trim() : rawValue
    )
    if (!type || seen.has(type)) {
      continue
    }

    seen.add(type)
    filters.push(type)
  }

  return filters
}

export function encodeJobsStatusFilters(value) {
  return normaliseJobStatusFilters(value).join(',')
}

export function encodeJobsLevelFilters(value) {
  return normaliseJobLevelFilters(value).join(',')
}

export function encodeJobsTypeFilters(value) {
  return normaliseJobTypeFilters(value).join(',')
}

export function buildJobsFilterClause({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
} = {}) {
  const encodedStatus = encodeJobsStatusFilters(statusFilter)
  const encodedLevels = encodeJobsLevelFilters(levelFilter)
  const encodedTypes = encodeJobsTypeFilters(typeFilter)

  return `${encodedStatus ? ` jobstatus=${encodedStatus}` : ''}` +
    `${encodedLevels ? ` joblevel=${encodedLevels}` : ''}` +
    `${encodedTypes ? ` jobtype=${encodedTypes}` : ''}`
}

export function buildJobsFilterClauses({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
} = {}) {
  const statuses = normaliseJobStatusFilters(statusFilter)
  const sharedFilters = {
    levelFilter,
    typeFilter,
  }

  return statuses.length > 0
    ? statuses.map(currentStatus => buildJobsFilterClause({
      ...sharedFilters,
      statusFilter: currentStatus,
    }))
    : [buildJobsFilterClause(sharedFilters)]
}

export function buildListJobsCommand({
  limit,
  offset = 0,
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
} = {}) {
  return `llist jobs reverse limit=${limit} offset=${offset}` +
    buildJobsFilterClause({ statusFilter, levelFilter, typeFilter })
}

export function buildListJobsCountCommand({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
} = {}) {
  return `list jobs count${buildJobsFilterClause({ statusFilter, levelFilter, typeFilter })}`
}

export function buildListJobCommand(jobId) {
  return `llist jobid=${jobId}`
}

export function buildCancelJobCommand(jobId) {
  return `cancel jobid=${jobId} yes`
}

export function buildRerunJobCommand(jobId) {
  return `rerun jobid=${jobId} yes`
}

export function buildSetJobEnabledCommand(name, enabled) {
  return `${enabled ? 'enable' : 'disable'} job=${quoteDirectorString(name)} yes`
}

export function buildJobDefaultsCommand(name) {
  return `.defaults job=${quoteDirectorString(name)}`
}

export function buildRunJobCommand({
  job,
  client,
  fileset,
  pool,
  storage,
  level,
  when,
  priority,
} = {}) {
  let command = `run job=${quoteDirectorString(job)}`

  if (client) {
    command += ` client=${quoteDirectorString(client)}`
  }
  if (fileset) {
    command += ` fileset=${quoteDirectorString(fileset)}`
  }
  if (pool) {
    command += ` pool=${quoteDirectorString(pool)}`
  }
  if (storage) {
    command += ` storage=${quoteDirectorString(storage)}`
  }
  if (level) {
    command += ` level=${quoteDirectorString(level)}`
  }
  if (when) {
    command += ` when=${quoteDirectorString(when)}`
  }
  if (priority !== null && priority !== undefined && priority !== '') {
    command += ` priority=${priority}`
  }

  return `${command} yes`
}

export function resolveJobsStatusFilters(query) {
  return normaliseJobStatusFilters(query?.status)
}

export function resolveJobsLevelFilters(query) {
  return normaliseJobLevelFilters(query?.level)
}

export function resolveJobsTypeFilters(query) {
  return normaliseJobTypeFilters(query?.type)
}

export function withJobsStatusFilterQuery(query, status) {
  const nextQuery = { ...query }
  const nextStatus = encodeJobsStatusFilters(status)

  delete nextQuery.status
  if (nextStatus) {
    nextQuery.status = nextStatus
  }

  return nextQuery
}

export function withJobsLevelFilterQuery(query, level) {
  const nextQuery = { ...query }
  const nextLevel = encodeJobsLevelFilters(level)

  delete nextQuery.level
  if (nextLevel) {
    nextQuery.level = nextLevel
  }

  return nextQuery
}

export function withJobsTypeFilterQuery(query, type) {
  const nextQuery = { ...query }
  const nextType = encodeJobsTypeFilters(type)

  delete nextQuery.type
  if (nextType) {
    nextQuery.type = nextType
  }

  return nextQuery
}

export function resolveJobsSearchQuery(query) {
  if (typeof query?.search === 'string' && query.search) {
    return query.search
  }

  if (typeof query?.name === 'string' && query.name) {
    return query.name
  }

  return ''
}

export function withJobsSearchQuery(query, search) {
  const nextQuery = { ...query }
  const nextSearch = typeof search === 'string' ? search : ''

  delete nextQuery.search
  delete nextQuery.name

  if (nextSearch) {
    nextQuery.search = nextSearch
  }

  return nextQuery
}

export function buildJobDetailsQuery({
  director,
  jobsAction,
  jobsStatus,
  jobsLevel,
  jobsType,
  jobsSearch,
  clientName,
  clientDirector,
  clientsTab,
  clientsScopeDirector,
  clientDashboardOrigin,
  clientJobsAction,
  clientJobsStatus,
  clientJobsLevel,
  clientJobsType,
  clientJobsSearch,
  volumeName,
  volumeDirector,
  restoreClient,
  restoreDirector,
  restoreJobid,
  directorTab,
  directorTarget,
  dashboardOrigin,
} = {}) {
  const query = {}

  if (director) {
    query.director = director
  }

  if (jobsAction && jobsAction !== 'list') {
    query.jobsAction = jobsAction
  }

  const nextJobsStatus = encodeJobsStatusFilters(jobsStatus)
  if (nextJobsStatus) {
    query.jobsStatus = nextJobsStatus
  }

  const nextJobsLevel = encodeJobsLevelFilters(jobsLevel)
  if (nextJobsLevel) {
    query.jobsLevel = nextJobsLevel
  }

  const nextJobsType = encodeJobsTypeFilters(jobsType)
  if (nextJobsType) {
    query.jobsType = nextJobsType
  }

  if (jobsSearch) {
    query.jobsSearch = jobsSearch
  }

  if (clientName) {
    query.clientName = clientName
  }

  if (clientDirector) {
    query.clientDirector = clientDirector
  }

  if (clientsTab && clientsTab !== 'list') {
    query.clientsTab = clientsTab
  }

  if (clientsScopeDirector) {
    query.clientsScopeDirector = clientsScopeDirector
  }

  if (clientDashboardOrigin) {
    query.clientDashboardOrigin = '1'
  }

  if (clientJobsAction && clientJobsAction !== 'list') {
    query.clientJobsAction = clientJobsAction
  }

  const nextClientJobsStatus = encodeJobsStatusFilters(clientJobsStatus)
  if (nextClientJobsStatus) {
    query.clientJobsStatus = nextClientJobsStatus
  }

  const nextClientJobsLevel = encodeJobsLevelFilters(clientJobsLevel)
  if (nextClientJobsLevel) {
    query.clientJobsLevel = nextClientJobsLevel
  }

  const nextClientJobsType = encodeJobsTypeFilters(clientJobsType)
  if (nextClientJobsType) {
    query.clientJobsType = nextClientJobsType
  }

  if (clientJobsSearch) {
    query.clientJobsSearch = clientJobsSearch
  }

  if (volumeName) {
    query.volumeName = volumeName
  }

  if (volumeDirector) {
    query.volumeDirector = volumeDirector
  }

  if (restoreClient) {
    query.restoreClient = restoreClient
  }

  if (restoreDirector) {
    query.restoreDirector = restoreDirector
  }

  if (restoreJobid !== null && restoreJobid !== undefined && restoreJobid !== '') {
    query.restoreJobid = String(restoreJobid)
  }

  if (directorTab && directorTab !== 'status') {
    query.directorTab = directorTab
  }

  if (directorTarget) {
    query.directorTarget = directorTarget
  }

  if (dashboardOrigin) {
    query.dashboardOrigin = '1'
  }

  return query
}

export function resolveJobsListQuery(query) {
  const nextQuery = {}

  if (typeof query?.jobsAction === 'string' && query.jobsAction && query.jobsAction !== 'list') {
    nextQuery.action = query.jobsAction
  }

  const nextStatus = encodeJobsStatusFilters(query?.jobsStatus)
  if (nextStatus) {
    nextQuery.status = nextStatus
  }

  const nextLevel = encodeJobsLevelFilters(query?.jobsLevel)
  if (nextLevel) {
    nextQuery.level = nextLevel
  }

  const nextType = encodeJobsTypeFilters(query?.jobsType)
  if (nextType) {
    nextQuery.type = nextType
  }

  if (typeof query?.jobsSearch === 'string' && query.jobsSearch) {
    nextQuery.search = query.jobsSearch
  }

  return nextQuery
}

export function resolveJobDetailsQuery(query) {
  return buildJobDetailsQuery({
    director: typeof query?.director === 'string' ? query.director : '',
    jobsAction: typeof query?.jobsAction === 'string' ? query.jobsAction : '',
    jobsStatus: encodeJobsStatusFilters(query?.jobsStatus),
    jobsLevel: encodeJobsLevelFilters(query?.jobsLevel),
    jobsType: encodeJobsTypeFilters(query?.jobsType),
    jobsSearch: typeof query?.jobsSearch === 'string' ? query.jobsSearch : '',
    clientName: typeof query?.clientName === 'string' ? query.clientName : '',
    clientDirector: typeof query?.clientDirector === 'string' ? query.clientDirector : '',
    clientsTab: typeof query?.clientsTab === 'string' ? query.clientsTab : '',
    clientsScopeDirector: typeof query?.clientsScopeDirector === 'string' ? query.clientsScopeDirector : '',
    clientDashboardOrigin: query?.clientDashboardOrigin === '1',
    clientJobsAction: typeof query?.clientJobsAction === 'string' ? query.clientJobsAction : '',
    clientJobsStatus: encodeJobsStatusFilters(query?.clientJobsStatus),
    clientJobsLevel: encodeJobsLevelFilters(query?.clientJobsLevel),
    clientJobsType: encodeJobsTypeFilters(query?.clientJobsType),
    clientJobsSearch: typeof query?.clientJobsSearch === 'string' ? query.clientJobsSearch : '',
    volumeName: typeof query?.volumeName === 'string' ? query.volumeName : '',
    volumeDirector: typeof query?.volumeDirector === 'string' ? query.volumeDirector : '',
    restoreClient: typeof query?.restoreClient === 'string' ? query.restoreClient : '',
    restoreDirector: typeof query?.restoreDirector === 'string' ? query.restoreDirector : '',
    restoreJobid: typeof query?.restoreJobid === 'string' ? query.restoreJobid : '',
    directorTab: typeof query?.directorTab === 'string' ? query.directorTab : '',
    directorTarget: typeof query?.directorTarget === 'string' ? query.directorTarget : '',
    dashboardOrigin: query?.dashboardOrigin === '1',
  })
}

export function resolveJobDetailsClientOrigin(query) {
  if (typeof query?.clientName !== 'string' || !query.clientName) {
    return null
  }

  return {
    name: query.clientName,
    director: typeof query?.clientDirector === 'string' ? query.clientDirector : '',
    clientsTab: typeof query?.clientsTab === 'string' ? query.clientsTab : '',
    scopeDirector: typeof query?.clientsScopeDirector === 'string' ? query.clientsScopeDirector : '',
    dashboardOrigin: query?.clientDashboardOrigin === '1',
    jobsAction: typeof query?.clientJobsAction === 'string' ? query.clientJobsAction : '',
    jobsStatus: encodeJobsStatusFilters(query?.clientJobsStatus),
    jobsLevel: encodeJobsLevelFilters(query?.clientJobsLevel),
    jobsType: encodeJobsTypeFilters(query?.clientJobsType),
    jobsSearch: typeof query?.clientJobsSearch === 'string' ? query.clientJobsSearch : '',
  }
}

export function resolveJobDetailsVolumeOrigin(query) {
  if (typeof query?.volumeName !== 'string' || !query.volumeName) {
    return null
  }

  return {
    name: query.volumeName,
    director: typeof query?.volumeDirector === 'string' ? query.volumeDirector : '',
  }
}

export function resolveJobDetailsRestoreOrigin(query) {
  if (typeof query?.restoreClient !== 'string' || !query.restoreClient) {
    return null
  }

  return {
    client: query.restoreClient,
    director: typeof query?.restoreDirector === 'string' ? query.restoreDirector : '',
    jobid: typeof query?.restoreJobid === 'string' ? query.restoreJobid : '',
  }
}

export function resolveJobDetailsDirectorOrigin(query) {
  if (typeof query?.directorTab !== 'string' || !query.directorTab) {
    return null
  }

  return {
    tab: query.directorTab,
    targetDirector: typeof query?.directorTarget === 'string' ? query.directorTarget : '',
  }
}

export function resolveJobDetailsDashboardOrigin(query) {
  return query?.dashboardOrigin === '1'
}
