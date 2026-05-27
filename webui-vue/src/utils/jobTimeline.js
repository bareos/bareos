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

function parseTimelineTimestamp(str) {
  if (!str || str.startsWith('0000')) return null
  return new Date(str.replace(' ', 'T')).getTime()
}

function buildClientSpans(rows) {
  const spans = []
  let i = 0
  while (i < rows.length) {
    const client = rows[i].client
    const director = rows[i].director ?? null
    const label = rows[i].clientLabel ?? client
    let count = 0
    while (
      i + count < rows.length
      && rows[i + count].client === client
      && (rows[i + count].director ?? null) === director
    ) {
      count++
    }
    spans.push({ client, director, label, startRow: i, rowCount: count })
    i += count
  }
  return spans
}

export function buildTimelineGroups(jobs, { start, now, multiDirectorTimeline }) {
  const directorGroups = new Map()

  for (const job of jobs ?? []) {
    const startedAt = parseTimelineTimestamp(job.starttime)
    const endedAt = parseTimelineTimestamp(job.endtime) ?? now
    if (startedAt === null || endedAt < start || startedAt > now) continue

    const directorKey = multiDirectorTimeline ? (job.director ?? '') : ''
    if (!directorGroups.has(directorKey)) {
      directorGroups.set(directorKey, {
        director: job.director ?? null,
        clients: new Map(),
      })
    }

    const directorGroup = directorGroups.get(directorKey)
    if (!directorGroup.clients.has(job.client)) {
      directorGroup.clients.set(job.client, {
        client: job.client,
        label: job.client,
        jobs: new Map(),
      })
    }

    const clientGroup = directorGroup.clients.get(job.client)
    if (!clientGroup.jobs.has(job.name)) {
      clientGroup.jobs.set(job.name, {
        name: job.name,
        client: job.client,
        clientLabel: clientGroup.label,
        director: job.director ?? null,
        runs: [],
      })
    }
    clientGroup.jobs.get(job.name).runs.push(job)
  }

  return [...directorGroups.entries()]
    .sort((a, b) => a[0].localeCompare(b[0]))
    .map(([, directorGroup]) => {
      const rows = [...directorGroup.clients.entries()]
        .sort((a, b) => a[0].localeCompare(b[0]))
        .flatMap(([, clientGroup]) => [...clientGroup.jobs.values()]
          .sort((a, b) => a.name.localeCompare(b.name))
          .map(row => ({
            ...row,
            runs: row.runs.sort(
              (a, b) => (parseTimelineTimestamp(a.starttime) ?? 0)
                - (parseTimelineTimestamp(b.starttime) ?? 0)
            ),
          })))

      return {
        director: directorGroup.director,
        rows,
        clientSpans: buildClientSpans(rows),
      }
    })
    .filter(group => group.rows.length > 0)
}
