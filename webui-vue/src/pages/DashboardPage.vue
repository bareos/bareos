<template>
  <q-page class="q-pa-md">
    <div class="row q-col-gutter-md">
      <!-- Left column: 8/12 -->
      <div class="col-12 col-md-8">
        <!-- Jobs Past 24h stat row -->
        <q-card flat bordered class="q-mb-md bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Jobs started during the past 24 hours</span>
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
            <span>Most recent job status per job name</span>
            <q-space />
            <q-btn flat round dense color="white" size="sm" class="q-mr-xs"
                   :icon="relativeStart ? 'calendar_today' : 'schedule'"
                   :title="relativeStart ? 'Show absolute start time' : 'Show relative start time'"
                   @click="relativeStart = !relativeStart" />
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
              <template #body-cell-starttime="props">
                <q-td :props="props">
                  <span :title="relativeStart ? props.value : timeAgo(props.value)">
                    {{ relativeStart ? timeAgo(props.value) : props.value }}
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
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right" style="min-width:90px">
                  <div>{{ jobBytes(props.row) }}</div>
                  <q-linear-progress
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
            </q-table>
          </q-card-section>
        </q-card>
      </div>

      <!-- Right column: 4/12 -->
      <div class="col-12 col-md-4">
        <!-- Job Totals -->
        <q-card flat bordered class="q-mb-md bareos-panel">
          <q-card-section class="panel-header">Job Totals</q-card-section>
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
            <span>Running Jobs</span>
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
                    {{ (job.files ?? 0).toLocaleString() }} files &middot; {{ jobBytes(job) }}
                  </q-item-label>
                  <q-linear-progress indeterminate color="positive" class="q-mt-xs" style="height:6px; border-radius:3px" />
                </q-item-section>
                <q-item-section side>
                  <q-btn flat round dense icon="cancel" color="negative" size="sm" title="Cancel job" />
                </q-item-section>
              </q-item>
              <q-item v-if="!runningJobs.length">
                <q-item-section class="text-grey text-caption text-center q-py-md">No running jobs</q-item-section>
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
import { formatBytes, timeAgo } from '../mock/index.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import StatNumber from '../components/StatNumber.vue'

const director = useDirectorStore()
const fmtBytes      = formatBytes
const relativeStart = ref(false)

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
      rawPast24hJobs.value = past24hRes.value.jobs
    if (runningRes.status === 'fulfilled' && runningRes.value?.jobs) {
      const raw = runningRes.value.jobs
      rawRunningJobs.value = Array.isArray(raw) ? raw : Object.values(raw)
    }
    if (lastRes.status === 'fulfilled' && lastRes.value?.jobs) {
      const raw = lastRes.value.jobs
      rawLastJobs.value = Array.isArray(raw) ? raw : Object.values(raw)
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
    if (cr.status === 'fulfilled') clientCount.value  = cr.value?.clients?.length  ?? 0
    if (sr.status === 'fulfilled') storageCount.value = sr.value?.storages?.length ?? 0
  } catch { /* ignore */ }
}

function refresh() {
  fetchJobs(); fetchTotals(); fetchSidebar()
}

const REFRESH_INTERVAL = 5   // seconds

onMounted(refresh)
watch(() => director.isConnected, (connected) => { if (connected) refresh() })

// ── auto-refresh ──────────────────────────────────────────────────────────────
const countdown = ref(REFRESH_INTERVAL)
let _countdownTimer = null
let _refreshTimer   = null

function startAutoRefresh() {
  stopAutoRefresh()
  countdown.value = REFRESH_INTERVAL
  _countdownTimer = setInterval(() => {
    countdown.value -= 1
    if (countdown.value <= 0) {
      refresh()
      countdown.value = REFRESH_INTERVAL
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
  countdown.value = REFRESH_INTERVAL
}

onMounted(startAutoRefresh)
onUnmounted(stopAutoRefresh)
watch(() => director.isConnected, (connected) => { if (connected) startAutoRefresh() })

// ── computed views ────────────────────────────────────────────────────────────
const past24hJobs = computed(() => rawPast24hJobs.value.map(normaliseJob))
const lastJobs    = computed(() => rawLastJobs.value.map(normaliseJob))

const recentCols = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right', sortable: true },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',  sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',  sortable: true },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center' },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',  sortable: true },
  { name: 'duration',  label: 'Duration', field: 'duration',  align: 'right', sortable: true,
    sort: (a, b) => parseDurationSecs(a) - parseDurationSecs(b) },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right', sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center' },
]

const recentJobs  = computed(() => lastJobs.value)
const runningJobs = computed(() => rawRunningJobs.value.map(normaliseJob))

function parseDurationSecs(str) {
  if (!str) return 0
  const parts = String(str).split(':').map(Number)
  if (parts.length === 3) return parts[0] * 3600 + parts[1] * 60 + parts[2]
  if (parts.length === 2) return parts[0] * 60 + parts[1]
  return Number(str) || 0
}

const maxBytesLog = computed(() => Math.log(Math.max(1, ...recentJobs.value.map(j => j.bytes)) + 1))
const maxDurationLog = computed(() => {
  const max = Math.max(1, ...recentJobs.value.map(j => parseDurationSecs(j.duration)))
  return Math.log(max + 1)
})
function bytesGauge(val) { return Math.log(val + 1) / maxBytesLog.value }
function durationGauge(str) {
  return Math.log(parseDurationSecs(str) + 1) / maxDurationLog.value
}

const summaryStats = computed(() => {
  const s = (code) => past24hJobs.value.filter(j => j.status === code).length
  return [
    { label: 'Running',    status: 'R', color: 'info',     count: s('R') },
    { label: 'Waiting',    status: 'C', color: 'grey',     count: s('C') },
    { label: 'Successful', status: 'T', color: 'positive', count: s('T') },
    { label: 'Warning',    status: 'W', color: 'warning',  count: s('W') },
    { label: 'Failed',     status: 'f', color: 'negative', count: s('f') },
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
  { label: 'Total Jobs',  value: totals.value.jobs },
  { label: 'Total Files', value: (totals.value.files ?? 0).toLocaleString() },
  { label: 'Total Bytes', value: fmtBytes(totals.value.bytes ?? 0) },
  { label: 'Clients',     value: clientCount.value },
  { label: 'Storages',    value: storageCount.value },
])

// helper: keep status/bytes getters for template compatibility
function jobStatus(row) { return row.status ?? '?' }
function jobBytes(row)  { return fmtBytes(row.bytes ?? 0) }
function jobId(row) {
  return row.id ?? row.jobid
}
</script>
