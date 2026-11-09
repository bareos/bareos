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

export function parseTimelineTimestamp(str) {
  if (!str || str.startsWith('0000')) return null
  return new Date(str.replace(' ', 'T')).getTime()
}

export function parseSchedulerRuntime(value) {
  const runtime = Number(value)
  return Number.isFinite(runtime) && runtime > 0 ? runtime * 1000 : null
}

export const CENTERED_TIMELINE_WINDOW_MS = 86_400_000

export function buildCenteredTimelineDayBounds(
  now,
  windowMs = CENTERED_TIMELINE_WINDOW_MS
) {
  const center = Number(now)
  const duration = Number(windowMs)
  const safeDuration = Number.isFinite(duration) && duration > 0
    ? duration
    : CENTERED_TIMELINE_WINDOW_MS
  const safeCenter = Number.isFinite(center) ? center : Date.now()
  const halfWindow = safeDuration / 2
  return {
    start: safeCenter - halfWindow,
    end: safeCenter + halfWindow,
  }
}

// Groups jobs into one timeline lane per <fileset>@<client> tuple (matching
// the tuple used for quick-restore selection elsewhere in the app), each
// holding the individual job runs (of any job name) that share that
// fileset+client, sorted chronologically. Used for the Day, Week, and
// Month views alike — only the caller-supplied [start, now) window and the
// resulting bar geometry differ between them.
export function buildTimelineLanes(jobs, { start, now, multiDirectorTimeline }) {
  const directorGroups = new Map()

  for (const job of jobs ?? []) {
    const startedAt = parseTimelineTimestamp(job.starttime)
    const endedAt = parseTimelineTimestamp(job.endtime) ?? now
    if (startedAt === null || endedAt < start || startedAt > now) continue

    const directorKey = multiDirectorTimeline ? (job.director ?? '') : ''
    if (!directorGroups.has(directorKey)) {
      directorGroups.set(directorKey, {
        director: job.director ?? null,
        lanes: new Map(),
      })
    }

    const directorGroup = directorGroups.get(directorKey)
    const client = job.client ?? ''
    const fileset = job.fileset ?? ''
    const laneKey = `${client}\u0000${fileset}`
    if (!directorGroup.lanes.has(laneKey)) {
      directorGroup.lanes.set(laneKey, {
        key: `${directorKey}\u0000${laneKey}`,
        client,
        fileset,
        director: job.director ?? null,
        runs: [],
      })
    }
    directorGroup.lanes.get(laneKey).runs.push(job)
  }

  return [...directorGroups.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([, directorGroup]) => ({
      director: directorGroup.director,
      lanes: [...directorGroup.lanes.values()]
        .sort((a, b) => a.client.localeCompare(b.client) || a.fileset.localeCompare(b.fileset))
        .map(lane => ({
          ...lane,
          runs: lane.runs.sort(
            (a, b) => (parseTimelineTimestamp(a.starttime) ?? 0)
              - (parseTimelineTimestamp(b.starttime) ?? 0)
          ),
        })),
    }))
    .filter(group => group.lanes.length > 0)
}

// Distinct fileset@client tuples present across `jobs`, sorted by label —
// used to build the "Show jobs" filter checkbox list (matches the lane
// grouping used by the timeline itself, so ticking a box shows/hides one
// whole lane at a time).
export function distinctFilesetClientOptions(jobs) {
  const seen = new Map()
  for (const job of jobs ?? []) {
    const client = job.client ?? ''
    const fileset = job.fileset ?? ''
    if (!client && !fileset) continue
    const key = `${fileset}\u0000${client}`
    if (!seen.has(key)) seen.set(key, { key, fileset, client, label: `${fileset}@${client}` })
  }
  return [...seen.values()].sort((a, b) => a.label.localeCompare(b.label))
}

function timelineDirectorKey(item, multiDirectorTimeline) {
  return multiDirectorTimeline ? (item?.director ?? '') : ''
}

function actualLaneKey(job, multiDirectorTimeline) {
  return [
    timelineDirectorKey(job, multiDirectorTimeline),
    'job',
    job?.name ?? '',
  ].join('\u0000')
}

function scheduledLaneKey(run, multiDirectorTimeline) {
  return [
    timelineDirectorKey(run, multiDirectorTimeline),
    'schedule',
    run?.job ?? run?.name ?? '',
    run?.schedule ?? '',
  ].join('\u0000')
}

