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
