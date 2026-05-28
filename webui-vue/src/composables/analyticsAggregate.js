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

export async function fetchAggregatedAnalytics(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const result = await client.call('list jobs')
    return decorateJobs(result?.jobs, director)
  })

  return {
    jobs: sortJobs(fulfilledDirectorValues(results).flatMap(value => value)),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load analytics data.'),
  }
}
