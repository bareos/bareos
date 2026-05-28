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

import { directorCollection } from './useDirectorFetch.js'
import {
  directorAggregateErrors,
  fulfilledDirectorValues,
  runDirectorAggregates,
} from './directorAggregateRunner.js'

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
  return directorCollection(entries).map((entry, index) => ({
    ...entry,
    director,
    scopeKey: `${director}:${section}:${entry.jobid ?? entry.name ?? index}`,
  }))
}

function decorateLogEntries(entries, director) {
  return directorCollection(entries).map((entry, index) => ({
    ...entry,
    director,
    scopeKey: `${director}:log:${entry.logid ?? index}`,
  }))
}

function directorCommandErrorMessage(result) {
  const messageList = result?.error?.data?.messages?.error
  if (Array.isArray(messageList) && messageList.length > 0) {
    return String(messageList[0]).trim()
  }
  if (typeof result?.error?.message === 'string' && result.error.message) {
    return result.error.message
  }
  return ''
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

export async function fetchAggregatedDirectorConfigStatusAccess(
  credentials,
  directors
) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    try {
      const result = await client.call('status configuration')
      const message = directorCommandErrorMessage(result)
      if (message) {
        return {
          director,
          available: false,
          message,
        }
      }
      return {
        director,
        available: true,
        message: '',
      }
    } catch (error) {
      return {
        director,
        available: false,
        message: error?.message ?? 'Configuration status unavailable.',
      }
    }
  })

  return results.map((result, index) => (
    result.status === 'fulfilled'
      ? result.value
      : {
        director: directors[index],
        available: false,
        message: result.reason?.message ?? 'Configuration status unavailable.',
      }
  ))
}

export async function fetchAggregatedDirectorStatus(credentials, directors) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const result = await client.call('status director')
    return normaliseDirectorStatusSnapshot(result, director)
  })

  return {
    snapshots: fulfilledDirectorValues(results),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load director status.'),
  }
}

export async function fetchAggregatedDirectorMessages(credentials, directors, limit) {
  const results = await runDirectorAggregates(credentials, directors, async ({ client, director }) => {
    const result = await client.call(`list log limit=${limit}`)
    return decorateLogEntries(result?.log, director)
  })

  return {
    logEntries: sortByTimestampDescending(
      fulfilledDirectorValues(results).flatMap(value => value),
      'time',
      'logid'
    ),
    directorErrors: directorAggregateErrors(results, directors, 'Failed to load director messages.'),
  }
}
