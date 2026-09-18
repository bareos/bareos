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

export function volumeEncryptionKey(volume) {
  if (!volume || typeof volume !== 'object') {
    return ''
  }

  return String(
    volume.encryptionkey
      ?? volume.EncryptionKey
      ?? volume.encrkey
      ?? volume.EncrKey
      ?? ''
  ).trim()
}

function volumeHasEncryptionKeyFlag(volume) {
  const flag = volume?.hasencryptionkey ?? volume?.HasEncryptionKey

  if (typeof flag === 'boolean') {
    return flag
  }

  if (typeof flag === 'number') {
    return flag !== 0
  }

  if (typeof flag === 'string') {
    const normalized = flag.trim().toLowerCase()
    return normalized === '1' || normalized === 'true' || normalized === 'yes'
  }

  return false
}

export function volumeHasEncryptionKey(volume) {
  return volumeHasEncryptionKeyFlag(volume)
    || volumeEncryptionKey(volume).length > 0
}

function numericValue(value, fallback = 0) {
  const parsed = Number(value)
  return Number.isFinite(parsed) ? parsed : fallback
}

function stringValue(value) {
  return value === null || value === undefined ? '' : String(value).trim()
}

export function volumeUsageSegmentsFromResponse(response) {
  if (Array.isArray(response)) {
    return response
  }

  if (Array.isArray(response?.segments)) {
    return response.segments
  }

  if (Array.isArray(response?.volumeusage?.segments)) {
    return response.volumeusage.segments
  }

  return []
}

export function volumeFileIndexLabel(segment) {
  const first = numericValue(segment?.firstindex)
  const last = numericValue(segment?.lastindex)
  if (first <= 0 && last <= 0) {
    return '—'
  }

  return first === last ? String(first) : `${first}–${last}`
}

export function volumeMediaPositionLabel(segment) {
  const startFile = numericValue(segment?.startfile)
  const startBlock = numericValue(segment?.startblock)
  const endFile = numericValue(segment?.endfile)
  const endBlock = numericValue(segment?.endblock)

  if (startFile <= 0 && startBlock <= 0 && endFile <= 0 && endBlock <= 0) {
    return '—'
  }

  return `${startFile}:${startBlock}–${endFile}:${endBlock}`
}

function segmentFallbackWeight(segment) {
  const first = numericValue(segment?.firstindex)
  const last = numericValue(segment?.lastindex)
  if (last >= first) {
    return Math.max(1, last - first + 1)
  }

  return 1
}

function resolveSegmentWeight(segment, useBytes) {
  const bytes = numericValue(segment?.jobbytes)
  if (useBytes && bytes > 0) {
    return bytes
  }

  const explicitWeight = numericValue(segment?.weight)
  return explicitWeight > 0 ? explicitWeight : segmentFallbackWeight(segment)
}

function mediaPosition(segment, end = false) {
  const file = numericValue(end ? segment?.endfile : segment?.startfile)
  const block = numericValue(end ? segment?.endblock : segment?.startblock)
  return file * 4294967296 + block
}

function mediaEndPosition(segment) {
  return Math.max(mediaPosition(segment, true), mediaPosition(segment))
}

function pctOfMediaRange(position, firstPosition, lastPosition) {
  if (lastPosition <= firstPosition) {
    return 0
  }

  return ((position - firstPosition) / (lastPosition - firstPosition)) * 100
}

function hasOverlappingMediaRanges(segments) {
  if (segments.length < 2) {
    return false
  }

  let currentEnd = mediaEndPosition(segments[0])
  for (let i = 1; i < segments.length; i += 1) {
    if (mediaPosition(segments[i]) <= currentEnd) {
      return true
    }
    currentEnd = Math.max(currentEnd, mediaEndPosition(segments[i]))
  }

  return false
}

