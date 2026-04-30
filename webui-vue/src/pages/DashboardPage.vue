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
  <q-page class="q-pa-md">
    <q-card flat bordered class="q-mb-md bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Dashboard Scope') }}</span>
        <q-space />
        <q-chip dense square color="white" text-color="primary" :label="dashboardScopeLabel" />
      </q-card-section>
      <q-card-section>
        <q-select
          v-model="selectedDirectorsModel"
          data-testid="dashboard-directors"
          :options="directorOptions"
          option-label="label"
          option-value="value"
          emit-value
          map-options
          multiple
          use-chips
          outlined
          dense
          :label="t('Directors')"
        />
        <div class="text-caption text-grey-6 q-mt-sm">
          {{ t('Select the directors that contribute to the common dashboard.') }}
        </div>
        <q-banner
          v-if="directorErrors.length"
          rounded
          dense
          class="bg-warning text-black q-mt-md"
        >
          <template #avatar>
            <q-icon name="warning" />
          </template>
          <div v-for="item in directorErrors" :key="item.director">
            <strong>{{ item.director }}</strong>: {{ item.message }}
          </div>
        </q-banner>
      </q-card-section>
    </q-card>

    <div class="row q-col-gutter-md">
      <!-- Left column: 8/12 -->
      <div class="col-12 col-md-8">
        <!-- Jobs Past 24h stat row -->
        <q-card flat bordered class="q-mb-md bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Jobs started during the past 24 hours') }}</span>
            <q-space />
            <span class="text-white text-caption q-mr-sm" style="opacity:0.7">↻ {{ countdown }}s</span>
            <q-spinner v-if="loadingJobs" color="white" size="18px" class="q-mr-sm" />
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="manualRefresh" />
          </q-card-section>
          <q-card-section>
            <div class="row q-col-gutter-md text-center">
              <div class="col" v-for="s in summaryStats" :key="s.label">
                <router-link
                  :to="{ name: 'jobs', query: withJobsStatusFilterQuery({}, s.status) }"
                  class="text-decoration-none"
                >
                  <StatNumber :value="s.count" :label="s.label" :color="s.color" />
                </router-link>
              </div>
            </div>
          </q-card-section>
        </q-card>

        <!-- Jobs Last Status table -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Most recent job status per job name') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="manualRefresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="recentJobs"
              :columns="recentCols"
              row-key="scopeKey"
              dense flat
              :pagination="{ rowsPerPage: 10, sortBy: 'starttime', descending: true }"
            >
              <template #body-cell-id="props">
                <q-td :props="props" class="text-right">
                  <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <q-chip dense square color="primary" text-color="white" :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="jobStatus(props.row)" />
                </q-td>
              </template>
              <template #body-cell-client="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openClientDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-starttime="props">
                <q-td :props="props">
                  <span :title="settings.relativeTime ? props.value : timeAgo(props.value, settings.locale)">
                    {{ settings.relativeTime ? timeAgo(props.value, settings.locale) : props.value }}
                  </span>
                </q-td>
              </template>
              <template #body-cell-name="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-level="props">
                <q-td :props="props" class="text-center">
                  <JobLevelBadge v-if="props.value" :level="props.value" />
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right" style="min-width:90px">
                  <div>{{ jobBytes(props.row) }}</div>
                  <q-linear-progress
                    v-if="props.row.status === 'R'"
                    indeterminate color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                  <q-linear-progress
                    v-else
                    :value="bytesGauge(props.row.bytes)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-duration="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ props.value || '—' }}</div>
                  <q-linear-progress
                    :value="durationGauge(props.value)"
                    color="orange" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-speed="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <span v-if="props.row.status === 'R'" class="text-grey-5">—</span>
                  <template v-else>
                    <div>{{ fmtSpeed(props.row.bytes, props.row.duration) }}</div>
                    <q-linear-progress
                      :value="speedGauge(props.row)"
                      color="cyan-7" track-color="grey-3"
                      size="4px" class="q-mt-xs" rounded
                    />
                  </template>
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </div>

      <!-- Right column: 4/12 -->
      <div class="col-12 col-md-4">
        <!-- Job Totals -->
        <q-card flat bordered class="q-mb-md bareos-panel">
          <q-card-section class="panel-header">{{ t('Job Totals') }}</q-card-section>
          <q-card-section class="q-pa-sm">
            <div class="row q-gutter-sm">
              <div v-for="stat in totalStats" :key="stat.label" class="col-auto">
                <div class="text-caption text-grey-6" style="white-space:nowrap">{{ stat.label }}</div>
                <div class="text-weight-bold" style="font-size:1rem; line-height:1.2">{{ stat.value }}</div>
              </div>
            </div>
          </q-card-section>
        </q-card>

        <!-- Running Jobs -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Running') }} {{ t('Jobs') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="manualRefresh" />
          </q-card-section>
          <q-card-section style="max-height:320px; overflow-y:auto; padding:0">
            <q-list separator>
              <q-item v-for="job in runningJobs" :key="job.scopeKey" class="q-py-sm">
                <q-item-section>
                  <q-item-label>
                    <a href="#" class="text-primary text-weight-medium" @click.prevent="openJobDetails(job)">
                      {{ job.name }}
                    </a>
                    <q-chip
                      v-if="showDirectorColumn"
                      dense
                      square
                      color="primary"
                      text-color="white"
                      size="sm"
                      class="q-ml-xs"
                      :label="job.director"
                    />
                    <span class="text-grey-6 text-caption q-ml-xs">({{ job.client }})</span>
                  </q-item-label>
                  <q-item-label caption>
                     {{ formatNumber(job.files ?? 0, settings.locale) }} {{ t('Files') }} &middot; {{ jobBytes(job) }} &middot; {{ formatDuration(elapsedSecs(job)) }}
                  </q-item-label>
                  <q-linear-progress indeterminate color="positive" class="q-mt-xs" style="height:6px; border-radius:3px" />
                </q-item-section>
                <q-item-section side>
                  <q-btn
                    v-if="allowRunningJobActions"
                    flat
                    round
                    dense
                    icon="cancel"
                    color="negative"
                    size="sm"
                    :title="t('Cancel Job')"
                    @click="confirmCancel(job)"
                  />
                </q-item-section>
              </q-item>
              <q-item v-if="!runningJobs.length">
                <q-item-section class="text-grey text-caption text-center q-py-md">{{ t('No running jobs') }}</q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { formatBytes, formatSpeed, parseDurationSecs, timeAgo, formatDuration } from '../mock/index.js'
