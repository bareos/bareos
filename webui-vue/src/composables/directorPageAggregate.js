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

import { createDirectorCommandClient } from './directorAggregate.js'
import {
  directorCollection,
  extractStorageRuntime,
} from './useDirectorFetch.js'

function directorError(results, directors, index, fallback) {
  return results[index].status === 'rejected'
    ? {
      director: directors[index],
      message: results[index].reason?.message ?? fallback,
    }
    : null
}

function sortByTimestampDescending(entries, field, fallbackField) {
  return [...entries].sort((left, right) => {
    const timeCompare = String(right[field] ?? '').localeCompare(String(left[field] ?? ''))
    if (timeCompare !== 0) {
      return timeCompare
    }

    const directorCompare = String(left.director ?? '').localeCompare(String(right.director ?? ''))
    if (directorCompare !== 0) {
      return directorCompare
    }

    return String(right[fallbackField] ?? '').localeCompare(String(left[fallbackField] ?? ''))
  })
}

function decorateStatusEntries(entries, director, section) {
  return directorCollection(entries).map((entry, index) => {
    const runtime = extractStorageRuntime(entry)

    return {
      ...entry,
      director,
      ...(runtime ? { runtime } : {}),
      scopeKey: `${director}:${section}:${entry.jobid ?? entry.name ?? index}`,
    }
  })
}

function decorateLogEntries(entries, director) {
  return directorCollection(entries).map((entry, index) => ({
    ...entry,
    director,
    scopeKey: `${director}:log:${entry.logid ?? index}`,
  }))
}

export function normaliseDirectorStatusSnapshot(data, director) {
  return {
    director,
    header: data?.header ?? {},
    scheduledJobs: sortByTimestampDescending(
      decorateStatusEntries(data?.scheduled, director, 'scheduled'),
      'scheduled',
      'name'
    ).reverse(),
    runningJobs: sortByTimestampDescending(
      decorateStatusEntries(data?.running, director, 'running'),
      'start_time',
      'jobid'
    ),
    terminatedJobs: sortByTimestampDescending(
      decorateStatusEntries(data?.terminated, director, 'terminated'),
      'finished',
      'jobid'
    ),
  }
}

export async function fetchAggregatedDirectorStatus(credentials, directors) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const result = await client.call('status director')
      return normaliseDirectorStatusSnapshot(result, director)
    } finally {
      client.disconnect()
    }
  }))

  return {
    snapshots: results
      .filter(result => result.status === 'fulfilled')
      .map(result => result.value),
    directorErrors: results
      .map((_, index) => directorError(
        results,
        directors,
        index,
        'Failed to load director status.'
      ))
      .filter(Boolean),
  }
}

export async function fetchAggregatedDirectorMessages(credentials, directors, limit) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const result = await client.call(`list log limit=${limit}`)
      return decorateLogEntries(result?.log, director)
    } finally {
      client.disconnect()
    }
  }))

  return {
    logEntries: sortByTimestampDescending(
      results
        .filter(result => result.status === 'fulfilled')
        .flatMap(result => result.value),
      'time',
      'logid'
    ),
    directorErrors: results
      .map((_, index) => directorError(
        results,
        directors,
        index,
        'Failed to load director messages.'
      ))
      .filter(Boolean),
  }
}
