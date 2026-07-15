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

import { parseDurationSecs } from '../mock/index.js'
import { directorCollection, normaliseJob } from './useDirectorFetch.js'
import { createDirectorCommandClient } from './directorAggregate.js'
import {
  buildListJobsCommand,
  buildListJobsCountCommand,
  filterJobsBySearch,
  normaliseJobStatusFilters,
  normaliseJobsSearchTerm,
  paginateJobs,
} from '../utils/jobs.js'

function numberValue(value) {
  const number = Number(value ?? 0)
  return Number.isFinite(number) ? number : 0
}

function decorateJobs(entries, director) {
  return directorCollection(entries).map((entry) => {
    const job = normaliseJob(entry)
    return {
      ...job,
      director,
      scopeKey: `${director}:${job.id}`,
    }
  })
}

function stringValue(value) {
  return String(value ?? '')
}

function jobSpeedBps(job) {
  const secs = parseDurationSecs(job?.duration)
  if (!secs) {
    return 0
  }

  return numberValue(job?.bytes) / secs
}

function compareStrings(left, right) {
  return stringValue(left).localeCompare(stringValue(right))
}

function compareJobsDefault(left, right) {
  const directorCompare = compareStrings(left.director, right.director)
  if (directorCompare !== 0) {
    return directorCompare
  }

  const startCompare = compareStrings(left.starttime, right.starttime)
  if (startCompare !== 0) {
    return startCompare
  }

  return numberValue(left.id) - numberValue(right.id)
}

function compareJobsByField(left, right, sortBy) {
  switch (sortBy) {
  case 'id':
    return numberValue(left.id) - numberValue(right.id)
  case 'director':
    return compareStrings(left.director, right.director)
  case 'name':
    return compareStrings(left.name, right.name)
  case 'client':
    return compareStrings(left.client, right.client)
  case 'type':
    return compareStrings(left.type, right.type)
  case 'level':
    return compareStrings(left.level, right.level)
  case 'starttime':
    return compareStrings(left.starttime, right.starttime)
  case 'duration':
    return parseDurationSecs(left.duration) - parseDurationSecs(right.duration)
  case 'files':
    return numberValue(left.files) - numberValue(right.files)
  case 'bytes':
    return numberValue(left.bytes) - numberValue(right.bytes)
  case 'speed':
    return jobSpeedBps(left) - jobSpeedBps(right)
  case 'errors':
    return numberValue(left.errors) - numberValue(right.errors)
  case 'status':
    return compareStrings(left.runtimeStatus ?? left.status, right.runtimeStatus ?? right.status)
  default:
    return compareJobsDefault(left, right)
  }
}

export function usesDefaultJobsSorting(pagination) {
  const { sortBy = 'id', descending = true } = pagination ?? {}
  return sortBy === 'id' && descending === true
}

export function sortJobsByPagination(jobs, pagination) {
  const { sortBy = 'id', descending = true } = pagination ?? {}

  return [...jobs].sort((left, right) => {
    const compare = compareJobsByField(left, right, sortBy)
    if (compare !== 0) {
      return descending ? -compare : compare
    }

    const fallback = compareJobsDefault(left, right)
    return descending ? -fallback : fallback
  })
}

function decorateRuntimeJobs(entries) {
  return directorCollection(entries).map((entry) => ({
    id: numberValue(entry.jobid ?? entry.id),
    runtimeStatus: entry.status ?? entry.jobstatus ?? undefined,
  }))
}

function overlayRuntimeStatuses(jobs, runtimeJobs) {
  const runtimeById = new Map(
    runtimeJobs
      .filter(job => job.id > 0)
      .map(job => [job.id, job])
  )

  return jobs.map((job) => ({
    ...job,
    runtimeStatus: runtimeById.get(job.id)?.runtimeStatus ?? job.runtimeStatus,
  }))
}

