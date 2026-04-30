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

export function normaliseJobStatusFilter(value) {
  if (typeof value !== 'string') {
    return ''
  }

  return Object.hasOwn(jobStatusMap, value) ? value : ''
}

export function withJobsStatusFilterQuery(query, status) {
  const nextQuery = { ...query }
  const nextStatus = normaliseJobStatusFilter(status)

  delete nextQuery.status
  if (nextStatus) {
    nextQuery.status = nextStatus
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
  jobsSearch,
  clientName,
  clientDirector,
  clientsTab,
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

  if (jobsStatus) {
    query.jobsStatus = jobsStatus
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

  if (typeof query?.jobsStatus === 'string' && query.jobsStatus) {
    nextQuery.status = query.jobsStatus
  }

  if (typeof query?.jobsSearch === 'string' && query.jobsSearch) {
    nextQuery.search = query.jobsSearch
  }

  return nextQuery
}

export function resolveJobDetailsClientOrigin(query) {
  if (typeof query?.clientName !== 'string' || !query.clientName) {
    return null
  }

  return {
    name: query.clientName,
    director: typeof query?.clientDirector === 'string' ? query.clientDirector : '',
    clientsTab: typeof query?.clientsTab === 'string' ? query.clientsTab : '',
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
