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
  normaliseClient,
  normaliseJob,
} from './useDirectorFetch.js'
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'
import { createTtlCache, hashCacheFingerprint } from './ttlCache.js'

function decorateClients(entries, enabledMap, director) {
  return directorCollection(entries).map((entry) => {
    const client = normaliseClient({
      ...entry,
      enabled: enabledMap[entry.name] ?? true,
    })
    return {
      ...client,
      director,
      scopeKey: `${director}:${client.name}`,
    }
  })
}

function decorateRecentBackups(entries, director) {
  return directorCollection(entries).map((entry) => ({
    ...normaliseJob(entry),
    director,
  }))
}

export function decorateScheduledBackups(response, director) {
  const schedules = new Map(
    (Array.isArray(response?.schedules) ? response.schedules : [])
      .map(schedule => [schedule?.name, schedule])
  )
  const scheduled = []
  const now = Math.floor(Date.now() / 1000)

  for (const preview of Array.isArray(response?.preview) ? response.preview : []) {
    const runtime = Number(preview?.runtime ?? 0)
    if (!Number.isFinite(runtime) || runtime <= 0 || runtime > now) {
      continue
    }
    const schedule = schedules.get(preview?.schedule)
    const jobs = Array.isArray(schedule?.jobs) ? schedule.jobs : []
    const previewJobs = preview?.client
      ? [{ name: preview.job ?? '', client: preview.client, enabled: true }]
      : jobs

    for (const job of previewJobs) {
      if (!job?.client || job.enabled === false || schedule?.enabled === false) {
        continue
      }
      scheduled.push({
        client: job.client,
        job: job.name ?? preview.job ?? '',
        schedule: preview.schedule ?? '',
        runtime,
        director,
      })
    }
  }

  return scheduled
}

function sortClients(clients) {
  return [...clients].sort((left, right) => {
    const nameCompare = String(left.name ?? '').localeCompare(String(right.name ?? ''))
    if (nameCompare !== 0) {
      return nameCompare
    }

    return String(left.director ?? '').localeCompare(String(right.director ?? ''))
  })
}

const clientsCache = createTtlCache()
const CACHE_TTL_MS = 60_000 // 60 seconds TTL

function buildCacheKey(credentials, directors) {
  return JSON.stringify({
    user: credentials?.username ?? '',
    // Fold the session credential into the key so a re-login under the same
    // username (but a different password/session) can't reuse another
    // session's cached catalog data within the TTL window.
    session: hashCacheFingerprint(credentials?.password),
    directors: [...directors].sort(),
  })
}

export function clearClientsCache() {
  clientsCache.clear()
}

export async function fetchAggregatedClients(credentials, directors, { forceRefresh = false } = {}) {
  const cacheKey = buildCacheKey(credentials, directors)
  if (!forceRefresh) {
    const cached = clientsCache.get(cacheKey, CACHE_TTL_MS)
    if (cached) {
      return cached
    }
  }
  const fetchGeneration = clientsCache.beginFetch()

  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const [listResult, dotResult, recentBackupsResult, schedulerResult] = await Promise.all([
      client.call('llist clients'),
      client.call('.clients'),
      client.call('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
      client.call('status scheduler days=-31,1'),
    ])

    const enabledMap = Object.fromEntries(
      directorCollection(dotResult?.clients).map(item => [item.name, item.enabled])
    )

    return {
      director,
      clients: decorateClients(listResult?.clients, enabledMap, director),
      recentBackups: decorateRecentBackups(recentBackupsResult?.jobs, director),
      scheduledBackups: decorateScheduledBackups(schedulerResult, director),
    }
  })

  const data = {
    clients: sortClients(fulfilledDirectorValues(results).flatMap(value => value.clients)),
    recentBackups: fulfilledDirectorValues(results).flatMap(value => value.recentBackups),
    scheduledBackups: fulfilledDirectorValues(results).flatMap(value => value.scheduledBackups),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load clients.'),
  }
  clientsCache.set(cacheKey, data, fetchGeneration)
  return data
}
