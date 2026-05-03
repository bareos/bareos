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

import { directorCollection, normaliseJob } from './useDirectorFetch.js'
import { createDirectorCommandClient } from './directorAggregate.js'
import { normaliseJobStatusFilters } from '../utils/jobs.js'

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

function sortJobsByStartTime(jobs) {
  return [...jobs].sort((left, right) => {
    const startCompare = String(right.starttime ?? '').localeCompare(String(left.starttime ?? ''))
    if (startCompare !== 0) {
      return startCompare
    }

    const directorCompare = String(left.director ?? '').localeCompare(String(right.director ?? ''))
    if (directorCompare !== 0) {
      return directorCompare
    }

    return numberValue(right.id) - numberValue(left.id)
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

function jobsFilterCommands(statusFilter) {
  const filters = normaliseJobStatusFilters(statusFilter)
  return filters.length ? filters.map(filter => ` jobstatus=${filter}`) : ['']
}

export async function fetchAggregatedJobsPage(credentials, directors, pagination, statusFilter = '') {
  const { page = 1, rowsPerPage = 25 } = pagination ?? {}
  const offset = Math.max(0, (page - 1) * rowsPerPage)
  const fetchLimit = offset + rowsPerPage
  const filters = jobsFilterCommands(statusFilter)

  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const [jobsResults, countResults, directorStatusResult] = await Promise.all([
        Promise.allSettled(filters.map(filter => (
          client.call(`llist jobs reverse limit=${fetchLimit} offset=0${filter}`)
        ))),
        Promise.allSettled(filters.map(filter => (
          client.call(`list jobs count${filter}`)
        ))),
        Promise.allSettled([client.call('status director')]),
      ])

      const rejectedJobsResult = jobsResults.find(result => result.status === 'rejected')
      if (rejectedJobsResult?.status === 'rejected') {
        throw rejectedJobsResult.reason
      }

      const jobs = sortJobsByStartTime(overlayRuntimeStatuses(jobsResults.flatMap((result) => (
        result.status === 'fulfilled'
          ? decorateJobs(result.value?.jobs, director)
          : []
      )), decorateRuntimeJobs(
        directorStatusResult[0]?.status === 'fulfilled'
          ? directorStatusResult[0].value?.running
          : []
      )))
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
  const jobs = sortJobsByStartTime(successful.flatMap(result => result.jobs))
  const totalJobs = successful.reduce((sum, result) => sum + numberValue(result.count), 0)

  return {
    jobs: jobs.slice(offset, offset + rowsPerPage),
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
