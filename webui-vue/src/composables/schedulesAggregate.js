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
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

function scheduleScopeKey(director, schedule) {
  return `${director}:${schedule}`
}

function isEnabled(value) {
  return value !== false && value !== 0
}

function scheduleNamesFromShow(showResponse) {
  return Object.values(showResponse?.schedules ?? {})
    .map(entry => entry?.name ?? '')
    .filter(Boolean)
}

function scheduleEnabledByName(scheduleStateResponse) {
  return new Map(
    (Array.isArray(scheduleStateResponse?.schedules) ? scheduleStateResponse.schedules : [])
      .map(entry => [entry?.name ?? '', isEnabled(entry?.enabled)])
      .filter(([name]) => Boolean(name))
  )
}

function scheduleJobsFromConfig(showAllResponse) {
  const jobsBySchedule = new Map()
  const clients = showAllResponse?.clients ?? {}
  const jobDefs = showAllResponse?.jobdefs ?? {}

  function addJob(scheduleName, job) {
    if (!scheduleName || !job?.name) {
      return
    }

    const client = job?.client ? clients[job.client] : null
    const enabled = isEnabled(job?.enabled) && isEnabled(client?.enabled)
    const jobs = jobsBySchedule.get(scheduleName) ?? []
    if (!jobs.some(existing => existing.name === job.name)) {
      jobs.push({ name: job.name, enabled })
      jobsBySchedule.set(scheduleName, jobs)
    }
  }

  Object.values(showAllResponse?.jobs ?? {}).forEach((job) => {
    const jobDef = job?.jobdefs ? jobDefs[job.jobdefs] : null
    const scheduleName = job?.schedule ?? jobDef?.schedule
    addJob(scheduleName, job)
  })

  return jobsBySchedule
}

function normaliseShownSchedule(entry, director) {
  const name = entry?.name ?? ''
  return {
    name,
    enabled: isEnabled(entry?.enabled),
    run: Array.isArray(entry?.run) ? entry.run : (entry?.run ? [entry.run] : []),
    director,
    scopeKey: scheduleScopeKey(director, name),
    displayName: `${director} / ${name}`,
  }
}

function normaliseStatusSchedule(entry, director) {
  const name = entry?.name ?? ''
  return {
    ...entry,
    name,
    enabled: isEnabled(entry?.enabled),
    director,
    scopeKey: scheduleScopeKey(director, name),
    displayName: `${director} / ${name}`,
    jobs: Array.isArray(entry?.jobs)
      ? entry.jobs.map(job => ({
        ...job,
        director,
        schedule: name,
        scheduleKey: scheduleScopeKey(director, name),
      }))
      : [],
  }
}

function sortSchedules(entries) {
  return [...entries].sort((left, right) => {
    const nameCompare = String(left.name ?? '').localeCompare(String(right.name ?? ''))
    if (nameCompare !== 0) {
      return nameCompare
    }

    return String(left.director ?? '').localeCompare(String(right.director ?? ''))
  })
}

export function buildShownSchedules(showResponse, scheduleStateResponse, director) {
  const enabledByName = scheduleEnabledByName(scheduleStateResponse)

  return sortSchedules(
    Object.values(showResponse?.schedules ?? {})
      .map(entry => {
        const name = entry?.name ?? ''
        return normaliseShownSchedule({
          ...entry,
          enabled: enabledByName.get(name) ?? entry?.enabled,
        }, director)
      })
  )
}

export function buildStatusSchedules(statusResponse, showResponse, showAllResponse, scheduleStateResponse, director) {
  const activeSchedules = Array.isArray(statusResponse?.schedules)
    ? statusResponse.schedules
    : []
  const activeByName = new Map(activeSchedules.map(entry => [entry?.name ?? '', entry]))
  const enabledByName = scheduleEnabledByName(scheduleStateResponse)
  const jobsBySchedule = scheduleJobsFromConfig(showAllResponse)
  const scheduleNames = new Set([
    ...scheduleNamesFromShow(showResponse),
    ...activeByName.keys(),
    ...enabledByName.keys(),
  ])

  return sortSchedules(
    [...scheduleNames]
      .filter(Boolean)
      .map((name) => {
        const activeEntry = activeByName.get(name)
        const fallbackJobs = jobsBySchedule.get(name) ?? []
        const jobs = Array.isArray(activeEntry?.jobs) && activeEntry.jobs.length > 0
          ? activeEntry.jobs
          : fallbackJobs

        return normaliseStatusSchedule({
          ...activeEntry,
          name,
          enabled: enabledByName.get(name) ?? activeEntry?.enabled,
          jobs,
        }, director)
      })
  )
}

export async function fetchAggregatedSchedulesShow(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const [showResponse, scheduleStateResponse] = await Promise.all([
      client.call('show schedules'),
      client.call('.schedule'),
    ])
    return {
      schedules: buildShownSchedules(showResponse, scheduleStateResponse, director),
    }
  })

  return {
    schedules: sortSchedules(fulfilledDirectorValues(results).flatMap(value => value.schedules)),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load schedules.'),
  }
}

export async function fetchAggregatedSchedulesStatus(credentials, directors, range) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const [statusResponse, showResponse, showAllResponse, scheduleStateResponse] = await Promise.all([
      client.call(`status scheduler days=${range.from},${range.to}`),
      client.call('show schedules'),
      client.call('show schedules all'),
      client.call('.schedule'),
    ])

    return {
      schedulesData: buildStatusSchedules(
        statusResponse,
        showResponse,
        showAllResponse,
        scheduleStateResponse,
        director
      ),
      previewData: (Array.isArray(statusResponse?.preview) ? statusResponse.preview : [])
        .map(item => ({
          ...item,
          director,
          scheduleKey: scheduleScopeKey(director, item.schedule ?? ''),
          scheduleDisplay: `${director} / ${item.schedule ?? ''}`,
        })),
    }
  })

  return {
    schedulesData: sortSchedules(fulfilledDirectorValues(results).flatMap(value => value.schedulesData)),
    previewData: fulfilledDirectorValues(results).flatMap(value => value.previewData),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load scheduler status.'),
  }
}