import {
  aggregateDirectorDashboardSnapshots,
  fetchDirectorDashboardSnapshot,
} from '../composables/directorAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useDirectorStore } from '../stores/director.js'
import { useAuthStore } from '../stores/auth.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildClientDetailsQuery } from '../utils/clients.js'
import { buildJobDetailsQuery, withJobsStatusFilterQuery } from '../utils/jobs.js'
import { formatNumber } from '../utils/locales.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'
import StatNumber from '../components/StatNumber.vue'

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const router = useRouter()
const { t } = useI18n()
const fmtBytes = formatBytes
const fmtSpeed = formatSpeed

// ── data ─────────────────────────────────────────────────────────────────────
const dashboardSnapshots = ref([])
const directorErrors = ref([])
const loadingJobs = ref(false)

const directorOptions = computed(() => {
  const values = new Set([
    ...director.availableDirectors,
    ...settings.selectedDirectors,
    auth.user?.director,
    settings.directorName,
  ].filter(Boolean))

  return [...values].map(value => ({ label: value, value }))
})

function syncSelectedDirectors() {
  const validDirectors = directorOptions.value.map(option => option.value)
  const selected = settings.selectedDirectors.filter(value => validDirectors.includes(value))

  if (selected.length > 0) {
    if (selected.length !== settings.selectedDirectors.length) {
      settings.setSelectedDirectors(selected)
    }
    return
  }

  if (validDirectors.length > 0) {
    settings.setSelectedDirectors(validDirectors)
  }
}