export async function fetchAggregatedJobsPage(
  credentials,
  directors,
  pagination,
  statusFilter = '',
  levelFilter = '',
  typeFilter = '',
  jobFilter = '',
  clientFilter = '',
  searchTerm = '',
) {
  const { page = 1, rowsPerPage = 25 } = pagination ?? {}
  const offset = Math.max(0, (page - 1) * rowsPerPage)
  const fetchLimit = offset + rowsPerPage
  const statusFilters = normaliseJobStatusFilters(statusFilter)
  const filters = statusFilters.length > 0 ? statusFilters : ['']
  const useDefaultSorting = usesDefaultJobsSorting(pagination)
  const normalizedSearchTerm = normaliseJobsSearchTerm(searchTerm)
  const fetchAllJobs = rowsPerPage === 0 || !useDefaultSorting || normalizedSearchTerm !== ''

  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      let jobsResults
      let countResults
      let directorStatusResult

      if (useDefaultSorting && !fetchAllJobs) {
        [jobsResults, countResults, directorStatusResult] = await Promise.all([
          Promise.allSettled(filters.map(currentStatusFilter => (
            client.call(buildListJobsCommand({
              limit: fetchLimit,
              offset: 0,
              statusFilter: currentStatusFilter,
              levelFilter,
              typeFilter,
              jobFilter,
              clientFilter,
            }))
          ))),
          Promise.allSettled(filters.map(currentStatusFilter => (
            client.call(buildListJobsCountCommand({
              statusFilter: currentStatusFilter,
              levelFilter,
              typeFilter,
              jobFilter,
              clientFilter,
            }))
          ))),
          Promise.allSettled([client.call('status director')]),
        ])
      } else {
        [countResults, directorStatusResult] = await Promise.all([
          Promise.allSettled(filters.map(currentStatusFilter => (
            client.call(buildListJobsCountCommand({
              statusFilter: currentStatusFilter,
              levelFilter,
              typeFilter,
              jobFilter,
              clientFilter,
            }))
          ))),
          Promise.allSettled([client.call('status director')]),
        ])

        jobsResults = await Promise.allSettled(filters.map((currentStatusFilter, index) => {
          const countResult = countResults[index]
          if (countResult?.status === 'fulfilled') {
            const count = numberValue(directorCollection(countResult.value?.jobs)[0]?.count)
            if (count === 0) {
              return Promise.resolve({ jobs: [] })
            }

            return client.call(buildListJobsCommand({
              limit: fetchAllJobs ? count : fetchLimit,
              offset: 0,
              statusFilter: currentStatusFilter,
              levelFilter,
              typeFilter,
              jobFilter,
              clientFilter,
            }))
          }

          return client.call(buildListJobsCommand({
            limit: fetchLimit,
            offset: 0,
            statusFilter: currentStatusFilter,
            levelFilter,
            typeFilter,
            jobFilter,
            clientFilter,
          }))
        }))
      }

      const rejectedJobsResult = jobsResults.find(result => result.status === 'rejected')
      if (rejectedJobsResult?.status === 'rejected') {
        throw rejectedJobsResult.reason
      }

      const jobs = sortJobsByPagination(overlayRuntimeStatuses(jobsResults.flatMap((result) => (
        result.status === 'fulfilled'
          ? decorateJobs(result.value?.jobs, director)
          : []
      )), decorateRuntimeJobs(
        directorStatusResult[0]?.status === 'fulfilled'
          ? directorStatusResult[0].value?.running
          : []
      )), pagination)
      const count = countResults.reduce((sum, result, index) => (
        sum + (
          result.status === 'fulfilled'
            ? numberValue(directorCollection(result.value?.jobs)[0]?.count)
            : numberValue(directorCollection(
              jobsResults[index]?.status === 'fulfilled' ? jobsResults[index].value?.jobs : []
            ).length)
        )
      ), 0)

      return {
        director,
        jobs,
        count,
      }
    } finally {
      client.disconnect()
    }
  }))

  const successful = results.filter(result => result.status === 'fulfilled').map(result => result.value)
  const jobs = sortJobsByPagination(successful.flatMap(result => result.jobs), pagination)
  const filteredJobs = filterJobsBySearch(jobs, normalizedSearchTerm)
  const totalJobs = normalizedSearchTerm !== ''
    ? filteredJobs.length
    : successful.reduce((sum, result) => sum + numberValue(result.count), 0)

  return {
    jobs: paginateJobs(filteredJobs, pagination),
    totalJobs,
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load jobs.',
        }]
        : []
    )),
  }
}
