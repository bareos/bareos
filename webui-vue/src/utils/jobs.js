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
import { isErrorJobStatus, isWarningJobStatus, isOkJobStatus } from '../composables/useDirectorFetch.js'

// Upper bound on the number of jobs fetched for a single "fetch everything"
// request (e.g. sorting by a non-default column, a free-text search, or the
// "All" rows-per-page option). Without a cap, a director with a very large
// job history would ship its entire Job table to the browser in one call.
export const MAX_JOBS_FETCH_LIMIT = 1000

const JOB_LEVEL_FILTERS = new Set(['F', 'I', 'D', 'V', 'B'])
const JOB_TYPE_FILTERS = new Set(['B', 'A', 'V', 'R', 'D', 'C', 'c', 'M', 'g', 'O', 'S', 'U', 'I'])

// The director can only reschedule these job types, see IsRerunableJobType()
// in core/src/include/job_types.h. Offering a rerun for any other type would
// silently do nothing.
const RERUNABLE_JOB_TYPES = new Set(['B', 'c', 'g'])

// Resolve which part of a job's log should be jumped to, based on its
// status: the first error/warning line, or the final termination summary
// line for a successful job. Returns '' when there is no specific target
// (e.g. Running/Waiting/Canceled), meaning the caller should fall back to
// the default "scroll to bottom" behavior.
export function resolveJobLogFocus(status) {
  if (isErrorJobStatus(status)) return 'error'
  if (isWarningJobStatus(status)) return 'warning'
  if (isOkJobStatus(status)) return 'ok'
  return ''
}

// Classify a single job log line by severity, for highlighting and for
// extracting error/warning lines (e.g. Trouble View widget, Job Details
// log highlighting). Order matters: the "0 errors/warnings" summary guard
// must run before the generic error/warning checks below it.
//
// The warning pattern covers Bareos' M_NOTSAVED file daemon messages
// (backup.cc), which cause a job's Warning status without containing the
// word "warning" itself (e.g. "Could not stat ...", "Cannot open ...").
// "Security violation" (M_SECURITY) and tape "Alert: ..." (M_ALERT)
// messages are also classified as warnings so they remain visible, even
// though they don't increment the job's warning/error counters.
export function classifyLogLine(line) {
  const l = line.toLowerCase()
  if (/\b(?:non-fatal\s+fd\s+errors|sd\s+errors|fd\s+errors|errors|warnings?)\s*:\s*0\b/.test(l)) {
    return 'normal'
  }
  if (/error|fatal|failed/.test(l)) return 'error'
  if (
    /warning|warn|could not stat|could not access|not saved|unknown file type/.test(l)
    || /could not follow link|could not open directory|cannot open/.test(l)
    || /security violation|\balert:/.test(l)
  ) {
    return 'warning'
  }
  if (/\bok\b|termination:.*ok|backup ok/.test(l)) return 'ok'
  return 'normal'
}

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