const selectedDirectorsModel = computed({
  get: () => settings.selectedDirectors,
  set: (value) => {
    const selected = Array.isArray(value) ? value : []
    if (selected.length > 0) {
      settings.setSelectedDirectors(selected)
      return
    }

    const fallbackDirector = auth.user?.director || settings.directorName
    settings.setSelectedDirectors(fallbackDirector ? [fallbackDirector] : [])
  },
})

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    directorOptions.value.some(option => option.value === value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isCommonDashboard = computed(() => activeDirectors.value.length > 1)
const showDirectorColumn = computed(() => isCommonDashboard.value)
const allowRunningJobActions = computed(() => activeDirectors.value.length === 1)
const dashboardScopeLabel = computed(() => (
  isCommonDashboard.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))

async function fetchDashboard() {
  const credentials = auth.getCredentials()
  if (!credentials || activeDirectors.value.length === 0) {
    dashboardSnapshots.value = []
    directorErrors.value = []
    return
  }

  loadingJobs.value = true
  try {
    const results = await Promise.allSettled(activeDirectors.value.map(currentDirector => (
      fetchDirectorDashboardSnapshot({
        ...credentials,
        director: currentDirector,
      })
    )))

    dashboardSnapshots.value = results
      .filter(result => result.status === 'fulfilled')
      .map(result => result.value)
    directorErrors.value = results.flatMap((result, index) => (
      result.status === 'rejected'
        ? [{
          director: activeDirectors.value[index],
          message: result.reason?.message ?? t('Failed to load dashboard data.'),
        }]
        : []
    ))
  } finally {
    loadingJobs.value = false
  }
}

async function loadAvailableDirectors() {
  try {
    await director.fetchAvailableDirectors()
  } catch {
    // Keep the selector usable with the active director when the proxy list is
    // unavailable.
  }
}

let refreshPromise = null
let refreshQueued = false

async function runRefreshBatch() {
  await fetchDashboard()
}

function refresh() {
  if (refreshPromise) {
    refreshQueued = true
    return refreshPromise
  }

  refreshPromise = (async () => {
    do {
      refreshQueued = false
      await runRefreshBatch()
    } while (refreshQueued)
  })().finally(() => {
    refreshPromise = null
  })

  return refreshPromise
}

function confirmCancel(job) {
  $q.dialog({
    title: 'Cancel Job',
    message: t('Cancel job {name} (ID {id})?', { name: job.name, id: jobId(job) }),
    ok: { label: t('Cancel Job'), color: 'negative', flat: true },
    cancel: { label: t('Keep Running'), flat: true },
  }).onOk(() => doCancel(job))
}

async function openJobDetails(row) {
  try {
    await switchActiveDirector(row.director)
    await router.push({
      name: 'job-details',
      params: { id: jobId(row) },
      query: buildJobDetailsQuery({
        director: row.director,
        dashboardOrigin: true,
      }),
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not switch to director {director}: {message}', {
        director: row.director ?? t('unknown'),
        message: error.message,
      }),
    })
  }
}

async function openClientDetails(row) {
  try {
    await switchActiveDirector(row.director)
    await router.push({
      name: 'client-details',
      params: { name: row.client },
      query: buildClientDetailsQuery({
        director: row.director,
        dashboardOrigin: true,
      }),
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not switch to director {director}: {message}', {
        director: row.director ?? t('unknown'),
        message: error.message,
      }),
    })
  }
}

async function doCancel(job) {
  try {
    await switchActiveDirector(job.director)
    await director.call(`cancel jobid=${jobId(job)} yes`)
    $q.notify({ type: 'positive', message: t('Job {id} cancelled.', { id: jobId(job) }) })
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: t('Cancel failed: {message}', { message: e.message }) })
  }
}

const now = ref(Date.now())

onMounted(async () => {
  await loadAvailableDirectors()
  syncSelectedDirectors()
  refresh()
})

// ── auto-refresh ──────────────────────────────────────────────────────────────
const countdown = ref(settings.refreshInterval)
let _countdownTimer = null
let _refreshTimer = null

function startAutoRefresh() {
  stopAutoRefresh()
  countdown.value = settings.refreshInterval
  _countdownTimer = setInterval(() => {
    now.value = Date.now()
    countdown.value -= 1
    if (countdown.value <= 0) {
      refresh()
      countdown.value = settings.refreshInterval
    }
  }, 1000)
}

function stopAutoRefresh() {
  clearInterval(_countdownTimer)
  clearInterval(_refreshTimer)
  _countdownTimer = null
  _refreshTimer = null
}

function manualRefresh() {
  refresh()
  countdown.value = settings.refreshInterval
}

onMounted(startAutoRefresh)
onUnmounted(stopAutoRefresh)
watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
}, { deep: true })
watch(() => activeDirectors.value.join('\u0000'), () => {
  countdown.value = settings.refreshInterval
  refresh()
})

