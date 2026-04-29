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

function scheduleScopeKey(director, schedule) {
  return `${director}:${schedule}`
}

function normaliseShownSchedule(entry, director) {
  const name = entry?.name ?? ''
  return {
    name,
    enabled: entry?.enabled !== false && entry?.enabled !== 0,
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
    enabled: entry?.enabled !== false && entry?.enabled !== 0,
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

export async function fetchAggregatedSchedulesShow(credentials, directors) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const response = await client.call('show schedules')
      const shown = Object.values(response?.schedules ?? {})
      return {
        schedules: shown.map(entry => normaliseShownSchedule(entry, director)),
      }
    } finally {
      client.disconnect()
    }
  }))

  return {
    schedules: sortSchedules(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value.schedules)),
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load schedules.',
        }]
        : []
    )),
  }
}

export async function fetchAggregatedSchedulesStatus(credentials, directors, range) {
  const results = await Promise.allSettled(directors.map(async (director) => {
    const client = await createDirectorCommandClient({
      ...credentials,
      director,
    })

    try {
      const [statusResponse, showResponse] = await Promise.all([
        client.call(`status scheduler days=${range.from},${range.to}`),
        client.call('show schedules'),
      ])
      const active = Array.isArray(statusResponse?.schedules)
        ? statusResponse.schedules
        : []
      const activeNames = new Set(active.map(item => item.name))
      const allShown = Object.values(showResponse?.schedules ?? {})
      const disabled = allShown
        .filter(item => !activeNames.has(item.name))
        .map(item => ({ name: item.name, enabled: false, jobs: [] }))

      return {
        schedulesData: sortSchedules(
          [...active, ...disabled].map(item => normaliseStatusSchedule(item, director))
        ),
        previewData: (Array.isArray(statusResponse?.preview) ? statusResponse.preview : [])
          .map(item => ({
            ...item,
            director,
            scheduleKey: scheduleScopeKey(director, item.schedule ?? ''),
            scheduleDisplay: `${director} / ${item.schedule ?? ''}`,
          })),
      }
    } finally {
      client.disconnect()
    }
  }))

  return {
    schedulesData: sortSchedules(results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value.schedulesData)),
    previewData: results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value.previewData),
    directorErrors: results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: directors[index],
          message: result.reason?.message ?? 'Failed to load scheduler status.',
        }]
        : []
    )),
  }
}
