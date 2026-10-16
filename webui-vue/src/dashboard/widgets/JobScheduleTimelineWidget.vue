<!--
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
-->
<template>
  <CombinedJobScheduleTimeline
    :actual-jobs="actualJobs"
    :scheduled-runs="scheduledRuns"
    :loading="loading"
    :error="error"
    :directors="activeDirectors"
    @range-change="onRangeChange"
    @refresh="refresh"
  />
</template>

<script setup>
import { computed, inject, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { fetchCombinedTimelineData } from '../../composables/combinedTimeline.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { useAuthStore } from '../../stores/auth.js'
import CombinedJobScheduleTimeline from '../../components/CombinedJobScheduleTimeline.vue'

const { t } = useI18n()
const auth = useAuthStore()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const activeDirectors = computed(() => ctx?.activeDirectors?.value ?? [])
const actualJobs = ref([])
const scheduledRuns = ref([])
const loading = ref(false)
const error = ref('')
const currentRange = ref(null)
let refreshRunning = false
let refreshQueued = false

async function refresh() {
  if (refreshRunning) {
    refreshQueued = true
    return
  }

  refreshRunning = true
  loading.value = true
  try {
    do {
      refreshQueued = false
      await refreshCurrentRange()
    } while (refreshQueued)
  } finally {
    refreshRunning = false
    loading.value = false
  }
}

async function refreshCurrentRange() {
  const range = currentRange.value
  if (!range || activeDirectors.value.length === 0) {
    actualJobs.value = []
    scheduledRuns.value = []
    return
  }

  const credentials = auth.getCredentials()
  if (!credentials?.password) {
    error.value = t('Not logged in.')
    return
  }

  error.value = ''
  const directors = [...activeDirectors.value]
  try {
    const result = await fetchCombinedTimelineData(
      credentials,
      directors,
      {
        jobDays: range.jobDays,
        schedulerRange: range.schedulerRange,
      }
    )
    actualJobs.value = result.actualJobs
    scheduledRuns.value = result.scheduledRuns
    error.value = result.errors.length
      ? result.errors.join('\n')
      : ''
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  }
}

function onRangeChange(range) {
  currentRange.value = range
  refresh()
}

watch(
  () => [activeDirectors.value.join('\0'), ctx?.refreshToken?.value],
  refresh
)
</script>