export function canRerunJob(job) {
  return RERUNABLE_JOB_TYPES.has(resolveJobTypeCode(job?.type))
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

export function normaliseJobsTextFilter(value) {
  return typeof value === 'string' ? value.trim() : ''
}

export function normaliseJobsTextOptions(values) {
  const seen = new Set()
  const options = []

  for (const value of values ?? []) {
    const normalized = normaliseJobsTextFilter(value)
    const lookupKey = normalized.toLowerCase()
    if (!normalized || seen.has(lookupKey)) {
      continue
    }

    seen.add(lookupKey)
    options.push(normalized)
  }

  return options.sort((left, right) => left.localeCompare(right))
}

export function filterJobsTextOptions(values, searchTerm = '', limit = 20) {
  const normalizedOptions = normaliseJobsTextOptions(values)
  const normalizedSearchTerm = normaliseJobsTextFilter(searchTerm).toLowerCase()
  const filteredOptions = normalizedSearchTerm
    ? normalizedOptions.filter(option => option.toLowerCase().includes(normalizedSearchTerm))
    : normalizedOptions

  return filteredOptions.slice(0, Math.max(limit, 0))
}

export function buildJobsFilterClause({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
  jobFilter = '',
  clientFilter = '',
} = {}) {
  const encodedStatus = encodeJobsStatusFilters(statusFilter)
  const encodedLevels = encodeJobsLevelFilters(levelFilter)
  const encodedTypes = encodeJobsTypeFilters(typeFilter)
  const normalizedJob = normaliseJobsTextFilter(jobFilter)
  const normalizedClient = normaliseJobsTextFilter(clientFilter)

  return `${encodedStatus ? ` jobstatus=${encodedStatus}` : ''}` +
    `${encodedLevels ? ` joblevel=${encodedLevels}` : ''}` +
    `${encodedTypes ? ` jobtype=${encodedTypes}` : ''}` +
    `${normalizedJob ? ` job=${quoteDirectorString(normalizedJob)}` : ''}` +
    `${normalizedClient ? ` client=${quoteDirectorString(normalizedClient)}` : ''}`
}

export function buildJobsFilterClauses({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
  jobFilter = '',
  clientFilter = '',
} = {}) {
  const statuses = normaliseJobStatusFilters(statusFilter)
  const sharedFilters = {
    levelFilter,
    typeFilter,
    jobFilter,
    clientFilter,
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
  jobFilter = '',
  clientFilter = '',
} = {}) {
  return `llist jobs reverse limit=${limit} offset=${offset}` +
    buildJobsFilterClause({ statusFilter, levelFilter, typeFilter, jobFilter, clientFilter })
}

export function buildListJobsCountCommand({
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
  jobFilter = '',
  clientFilter = '',
} = {}) {
  return `list jobs count${buildJobsFilterClause({
    statusFilter,
    levelFilter,
    typeFilter,
    jobFilter,
    clientFilter,
  })}`
}

export function buildListJobCommand(jobId) {
  return `llist jobid=${jobId}`
}

export function normaliseJobsSearchTerm(value) {
  return typeof value === 'string' ? value.trim() : ''
}

export function filterJobsBySearch(jobs, searchTerm) {
  const query = normaliseJobsSearchTerm(searchTerm).toLowerCase()
  if (!query) {
    return jobs
  }

  return jobs.filter((job) => {
    const jobId = String(job.id ?? '').toLowerCase()
    const jobName = String(job.name ?? '').toLowerCase()
    const clientName = String(job.client ?? '').toLowerCase()

    return jobId.includes(query) || jobName.includes(query) || clientName.includes(query)
  })
}

export function paginateJobs(jobs, pagination) {
  const { page = 1, rowsPerPage = 25 } = pagination ?? {}
  if (rowsPerPage === 0) {
    return jobs
  }

  const offset = Math.max(0, (page - 1) * rowsPerPage)
  return jobs.slice(offset, offset + rowsPerPage)
}

export function mergeJobMediaByVolume(jobmediaRows) {
  if (!Array.isArray(jobmediaRows)) {
    return []
  }

  const groupedByVolume = new Map()

  for (const row of jobmediaRows) {
    const volumeName = typeof row?.volumename === 'string'
      ? row.volumename.trim()
      : ''
    if (!volumeName) {
      continue
    }

    const existing = groupedByVolume.get(volumeName)
    if (existing) {
      existing.segments += 1
      continue
    }

    const {
      firstindex: _firstindex,
      lastindex: _lastindex,
      ...rowWithoutIndexes
    } = row ?? {}

    groupedByVolume.set(volumeName, {
      ...rowWithoutIndexes,
      volumename: volumeName,
      segments: 1,
    })
  }

  return [...groupedByVolume.values()]
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

export function resolvePermittedRunJobDefault(options, value) {
  if (typeof value !== 'string') {
    return null
  }

  const normalizedValue = value.trim()
  if (!normalizedValue) {
    return null
  }

  return Array.isArray(options) && options.includes(normalizedValue)
    ? normalizedValue
    : null
}

export function filterRunnableJobOptions(jobOptions, restoreJobOptions) {
  const restoreJobs = new Set(normaliseJobsTextOptions(restoreJobOptions))
  return normaliseJobsTextOptions(jobOptions).filter(name => !restoreJobs.has(name))
}

export function formatRunWhenPickerDate(date) {
  if (!(date instanceof Date) || Number.isNaN(date.getTime())) {
    return ''
  }

  return [
    date.getFullYear().toString().padStart(4, '0'),
    (date.getMonth() + 1).toString().padStart(2, '0'),
    date.getDate().toString().padStart(2, '0'),
  ].join('-') + ' ' + [
    date.getHours().toString().padStart(2, '0'),
    date.getMinutes().toString().padStart(2, '0'),
    date.getSeconds().toString().padStart(2, '0'),
  ].join(':')
}

export function resolveRunWhenPickerValue(value, fallbackDate = new Date()) {
  if (typeof value === 'string') {
    const normalizedValue = value.trim()
    if (/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/.test(normalizedValue)) {
      return normalizedValue
    }
  }

  return formatRunWhenPickerDate(fallbackDate)
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

export function resolveJobsJobQuery(query) {
  return normaliseJobsTextFilter(query?.job)
}

export function resolveJobsClientQuery(query) {
  return normaliseJobsTextFilter(query?.client)
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

export function withJobsJobQuery(query, job) {
  const nextQuery = { ...query }
  const nextJob = normaliseJobsTextFilter(job)

  delete nextQuery.job
  if (nextJob) {
    nextQuery.job = nextJob
  }

  return nextQuery
}

export function withJobsClientQuery(query, client) {
  const nextQuery = { ...query }
  const nextClient = normaliseJobsTextFilter(client)

  delete nextQuery.client
  if (nextClient) {
    nextQuery.client = nextClient
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
  jobsJob,
  jobsClient,
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
  clientJobsJob,
  clientJobsClient,
  clientJobsSearch,
  volumeName,
  volumeDirector,
  restoreClient,
  restoreDirector,
  restoreJobid,
  directorTab,
  directorTarget,
  dashboardOrigin,
  logFocus,
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

  const nextJobsJob = normaliseJobsTextFilter(jobsJob)
  if (nextJobsJob) {
    query.jobsJob = nextJobsJob
  }

  const nextJobsClient = normaliseJobsTextFilter(jobsClient)
  if (nextJobsClient) {
    query.jobsClient = nextJobsClient
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

  const nextClientJobsJob = normaliseJobsTextFilter(clientJobsJob)
  if (nextClientJobsJob) {
    query.clientJobsJob = nextClientJobsJob
  }

  const nextClientJobsClient = normaliseJobsTextFilter(clientJobsClient)
  if (nextClientJobsClient) {
    query.clientJobsClient = nextClientJobsClient
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

  if (logFocus === 'error' || logFocus === 'warning' || logFocus === 'ok') {
    query.logFocus = logFocus
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

  const nextJob = normaliseJobsTextFilter(query?.jobsJob)
  if (nextJob) {
    nextQuery.job = nextJob
  }

  const nextClient = normaliseJobsTextFilter(query?.jobsClient)
  if (nextClient) {
    nextQuery.client = nextClient
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
    jobsJob: normaliseJobsTextFilter(query?.jobsJob),
    jobsClient: normaliseJobsTextFilter(query?.jobsClient),
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
    clientJobsJob: normaliseJobsTextFilter(query?.clientJobsJob),
    clientJobsClient: normaliseJobsTextFilter(query?.clientJobsClient),
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
    jobsJob: normaliseJobsTextFilter(query?.clientJobsJob),
    jobsClient: normaliseJobsTextFilter(query?.clientJobsClient),
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
