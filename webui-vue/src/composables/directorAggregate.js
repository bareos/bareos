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
  normaliseJob,
} from './useDirectorFetch.js'
import {
  createDirectorCommandSession,
  DIRECTOR_COMMAND_AUTH_TIMEOUT_MS,
  DIRECTOR_COMMAND_TIMEOUT_MS,
  DIRECTOR_WS_URL,
} from '../utils/directorCommandSocket.js'
import { useAuthStore } from '../stores/auth.js'
import { SESSION_AUTH_PASSWORD } from '../utils/sessionApi.js'

export async function createDirectorCommandClient(credentials, options = {}) {
  const auth = useAuthStore()
  if (
    credentials?.password === SESSION_AUTH_PASSWORD
    && !auth.hasDirectorSession(credentials.director)
  ) {
    throw new Error(`Please log in to director "${credentials.director}" first.`)
  }

  const session = await createDirectorCommandSession(credentials, {
    wsUrl: options.wsUrl ?? DIRECTOR_WS_URL,
    authTimeoutMs: options.authTimeoutMs ?? DIRECTOR_COMMAND_AUTH_TIMEOUT_MS,
    commandTimeoutMs: options.commandTimeoutMs ?? DIRECTOR_COMMAND_TIMEOUT_MS,
  })

  return {
    director: credentials.director,
    transport: session.transport,
    call: session.call,
    disconnect: session.close,
  }
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

function numberValue(value) {
  const number = Number(value ?? 0)
  return Number.isFinite(number) ? number : 0
}

function optionalNumberValue(value) {
  if (value == null || value === '') {
    return undefined
  }

  const number = Number(value)
  return Number.isFinite(number) ? number : undefined
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

function decorateRuntimeJobs(entries, director) {
  return directorCollection(entries).map((entry, index) => {
    const id = optionalNumberValue(entry.jobid ?? entry.id)

    return {
      id,
      name: entry.name ?? undefined,
      client: entry.client ?? entry.clientname ?? undefined,
      type: entry.jobtype ?? entry.type ?? undefined,
      level: entry.joblevel ?? entry.level ?? undefined,
      starttime: entry.start_time ?? entry.starttime ?? undefined,
      duration: entry.duration ?? undefined,
      files: optionalNumberValue(entry.files ?? entry.jobfiles),
      bytes: optionalNumberValue(entry.bytes ?? entry.jobbytes),
      errors: optionalNumberValue(entry.errors ?? entry.joberrors),
      runtimeStatus: entry.status ?? entry.jobstatus ?? undefined,
      director,
      scopeKey: `${director}:${id ?? entry.name ?? index}`,
    }
  })
}

function withRuntimeJobState(job, runtimeJob) {
  if (!runtimeJob) {
    return job
  }

  return {
    ...job,
    name: runtimeJob.name ?? job.name,
    client: runtimeJob.client ?? job.client,
    type: runtimeJob.type ?? job.type,
    level: runtimeJob.level ?? job.level,
    starttime: runtimeJob.starttime ?? job.starttime,
    duration: runtimeJob.duration ?? job.duration,
    files: runtimeJob.files ?? job.files,
    bytes: runtimeJob.bytes ?? job.bytes,
    errors: runtimeJob.errors ?? job.errors,
    runtimeStatus: runtimeJob.runtimeStatus ?? job.runtimeStatus,
  }
}

function runtimeJobsById(runtimeJobs) {
  return new Map(
    runtimeJobs
      .filter(job => job.id != null)
      .map(job => [job.id, job])
  )
}

function mergeRunningJobs(catalogJobs, runtimeJobs) {
  const runtimeById = runtimeJobsById(runtimeJobs)
  const mergedCatalogJobs = catalogJobs.map(job => withRuntimeJobState(job, runtimeById.get(job.id)))
  const knownJobIds = new Set(catalogJobs.map(job => job.id))
  const runtimeOnlyJobs = runtimeJobs
    .filter(job => !knownJobIds.has(job.id))
    .map(job => withRuntimeJobState({
      id: job.id ?? 0,
      name: job.name ?? '',
      client: job.client ?? '',
      type: job.type ?? '',
      level: job.level ?? '',
      status: 'R',
      starttime: job.starttime ?? '',
      endtime: '',
      duration: job.duration ?? '',
      files: job.files ?? 0,
      bytes: job.bytes ?? 0,
      errors: job.errors ?? 0,
      director: job.director,
      scopeKey: job.scopeKey,
    }, job))

  return sortJobsByStartTime([...mergedCatalogJobs, ...runtimeOnlyJobs])
}

function overlayRuntimeStatus(jobs, runtimeJobs) {
  const runtimeById = runtimeJobsById(runtimeJobs)
  return sortJobsByStartTime(
    jobs.map(job => withRuntimeJobState(job, runtimeById.get(job.id)))
  )
}

export async function fetchDirectorDashboardSnapshot(credentials, options = {}) {
  const client = await createDirectorCommandClient(credentials, options)

  try {
    const [
      past24hResult,
      runningResult,
      lastResult,
      totalsResult,
      clientsResult,
      storagesResult,
      directorStatusResult,
    ] = await Promise.allSettled([
      client.call('llist jobs days=1'),
      client.call('list jobs jobstatus=R'),
      client.call('llist jobs last'),
      client.call('list jobtotals'),
      client.call('list clients'),
      client.call('list storages'),
      client.call('status director'),
    ])

    const runtimeRunningJobs = directorStatusResult.status === 'fulfilled'
      ? decorateRuntimeJobs(directorStatusResult.value?.running, credentials.director)
      : []
    const runningJobs = runningResult.status === 'fulfilled'
      ? decorateJobs(runningResult.value?.jobs, credentials.director)
      : []
    const recentJobs = lastResult.status === 'fulfilled'
      ? decorateJobs(lastResult.value?.jobs, credentials.director)
      : []
    const jobsPast24h = past24hResult.status === 'fulfilled'
      ? decorateJobs(past24hResult.value?.jobs, credentials.director)
      : []

    return {
      director: credentials.director,
      transport: client.transport,
      jobsPast24h,
      runningJobs: mergeRunningJobs(runningJobs, runtimeRunningJobs),
      recentJobs: overlayRuntimeStatus(recentJobs, runtimeRunningJobs),
      jobTotals: totalsResult.status === 'fulfilled'
        ? {
          jobs: numberValue(totalsResult.value?.jobtotals?.jobs),
          files: numberValue(totalsResult.value?.jobtotals?.files),
          bytes: numberValue(totalsResult.value?.jobtotals?.bytes),
        }
        : {
          jobs: 0,
          files: 0,
          bytes: 0,
        },
      clientCount: clientsResult.status === 'fulfilled'
        ? directorCollection(clientsResult.value?.clients).length
        : 0,
      storageCount: storagesResult.status === 'fulfilled'
        ? directorCollection(storagesResult.value?.storages).length
        : 0,
    }
  } finally {
    client.disconnect()
  }
}

export function aggregateDirectorDashboardSnapshots(snapshots) {
  return snapshots.reduce((aggregate, snapshot) => ({
    jobsPast24h: sortJobsByStartTime([
      ...aggregate.jobsPast24h,
      ...(snapshot.jobsPast24h ?? []),
    ]),
    runningJobs: sortJobsByStartTime([
      ...aggregate.runningJobs,
      ...(snapshot.runningJobs ?? []),
    ]),
    recentJobs: sortJobsByStartTime([
      ...aggregate.recentJobs,
      ...(snapshot.recentJobs ?? []),
    ]),
    clientCount: aggregate.clientCount + numberValue(snapshot.clientCount),
    storageCount: aggregate.storageCount + numberValue(snapshot.storageCount),
    jobTotals: {
      jobs: aggregate.jobTotals.jobs + numberValue(snapshot.jobTotals?.jobs),
      files: aggregate.jobTotals.files + numberValue(snapshot.jobTotals?.files),
      bytes: aggregate.jobTotals.bytes + numberValue(snapshot.jobTotals?.bytes),
    },
  }), {
    jobsPast24h: [],
    runningJobs: [],
    recentJobs: [],
    clientCount: 0,
    storageCount: 0,
    jobTotals: {
      jobs: 0,
      files: 0,
      bytes: 0,
    },
  })
}
