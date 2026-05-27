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

const JOB_LEVEL_FILTERS = new Set(['F', 'I', 'D', 'V', 'B'])

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

export function encodeJobsStatusFilters(value) {
  return normaliseJobStatusFilters(value).join(',')
}

export function encodeJobsLevelFilters(value) {
  return normaliseJobLevelFilters(value).join(',')
}

export function resolveJobsStatusFilters(query) {
  return normaliseJobStatusFilters(query?.status)
}

export function resolveJobsLevelFilters(query) {
  return normaliseJobLevelFilters(query?.level)
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
  jobsSearch,
  clientName,
  clientDirector,
  clientsTab,
  clientsScopeDirector,
  clientDashboardOrigin,
  clientJobsAction,
  clientJobsStatus,
  clientJobsLevel,
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
    jobsSearch: typeof query?.jobsSearch === 'string' ? query.jobsSearch : '',
    clientName: typeof query?.clientName === 'string' ? query.clientName : '',
    clientDirector: typeof query?.clientDirector === 'string' ? query.clientDirector : '',
    clientsTab: typeof query?.clientsTab === 'string' ? query.clientsTab : '',
    clientsScopeDirector: typeof query?.clientsScopeDirector === 'string' ? query.clientsScopeDirector : '',
    clientDashboardOrigin: query?.clientDashboardOrigin === '1',
    clientJobsAction: typeof query?.clientJobsAction === 'string' ? query.clientJobsAction : '',
    clientJobsStatus: encodeJobsStatusFilters(query?.clientJobsStatus),
    clientJobsLevel: encodeJobsLevelFilters(query?.clientJobsLevel),
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