// ── computed views ────────────────────────────────────────────────────────────
const aggregate = computed(() => (
  aggregateDirectorDashboardSnapshots(dashboardSnapshots.value)
))
const past24hJobs = computed(() => aggregate.value.jobsPast24h)
const lastJobs = computed(() => aggregate.value.recentJobs)

const recentCols = computed(() => {
  const columns = [
    { name: 'id', label: 'ID', field: 'id', align: 'right', sortable: true },
    { name: 'name', label: t('Job Name'), field: 'name', align: 'left', sortable: true },
    { name: 'client', label: t('Client'), field: 'client', align: 'left', sortable: true },
    { name: 'level', label: t('Level'), field: 'level', align: 'center' },
    { name: 'starttime', label: t('Start'), field: 'starttime', align: 'left', sortable: true },
    {
      name: 'duration',
      label: t('Duration'),
      field: 'duration',
      align: 'right',
      sortable: true,
      sort: (a, b) => parseDurationSecs(a) - parseDurationSecs(b),
    },
    { name: 'bytes', label: t('Bytes'), field: 'bytes', align: 'right', sortable: true },
    { name: 'speed', label: t('Speed'), field: 'speed', align: 'right' },
    { name: 'status', label: t('Status'), field: 'status', align: 'center' },
  ]

  if (showDirectorColumn.value) {
    columns.splice(1, 0, {
      name: 'director',
      label: t('Director'),
      field: 'director',
      align: 'left',
      sortable: true,
    })
  }

  return columns
})

const recentJobs = computed(() => lastJobs.value)
const runningJobs = computed(() => aggregate.value.runningJobs)

function elapsedSecs(job) {
  if (!job.starttime) return 0
  const start = new Date(job.starttime.replace(' ', 'T')).getTime()
  if (isNaN(start)) return 0
  return Math.max(0, Math.floor((now.value - start) / 1000))
}

const maxBytes = computed(() => Math.max(1, ...recentJobs.value.map(j => j.bytes)))
const maxDurationSecs = computed(() => Math.max(1, ...recentJobs.value.map(j => parseDurationSecs(j.duration))))
function jobSpeedBps(row) {
  const secs = parseDurationSecs(row.duration)
  if (!secs) return 0
  const bytes = typeof row.bytes === 'string' ? parseFloat(row.bytes) : (row.bytes || 0)
  return bytes / secs
}
const maxSpeedBps = computed(() => Math.max(1, ...recentJobs.value
  .filter(j => j.status !== 'R')
  .map(j => jobSpeedBps(j))))
function bytesGauge(val) { return val / maxBytes.value }
function durationGauge(str) { return parseDurationSecs(str) / maxDurationSecs.value }
function speedGauge(row) { return jobSpeedBps(row) / maxSpeedBps.value }

const summaryStats = computed(() => {
  const countByStatus = (code) => past24hJobs.value.filter(j => j.status === code).length
  return [
    { label: t('Running'), status: 'R', color: 'info', count: countByStatus('R') },
    { label: t('Waiting'), status: 'C', color: 'grey', count: countByStatus('C') },
    { label: t('Successful'), status: 'T', color: 'positive', count: countByStatus('T') },
    { label: t('Warning'), status: 'W', color: 'warning', count: countByStatus('W') },
    { label: t('Failed'), status: 'f', color: 'negative', count: countByStatus('f') },
  ]
})

const totals = computed(() => ({
  jobs: aggregate.value.jobTotals.jobs,
  files: aggregate.value.jobTotals.files,
  bytes: aggregate.value.jobTotals.bytes,
  clients: aggregate.value.clientCount,
  storages: aggregate.value.storageCount,
}))

const totalStats = computed(() => [
  { label: t('Total Jobs'), value: totals.value.jobs },
  { label: t('Total Files'), value: formatNumber(totals.value.files ?? 0, settings.locale) },
  { label: t('Total Bytes'), value: fmtBytes(totals.value.bytes ?? 0) },
  { label: t('Clients'), value: totals.value.clients },
  { label: t('Storages'), value: totals.value.storages },
])

// helper: keep status/bytes getters for template compatibility
function jobStatus(row) { return row.status ?? '?' }
function jobBytes(row) { return fmtBytes(row.bytes ?? 0) }
function jobId(row) {
  return row.id ?? row.jobid
}
</script>
