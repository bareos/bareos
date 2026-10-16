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

import {
  directorCollection,
  isRunningJobStatus,
  normaliseJob,
  overlayRuntimeStatuses,
} from './useDirectorFetch.js'
import { createDirectorCommandClient } from './directorAggregate.js'

function scheduleScopeKey(director, schedule) {
  return `${director}:${schedule ?? ''}`
}

export async function fetchCombinedTimelineData(
  credentials,
  directors,
  { jobDays, schedulerRange }
) {
  const results = await Promise.allSettled(
    (directors ?? []).map(async (directorName) => {
      const client = await createDirectorCommandClient({
        ...credentials,
        director: directorName,
      })

      try {
        const [jobsResult, statusResult, schedulerResult] = await Promise.all([
          client.call(`llist jobs days=${jobDays}`),
          client.call('status director'),
          client.call(
            `status scheduler days=${schedulerRange.from},${schedulerRange.to}`
          ),
        ])
        const jobs = directorCollection(jobsResult?.jobs).map((job) => ({
          ...normaliseJob(job),
          director: directorName,
        }))
        const actualJobs = jobs.some(job => isRunningJobStatus(job.status))
          ? overlayRuntimeStatuses(jobs, statusResult?.running)
          : jobs
        const scheduledRuns = (
          Array.isArray(schedulerResult?.preview) ? schedulerResult.preview : []
        ).map((run, index) => ({
          ...run,
          director: directorName,
          scheduleKey: scheduleScopeKey(directorName, run?.schedule),
          scheduleDisplay: run?.schedule ?? '',
          scopeKey: `${directorName}:preview:${run?.schedule ?? ''}:${run?.job ?? index}:${run?.runtime ?? index}`,
        }))

        return { actualJobs, scheduledRuns }
      } finally {
        client.disconnect()
      }
    })
  )

  const fulfilled = results
    .filter(result => result.status === 'fulfilled')
    .map(result => result.value)

  if (fulfilled.length === 0 && results.some(result => result.status === 'rejected')) {
    throw results.find(result => result.status === 'rejected').reason
  }

  return {
    actualJobs: fulfilled.flatMap(result => result.actualJobs),
    scheduledRuns: fulfilled.flatMap(result => result.scheduledRuns),
    errors: results
      .filter(result => result.status === 'rejected')
      .map(result => result.reason?.message ?? String(result.reason)),
  }
}
