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

export const useDirectorAclStore = defineStore('directorAcl', () => {
  const director = useDirectorStore()

  const commands = ref(null)
  const loading = ref(false)
  const error = ref(null)

  const allowedCount = computed(() => (
    Object.values(commands.value ?? {}).filter(cmd => cmd?.permission === true).length
  ))
  const deniedCount = computed(() => (
    Object.values(commands.value ?? {}).filter(cmd => cmd?.permission !== true).length
  ))

  function reset() {
    commands.value = null
    loading.value = false
    error.value = null
  }

  async function refresh() {
    if (!director.isConnected) {
      reset()
      error.value = 'Not connected to director'
      return null
    }

    loading.value = true
    error.value = null
    try {
      commands.value = await director.call('.help full=yes')
      return commands.value
    } catch (e) {
      commands.value = null
      error.value = e.message
      return null
    } finally {
      loading.value = false
    }
  }

  async function ensureLoaded() {
    if (commands.value || loading.value) {
      return commands.value
    }
    return refresh()
  }

  watch(() => director.isConnected, (connected) => {
    if (!connected) {
      reset()
    }
  })

  return {
    commands,
    loading,
    error,
    allowedCount,
    deniedCount,
    refresh,
    ensureLoaded,
    reset,
  }
})