export function buildVolumeTapeSegments(usageRows, jobs = []) {
  if (!Array.isArray(usageRows) || usageRows.length === 0) {
    return []
  }

  const jobsById = new Map(
    (Array.isArray(jobs) ? jobs : [])
      .map(job => [String(job?.jobid ?? job?.id ?? ''), job])
      .filter(([id]) => id)
  )
  const useBytes = usageRows.some(row => numericValue(row?.jobbytes) > 0)

  const colorByJobId = new Map()
  const normalized = usageRows.map((row, index) => {
    const jobId = stringValue(row?.jobid)
    const job = jobsById.get(jobId)
    if (!colorByJobId.has(jobId)) {
      colorByJobId.set(jobId, colorByJobId.size)
    }
    const segment = {
      key: stringValue(row?.jobmediaid) || `${jobId || 'segment'}-${index}`,
      marker: stringValue(row?.marker) || String(index + 1),
      jobid: jobId,
      name: stringValue(row?.job ?? row?.name ?? job?.name),
      client: stringValue(row?.client ?? job?.client),
      status: stringValue(row?.jobstatus ?? job?.jobstatus ?? job?.status),
      volumename: stringValue(row?.volumename),
      firstindex: numericValue(row?.firstindex),
      lastindex: numericValue(row?.lastindex),
      startfile: numericValue(row?.startfile),
      startblock: numericValue(row?.startblock),
      endfile: numericValue(row?.endfile),
      endblock: numericValue(row?.endblock),
      jobbytes: numericValue(row?.jobbytes),
    }
    segment.weight = resolveSegmentWeight({ ...row, ...segment }, useBytes)
    segment.fileIndexLabel = volumeFileIndexLabel(segment)
    segment.mediaPositionLabel = volumeMediaPositionLabel(segment)
    return segment
  }).sort((a, b) => (
    a.startfile - b.startfile
      || a.startblock - b.startblock
      || numericValue(a.key) - numericValue(b.key)
      || numericValue(a.jobid) - numericValue(b.jobid)
  ))

  const totalWeight = normalized.reduce((sum, segment) => sum + segment.weight, 0) || 1
  const firstPosition = Math.min(...normalized.map(segment => mediaPosition(segment)))
  const lastPosition = Math.max(...normalized.map(segment => mediaEndPosition(segment)))
  const hasOverlaps = hasOverlappingMediaRanges(normalized)

  return normalized.map((segment, index) => ({
    ...segment,
    pct: (segment.weight / totalWeight) * 100,
    colorIndex: colorByJobId.get(segment.jobid) ?? index,
    leftPct: pctOfMediaRange(mediaPosition(segment), firstPosition, lastPosition),
    widthPct: Math.max(
      0.5,
      pctOfMediaRange(mediaEndPosition(segment), firstPosition, lastPosition)
        - pctOfMediaRange(mediaPosition(segment), firstPosition, lastPosition)
    ),
    hasOverlaps,
  }))
}

export function buildVolumeDetailsQuery({
  director,
  directorTab,
  directorTarget,
  jobId,
  poolName,
  storagesTab,
  storagesScopeDirector,
} = {}) {
  const query = {}

  if (director) {
    query.director = director
  }

  if (directorTab && directorTab !== 'status') {
    query.directorTab = directorTab
  }

  if (directorTarget) {
    query.directorTarget = directorTarget
  }

  if (jobId !== null && jobId !== undefined && jobId !== '') {
    query.jobId = String(jobId)
  }

  if (poolName) {
    query.poolName = poolName
  }

  if (storagesTab) {
    query.storagesTab = storagesTab
  }

  if (storagesScopeDirector) {
    query.storagesScopeDirector = storagesScopeDirector
  }

  return query
}

export function resolveVolumeDetailsDirectorOrigin(query) {
  if (typeof query?.directorTab !== 'string' || !query.directorTab) {
    return null
  }

  return {
    tab: query.directorTab,
    targetDirector: typeof query?.directorTarget === 'string' ? query.directorTarget : '',
  }
}

export function resolveVolumeDetailsPoolOrigin(query) {
  if (typeof query?.poolName !== 'string' || !query.poolName) {
    return null
  }

  return {
    name: query.poolName,
  }
}

export function resolveVolumeDetailsJobOrigin(query) {
  if (typeof query?.jobId !== 'string' || !query.jobId) {
    return null
  }

  return {
    id: query.jobId,
  }
}

export function resolveVolumeDetailsStoragesOrigin(query) {
  if (typeof query?.storagesTab !== 'string' || !query.storagesTab) {
    return null
  }

  return {
    tab: query.storagesTab,
    scopeDirector: typeof query?.storagesScopeDirector === 'string'
      ? query.storagesScopeDirector
      : '',
  }
}
