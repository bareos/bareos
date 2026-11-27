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

import { defineStore } from 'pinia'
import { computed, ref, watch } from 'vue'
import { useDirectorStore } from './director.js'

// The Bareos documentation base URL. dird's ".pluginhints" emits
// manual paths relative to this (e.g. "TasksAndConcepts/Plugins.html#..."),
// matching the pre-refactor static data/restorePluginHints.js dataset.
const manualBaseUrl = 'https://docs.bareos.org/master'

function resolveManualUrl(manualUrl) {
  const path = String(manualUrl ?? '').trim()
  if (!path) {
    return ''
  }
  if (/^[a-z][a-z0-9+.-]*:/i.test(path)) {
    // Already an absolute URL (e.g. a custom http(s):// link).
    return path
  }
  return `${manualBaseUrl}/${path}`
}

// Converts one ".pluginhints" hint object (as emitted by dird's
// EmitPluginRestoreHintFields(), all-lowercase JSON keys) into the
// camelCase shape the frontend (restore.js / PluginRestoreInfoPanel.vue)
// already expects -- the same shape the old static
// data/restorePluginHints.js dataset used.
function normalizeHint(rawHint) {
  const id = String(rawHint?.id ?? '').trim()
  if (!id) {
    return null
  }

  return [id, {
    displayName: rawHint?.displayname ?? '',
    manualUrl: resolveManualUrl(rawHint?.manualurl),
    optionSeparator: rawHint?.optionseparator || ':',
    note: rawHint?.note ?? '',
    supportLevel: rawHint?.supportlevel || 'bareos',
    aliases: Array.isArray(rawHint?.aliases) ? rawHint.aliases : [],
    options: Array.isArray(rawHint?.options)
      ? rawHint.options.map(option => ({
        name: option?.name ?? '',
        status: option?.status ?? 'known',
        description: option?.description ?? '',
        source: option?.source ?? 'plugin-doc',
        type: option?.type ?? 'string',
        providesDefaults: option?.providesdefaults === true,
      }))
      : [],
  }]
}

// Builds the { id: hint } map the rest of the frontend consumes from the
// director's ".pluginhints" response ({ hints: [...] }).
export function normalizePluginHintsResponse(response) {
  const rawHints = Array.isArray(response?.hints) ? response.hints : []
  return Object.fromEntries(
    rawHints
      .map(normalizeHint)
      .filter(Boolean)
  )
}

export const usePluginHintsStore = defineStore('pluginHints', () => {
  const director = useDirectorStore()

  const hintsById = ref(null)
  const loading = ref(false)
  const error = ref(null)

  // Bumped on every reset()/disconnect so a stale in-flight refresh()
  // (from a request started before a reset or director switch) can
  // detect it is no longer current and avoid clobbering newer state.
  let requestToken = 0
  let pendingRefresh = null

  function reset() {
    requestToken += 1
    pendingRefresh = null
    hintsById.value = null
    loading.value = false
    error.value = null
  }

  async function refresh() {
    if (!director.isConnected) {
      reset()
      error.value = 'Not connected to director'
      return null
    }

    const token = ++requestToken
    loading.value = true
    error.value = null
    const promise = (async () => {
      try {
        const response = await director.call('.pluginhints')
        if (token !== requestToken) {
          // A reset/disconnect happened while this request was in
          // flight; discard the result instead of overwriting newer
          // (or reset) state.
          return hintsById.value
        }
        hintsById.value = normalizePluginHintsResponse(response)
        return hintsById.value
      } catch (e) {
        if (token !== requestToken) {
          return hintsById.value
        }
        hintsById.value = null
        error.value = e.message
        return null
      } finally {
        if (token === requestToken) {
          loading.value = false
        }
        if (pendingRefresh === promise) {
          pendingRefresh = null
        }
      }
    })()
    pendingRefresh = promise
    return promise
  }

  async function ensureLoaded() {
    if (hintsById.value) {
      return hintsById.value
    }
    if (pendingRefresh) {
      return pendingRefresh
    }
    return refresh()
  }

  const hints = computed(() => hintsById.value ?? {})

  watch(() => director.isConnected, (connected) => {
    if (!connected) {
      reset()
    }
  })

  return {
    hintsById,
    hints,
    loading,
    error,
    refresh,
    ensureLoaded,
    reset,
  }
})