function ensureCombinedDirectorGroup(groups, item, multiDirectorTimeline) {
  const directorKey = timelineDirectorKey(item, multiDirectorTimeline)
  if (!groups.has(directorKey)) {
    groups.set(directorKey, {
      director: item?.director ?? null,
      lanes: new Map(),
    })
  }
  return groups.get(directorKey)
}

function ensureCombinedLane(group, key, seed) {
  if (!group.lanes.has(key)) {
    group.lanes.set(key, {
      key,
      type: seed.type,
      name: seed.name ?? '',
      client: seed.client ?? '',
      fileset: seed.fileset ?? '',
      schedule: seed.schedule ?? '',
      director: seed.director ?? null,
      runs: [],
      markers: [],
    })
  }
  return group.lanes.get(key)
}

function normalizeScheduledMarker(run) {
  const runtime = parseSchedulerRuntime(run?.runtime)
  if (runtime === null) return null

  return {
    ...run,
    runtime,
    name: run?.job ?? run?.name ?? '',
    schedule: run?.schedule ?? '',
    scheduleDisplay: run?.scheduleDisplay ?? run?.schedule ?? '',
    director: run?.director ?? null,
  }
}

/**
 * Builds one continuous lane model for actual job runs and scheduler preview
 * triggers. Actual bars are clipped at `now`, while scheduler markers are
 * considered across the full requested [start, end] range so past and future
 * trigger times remain visible.
 */
export function buildCombinedTimelineGroups(
  jobs,
  scheduledRuns,
  { start, end, now, multiDirectorTimeline }
) {
  const groups = new Map()
  const actualLaneByDirectorAndJob = new Map()
  const effectiveNow = Math.min(now, end)

  for (const job of jobs ?? []) {
    const startedAt = parseTimelineTimestamp(job?.starttime)
    const endedAt = parseTimelineTimestamp(job?.endtime) ?? effectiveNow
    if (startedAt === null || endedAt < start || startedAt > effectiveNow) {
      continue
    }

    const group = ensureCombinedDirectorGroup(groups, job, multiDirectorTimeline)
    const laneKey = actualLaneKey(job, multiDirectorTimeline)
    const lane = ensureCombinedLane(group, laneKey, {
      type: 'job',
      name: job?.name ?? '',
      client: job?.client ?? '',
      fileset: job?.fileset ?? '',
      director: job?.director ?? null,
    })
    lane.runs.push(job)

    const directorKey = timelineDirectorKey(job, multiDirectorTimeline)
    actualLaneByDirectorAndJob.set(
      `${directorKey}\u0000${job?.name ?? ''}`,
      laneKey
    )
  }

  for (const rawRun of scheduledRuns ?? []) {
    const marker = normalizeScheduledMarker(rawRun)
    if (!marker || marker.runtime < start || marker.runtime > end) {
      continue
    }

    const group = ensureCombinedDirectorGroup(groups, marker, multiDirectorTimeline)
    const directorKey = timelineDirectorKey(marker, multiDirectorTimeline)
    const matchedLaneKey = actualLaneByDirectorAndJob.get(
      `${directorKey}\u0000${marker.name}`
    )
    const laneKey = matchedLaneKey ?? scheduledLaneKey(marker, multiDirectorTimeline)
    const lane = ensureCombinedLane(group, laneKey, {
      type: matchedLaneKey ? 'job' : 'schedule',
      name: marker.name,
      schedule: marker.scheduleDisplay || marker.schedule,
      director: marker.director,
    })
    lane.markers.push(marker)
  }

  return [...groups.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([, group]) => ({
      director: group.director,
      lanes: [...group.lanes.values()]
        .sort((a, b) => (
          String(a.name).localeCompare(String(b.name))
          || String(a.client).localeCompare(String(b.client))
          || String(a.schedule).localeCompare(String(b.schedule))
        ))
        .map(lane => ({
          ...lane,
          runs: lane.runs.sort(
            (a, b) => (parseTimelineTimestamp(a.starttime) ?? 0)
              - (parseTimelineTimestamp(b.starttime) ?? 0)
          ),
          markers: lane.markers.sort((a, b) => a.runtime - b.runtime),
        })),
    }))
    .filter(group => group.lanes.length > 0)
}
