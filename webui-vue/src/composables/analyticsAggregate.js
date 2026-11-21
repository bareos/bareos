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
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

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

function sortJobs(jobs) {
  return [...jobs].sort((left, right) => {
    const nameCompare = String(left.name ?? '').localeCompare(String(right.name ?? ''))
    if (nameCompare !== 0) {
      return nameCompare
    }

    const directorCompare = String(left.director ?? '').localeCompare(String(right.director ?? ''))
    if (directorCompare !== 0) {
      return directorCompare
    }

    return Number(right.id ?? 0) - Number(left.id ?? 0)
  })
}

export async function fetchAggregatedAnalytics(credentials, directors, maxJobsLimit = 1000) {
  if (!Number.isSafeInteger(maxJobsLimit) || maxJobsLimit <= 0 || maxJobsLimit >= Number.MAX_SAFE_INTEGER) {
    throw new RangeError('Analytics page size must be a positive integer')
  }
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    for (let attempt = 0; attempt < 3; attempt += 1) {
      const jobs = []
      let offset = 0
      let anchor = null
      while (true) {
        const limit = anchor === null ? maxJobsLimit : maxJobsLimit + 1
        const result = await client.call(
          `list jobs limit=${limit} offset=${offset} sortby=jobid`
        )
        const page = directorCollection(result?.jobs)
        const filtered = result?.meta?.range?.filtered
        if (!Number.isSafeInteger(filtered) || filtered < 0 || page.length > limit) {
          throw new Error('Invalid analytics pagination metadata')
        }
        if (filtered > 0) {
          // A filtered row has no visible ID to use as an overlap anchor.
          const snapshot = await client.call('list jobs sortby=jobid')
          return decorateJobs(snapshot?.jobs, director)
        }
        const entries = decorateJobs(page, director)
        if (entries.some((job, index) => !Number.isSafeInteger(job.id) || job.id <= 0
            || (index > 0 && job.id <= entries[index - 1].id))) {
          throw new Error('Invalid analytics job ordering')
        }
        if (anchor !== null && entries[0]?.id !== anchor) break
        jobs.push(...(anchor === null ? entries : entries.slice(1)))
        if (page.length < limit) return jobs
        offset += page.length - 1
        anchor = entries.at(-1).id
      }
    }
    throw new Error('Analytics jobs changed during pagination')
  })

  return {
    jobs: sortJobs(fulfilledDirectorValues(results).flatMap(value => value)),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load analytics data.'),
  }
}
