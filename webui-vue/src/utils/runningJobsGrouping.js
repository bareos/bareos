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

// Shared helpers for presenting large numbers of running/queued jobs
// (from the director's live `status director` -> `running` array, as
// already decorated by directorAggregate.js's decorateRuntimeJobs()) in a
// human-consumable, aggregated form instead of one row per job. Used by:
//   - RunningJobsWidget.vue's grouped "Queued" section
//   - the Running Jobs status doughnut widget

import { parseDirectorDate } from './locales.js'

const UNKNOWN_STATUS_LABEL = 'Unknown'
const UNKNOWN_CLIENT_LABEL = 'Unknown'

/**
 * A job is "waiting" if its runtime status text contains "is waiting"
 * (e.g. "is waiting on max Storage jobs", "is waiting execution"),
 * matching the director's ListRunningJobs() text output.
 */
export function isWaitingStatus(status) {
  return typeof status === 'string' && status.toLowerCase().includes('is waiting')
}

/** Best-effort human status/wait-reason bucket for a decorated running job. */
export function jobStatusBucket(job) {
  const status = job?.runtimeStatus ?? job?.status
  if (typeof status === 'string' && status.trim() !== '') {
    return status
  }
  return UNKNOWN_STATUS_LABEL
}

/** Best-effort client name bucket for a decorated running job. */
export function jobClientBucket(job) {
  const client = job?.client
  if (typeof client === 'string' && client.trim() !== '') {
    return client
  }
  return UNKNOWN_CLIENT_LABEL
}

/**
 * Elapsed running time in whole seconds for a decorated running/queued
 * job, using the director's `start_time` field (format `dd-Mon-yy HH:MM`,
 * e.g. "03-Sep-26 16:02" - no seconds, parsed via the shared
 * parseDirectorDate() helper). Returns 0 if the job has no start time or
 * it cannot be parsed. `now` defaults to the current time but should be
 * passed explicitly (e.g. a reactive `now.value`) by callers that need
 * live-updating durations.
 *
 * Note: since the source format has no seconds, the result is only
 * accurate to within the job's start minute (up to ~59s off).
 */
export function elapsedRunningSecs(job, now = Date.now()) {
  const start = parseDirectorDate(job?.starttime)
  if (!start) return 0
  return Math.max(0, Math.floor((now - start.getTime()) / 1000))
}

/**
 * Groups jobs by a key function, returning an array of
 * { key, count, jobs } sorted by descending count (ties broken by key,
 * ascending, for stable/deterministic rendering).
 */
export function groupRunningJobsBy(jobs, keyFn) {
  const groups = new Map()
  for (const job of jobs ?? []) {
    const key = keyFn(job)
    if (!groups.has(key)) { groups.set(key, []) }
    groups.get(key).push(job)
  }

  return Array.from(groups.entries())
    .map(([key, groupJobs]) => ({ key, count: groupJobs.length, jobs: groupJobs }))
    .sort((a, b) => (b.count - a.count) || String(a.key).localeCompare(String(b.key)))
}

/** Groups running/queued jobs by status/wait-reason text. */
export function groupRunningJobsByStatus(jobs) {
  return groupRunningJobsBy(jobs, jobStatusBucket)
}

/**
 * Groups running/queued jobs by client, keeping only the top `topN`
 * clients by job count and folding the remainder into a single "Other"
 * bucket (omitted entirely if there is no remainder).
 */
export function groupRunningJobsByClient(jobs, { topN = 10 } = {}) {
  const grouped = groupRunningJobsBy(jobs, jobClientBucket)
  if (grouped.length <= topN) { return grouped }

  const top = grouped.slice(0, topN)
  const rest = grouped.slice(topN)
  const otherJobs = rest.flatMap(g => g.jobs)
  return [
    ...top,
    { key: 'Other', count: otherJobs.length, jobs: otherJobs },
  ]
}
