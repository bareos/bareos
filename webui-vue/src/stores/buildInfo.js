/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
import {
  BUILD_KINDS,
  classifyBinaryInfo,
  loadCachedBuildKind,
  loadRibbonDismissed,
  shouldPromote,
  storeBuildKind,
  storeRibbonDismissed,
  summariseBuildKinds,
} from '../utils/commercialOffering.js'

/*
 * Build kind of the connected directors, fed from "status director"
 * results the WebUI fetches anyway, so no extra director connections
 * are opened for it.
 */
export const useBuildInfoStore = defineStore('build-info', () => {
  const kindsByDirector = ref({})
  const cachedKind = ref(loadCachedBuildKind())
  const ribbonDismissed = ref(loadRibbonDismissed())

  const kinds = computed(() => Object.values(kindsByDirector.value))
  const kind = computed(() => (
    kinds.value.length ? summariseBuildKinds(kinds.value) : cachedKind.value
  ))
  const promote = computed(() => (
    kinds.value.length ? shouldPromote(kinds.value) : shouldPromote([cachedKind.value])
  ))

  function record(director, binaryInfo) {
    if (!director || binaryInfo === undefined) return
    const next = classifyBinaryInfo(binaryInfo)
    if (kindsByDirector.value[director] === next) return
    kindsByDirector.value = { ...kindsByDirector.value, [director]: next }
    cachedKind.value = summariseBuildKinds(kinds.value)
    storeBuildKind(cachedKind.value)
  }

  function recordStatus(director, status) {
    record(director, status?.header?.binary_info)
  }

  /** Forget directors that are no longer part of the session. */
  function retain(directors) {
    const keep = new Set(directors ?? [])
    const next = Object.fromEntries([...keep].map(name => [
      name,
      kindsByDirector.value[name] ?? BUILD_KINDS.unknown,
    ]))
    if (
      Object.keys(next).length !== kinds.value.length
      || Object.entries(next).some(([name, kind]) => kindsByDirector.value[name] !== kind)
    ) {
      kindsByDirector.value = next
    }
  }

  function setRibbonDismissed(dismissed) {
    ribbonDismissed.value = Boolean(dismissed)
    storeRibbonDismissed(ribbonDismissed.value)
  }

  return {
    kindsByDirector, kind, promote, ribbonDismissed,
    record, recordStatus, retain, setRibbonDismissed,
  }
})
