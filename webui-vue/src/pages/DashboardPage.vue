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
                <router-link :to="{ name: 'jobs', query: { status: s.status } }" class="text-decoration-none">
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
              row-key="id"
              dense flat
              :pagination="{ rowsPerPage: 10, sortBy: 'id', descending: true }"
            >
              <template #body-cell-id="props">
                <q-td :props="props" class="text-right">
                  <router-link :to="{ name: 'job-details', params: { id: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="jobStatus(props.row)" />
                </q-td>
              </template>
              <template #body-cell-client="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'client-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
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
                  <router-link :to="{ name: 'job-details', params: { id: jobId(props.row) } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
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
              <q-item v-for="job in runningJobs" :key="jobId(job)" class="q-py-sm">
                <q-item-section>
                  <q-item-label>
                    <router-link :to="{ name: 'job-details', params: { id: jobId(job) } }" class="text-primary text-weight-medium">
                      {{ job.name }}
                    </router-link>
                    <span class="text-grey-6 text-caption q-ml-xs">({{ job.client }})</span>
                  </q-item-label>
                  <q-item-label caption>
                     {{ formatNumber(job.files ?? 0, settings.locale) }} {{ t('Files') }} &middot; {{ jobBytes(job) }} &middot; {{ formatDuration(elapsedSecs(job)) }}
                  </q-item-label>
                  <q-linear-progress indeterminate color="positive" class="q-mt-xs" style="height:6px; border-radius:3px" />
                </q-item-section>
                <q-item-section side>
                  <q-btn flat round dense icon="cancel" color="negative" size="sm"
                         :title="t('Cancel Job')" @click="confirmCancel(job)" />
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
import { useI18n } from 'vue-i18n'
import { formatBytes, formatSpeed, parseDurationSecs, timeAgo, formatDuration } from '../mock/index.js'
import { directorCollection, normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { formatNumber } from '../utils/locales.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'
import StatNumber from '../components/StatNumber.vue'

const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const { t } = useI18n()
const fmtBytes      = formatBytes
const fmtSpeed      = formatSpeed

// ── data ─────────────────────────────────────────────────────────────────────
const rawPast24hJobs = ref([])
const rawRunningJobs = ref([])
const rawLastJobs    = ref([])
const clientCount    = ref(0)
const storageCount   = ref(0)
const jobTotals      = ref(null)
const loadingJobs    = ref(false)

async function fetchJobs() {
  if (!director.isConnected) return
  loadingJobs.value = true
  try {
    const [past24hRes, runningRes, lastRes] = await Promise.allSettled([
      director.call('llist jobs days=1'),
      director.call('list jobs jobstatus=R'),
      director.call('llist jobs last'),
    ])
    if (past24hRes.status === 'fulfilled' && past24hRes.value?.jobs)
      rawPast24hJobs.value = directorCollection(past24hRes.value.jobs)
    if (runningRes.status === 'fulfilled' && runningRes.value?.jobs) {
      rawRunningJobs.value = directorCollection(runningRes.value.jobs)
    }
    if (lastRes.status === 'fulfilled' && lastRes.value?.jobs) {
      rawLastJobs.value = directorCollection(lastRes.value.jobs)
    }
  } catch { /* keep empty */ } finally {
    loadingJobs.value = false
  }
}

async function fetchTotals() {
  if (!director.isConnected) return
  try {
    const result = await director.call('list jobtotals')
    const t = result?.jobtotals
    if (t) {
      jobTotals.value = {
        jobs:  Number(t.jobs  ?? 0),
        files: Number(t.files ?? 0),
        bytes: Number(t.bytes ?? 0),
      }
    }
  } catch { /* ignore */ }
}

async function fetchSidebar() {
  if (!director.isConnected) return
  try {
    const [cr, sr] = await Promise.allSettled([
      director.call('list clients'),
      director.call('list storages'),
    ])
    if (cr.status === 'fulfilled') clientCount.value = directorCollection(cr.value?.clients).length
    if (sr.status === 'fulfilled') storageCount.value = directorCollection(sr.value?.storages).length
  } catch { /* ignore */ }
}

let refreshPromise = null
let refreshQueued = false

async function runRefreshBatch() {
  await Promise.allSettled([fetchJobs(), fetchTotals(), fetchSidebar()])
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

async function doCancel(job) {
  try {
    await director.call(`cancel jobid=${jobId(job)} yes`)
    $q.notify({ type: 'positive', message: t('Job {id} cancelled.', { id: jobId(job) }) })
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: t('Cancel failed: {message}', { message: e.message }) })
  }
}

const now = ref(Date.now())

onMounted(refresh)
watch(() => director.isConnected, (connected) => { if (connected) refresh() })

// ── auto-refresh ──────────────────────────────────────────────────────────────
const countdown = ref(settings.refreshInterval)
let _countdownTimer = null
let _refreshTimer   = null

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
  _refreshTimer   = null
}

function manualRefresh() {
  refresh()
  countdown.value = settings.refreshInterval
}

onMounted(startAutoRefresh)
onUnmounted(stopAutoRefresh)
watch(() => director.isConnected, (connected) => { if (connected) startAutoRefresh() })

// ── computed views ────────────────────────────────────────────────────────────
const past24hJobs = computed(() => rawPast24hJobs.value.map(normaliseJob))
const lastJobs    = computed(() => rawLastJobs.value.map(normaliseJob))

const recentCols = computed(() => [
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
])

const recentJobs  = computed(() => lastJobs.value)
const runningJobs = computed(() => rawRunningJobs.value.map(normaliseJob))

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
    const s = (code) => past24hJobs.value.filter(j => j.status === code).length
  return [
    { label: t('Running'), status: 'R', color: 'info', count: s('R') },
    { label: t('Waiting'), status: 'C', color: 'grey', count: s('C') },
    { label: t('Successful'), status: 'T', color: 'positive', count: s('T') },
    { label: t('Warning'), status: 'W', color: 'warning', count: s('W') },
    { label: t('Failed'), status: 'f', color: 'negative', count: s('f') },
  ]
})

const totals = computed(() => {
  if (jobTotals.value) return jobTotals.value
  return {
    jobs:     past24hJobs.value.length,
    files:    past24hJobs.value.reduce((a, j) => a + j.files, 0),
    bytes:    past24hJobs.value.reduce((a, j) => a + j.bytes, 0),
    clients:  clientCount.value,
    storages: storageCount.value,
  }
})

const totalStats = computed(() => [
  { label: t('Total Jobs'), value: totals.value.jobs },
  { label: t('Total Files'), value: formatNumber(totals.value.files ?? 0, settings.locale) },
  { label: t('Total Bytes'), value: fmtBytes(totals.value.bytes ?? 0) },
  { label: t('Clients'), value: clientCount.value },
  { label: t('Storages'), value: storageCount.value },
])

// helper: keep status/bytes getters for template compatibility
function jobStatus(row) { return row.status ?? '?' }
function jobBytes(row)  { return fmtBytes(row.bytes ?? 0) }
function jobId(row) {
  return row.id ?? row.jobid
}
</script>
