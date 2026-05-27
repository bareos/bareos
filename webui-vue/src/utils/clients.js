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

export function buildClientDetailsQuery({
  director,
  clientsTab,
  clientsScopeDirector,
  jobsOrigin,
  jobsAction,
  jobsStatus,
  jobsLevel,
  jobsSearch,
  dashboardOrigin,
} = {}) {
  const query = {}

  if (director) {
    query.director = director
  }

  if (clientsTab && clientsTab !== 'list') {
    query.clientsTab = clientsTab
  }

  if (clientsScopeDirector) {
    query.clientsScopeDirector = clientsScopeDirector
  }

  if (jobsOrigin) {
    query.jobsOrigin = '1'
  }

  if (jobsAction && jobsAction !== 'list') {
    query.jobsAction = jobsAction
  }

  if (jobsStatus) {
    query.jobsStatus = jobsStatus
  }

  if (jobsLevel) {
    query.jobsLevel = jobsLevel
  }

  if (jobsSearch) {
    query.jobsSearch = jobsSearch
  }

  if (dashboardOrigin) {
    query.dashboardOrigin = '1'
  }

  return query
}

export function resolveClientsListQuery(query) {
  const nextQuery = {}

  if (typeof query?.clientsTab === 'string' && query.clientsTab && query.clientsTab !== 'list') {
    nextQuery.tab = query.clientsTab
  }

  if (typeof query?.clientsScopeDirector === 'string' && query.clientsScopeDirector) {
    nextQuery.scopeDirector = query.clientsScopeDirector
  }

  return nextQuery
}

export function resolveClientDetailsDashboardOrigin(query) {
  return query?.dashboardOrigin === '1'
}

export function resolveClientDetailsJobsOrigin(query) {
  const nextQuery = {}

  if (typeof query?.jobsAction === 'string' && query.jobsAction && query.jobsAction !== 'list') {
    nextQuery.action = query.jobsAction
  }

  if (typeof query?.jobsStatus === 'string' && query.jobsStatus) {
    nextQuery.status = query.jobsStatus
  }

  if (typeof query?.jobsLevel === 'string' && query.jobsLevel) {
    nextQuery.level = query.jobsLevel
  }

  if (typeof query?.jobsSearch === 'string' && query.jobsSearch) {
    nextQuery.search = query.jobsSearch
  }

  return nextQuery
}

export function resolveClientsScopeDirector(query) {
  return typeof query?.scopeDirector === 'string' ? query.scopeDirector : ''
}

export function withClientsScopeDirectorQuery(query, director) {
  const nextQuery = { ...query }
  const nextDirector = typeof director === 'string' ? director : ''

  delete nextQuery.scopeDirector

  if (nextDirector) {
    nextQuery.scopeDirector = nextDirector
  }

  return nextQuery
}
