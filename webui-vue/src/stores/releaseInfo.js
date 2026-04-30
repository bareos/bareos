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

import { computed, ref } from 'vue'
import { defineStore } from 'pinia'

export const RELEASE_INFO_URL = 'https://download.bareos.com/release-info/bareos-version-info.js'
export const RELEASE_INFO_PAGE_URL = 'https://download.bareos.com/'

const FETCH_TIMEOUT_MS = 10_000
const UNKNOWN_MESSAGE = 'Update information could not be retrieved'

function tokeniseVersion(version) {
  return String(version ?? '')
    .toLowerCase()
    .match(/[0-9]+|[a-z]+/g) ?? []
}

function compareVersionTokens(left, right) {
  const max = Math.max(left.length, right.length)
  for (let i = 0; i < max; i += 1) {
    const a = left[i]
    const b = right[i]
    if (a == null && b == null) return 0
    if (a == null) return -1
    if (b == null) return 1

    const aIsNumber = /^[0-9]+$/.test(a)
    const bIsNumber = /^[0-9]+$/.test(b)
    if (aIsNumber && bIsNumber) {
      const diff = Number(a) - Number(b)
      if (diff !== 0) return diff > 0 ? 1 : -1
      continue
    }
    if (aIsNumber !== bIsNumber) return aIsNumber ? 1 : -1
    if (a !== b) return a > b ? 1 : -1
  }
  return 0
}

export function compareBareosVersions(left, right) {
  return compareVersionTokens(tokeniseVersion(left), tokeniseVersion(right))
}

export function normaliseReleaseInfoEntry(entry) {
  if (!entry || typeof entry !== 'object') {
    throw new Error('Release metadata contains an invalid entry')
  }
  if (typeof entry.version !== 'string' || !entry.version.trim()) {
    throw new Error('Release metadata entry misses its version')
  }

  return {
    version: entry.version.trim(),
    status: typeof entry.status === 'string' && entry.status.trim()
      ? entry.status.trim()
      : 'unknown',
    package_update_info: typeof entry.package_update_info === 'string'
      ? entry.package_update_info.trim()
      : '',
  }
}

export function parseReleaseInfoPayload(payload) {
  const text = String(payload ?? '').trim()
  if (!text) {
    throw new Error('Release metadata is empty')
  }

  let parsed
  if (text.startsWith('[') || text.startsWith('{')) {
    parsed = JSON.parse(text)
  } else {
    const match = text.match(/^\s*(?:[A-Za-z_$][\w$]*\s*)?\(([\s\S]*)\)\s*;?\s*$/)
    if (!match) {
      throw new Error('Unexpected release metadata format')
    }
    parsed = JSON.parse(match[1])
  }

  const entries = Array.isArray(parsed) ? parsed : parsed?.versions
  if (!Array.isArray(entries)) {
    throw new Error('Release metadata does not contain a version list')
  }

  return entries.map(normaliseReleaseInfoEntry)
}

export function getNearestVersionInfo(entries, version) {
  if (!version) return null
  for (const entry of entries) {
    if (compareBareosVersions(version, entry.version) >= 0) {
      return {
        ...entry,
        requested_version: version,
      }
    }
  }
  return null
}

function getUnknownVersionInfo(version, reason = UNKNOWN_MESSAGE) {
  return {
    version: version ?? '',
    requested_version: version ?? '',
    status: 'unknown',
    package_update_info: reason,
  }
}

export const useReleaseInfoStore = defineStore('release-info', () => {
  const entries = ref([])
  const loading = ref(false)
  const error = ref(null)
  const lastFetchedAt = ref(null)
  let pending = null

  async function refresh(force = false) {
    if (!force && entries.value.length) return entries.value
    if (pending) return pending

    pending = (async () => {
      loading.value = true
      error.value = null

      const controller = new AbortController()
      const timeout = setTimeout(() => controller.abort(), FETCH_TIMEOUT_MS)

      try {
        const response = await fetch(RELEASE_INFO_URL, {
          method: 'GET',
          mode: 'cors',
          cache: 'no-store',
          signal: controller.signal,
        })
        if (!response.ok) {
          throw new Error(`Release metadata request failed (${response.status})`)
        }
        const payload = await response.text()
        entries.value = parseReleaseInfoPayload(payload)
        lastFetchedAt.value = Date.now()
        return entries.value
      } catch (e) {
        entries.value = []
        error.value = e?.name === 'AbortError'
          ? 'Release metadata request timed out'
          : (e?.message ?? String(e))
        throw e
      } finally {
        clearTimeout(timeout)
        loading.value = false
        pending = null
      }
    })()

    return pending
  }

  function getVersionInfo(version) {
    if (!version) return getUnknownVersionInfo('', 'Version information is unavailable')
    if (!entries.value.length) return getUnknownVersionInfo(version, error.value ?? UNKNOWN_MESSAGE)
    return getNearestVersionInfo(entries.value, version)
      ?? getUnknownVersionInfo(version, 'No release metadata matched this version')
  }

  const hasMetadata = computed(() => entries.value.length > 0)

  return {
    entries,
    error,
    hasMetadata,
    lastFetchedAt,
    loading,
    refresh,
    getVersionInfo,
  }
})
