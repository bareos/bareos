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
            <span>Most Recent Job Status</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="manualRefresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="recentJobs"
              :columns="recentCols"
              row-key="id"
              dense flat
              :pagination="{ rowsPerPage: 8 }"
              hide-bottom
            >
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="jobStatus(props.row)" />
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
                <q-td :props="props" class="text-right">{{ jobBytes(props.row) }}</q-td>
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
          <q-card-section>
            <q-list dense>
              <q-item>
                <q-item-section><q-item-label caption>Total Jobs</q-item-label><q-item-label class="text-h6">{{ totals.jobs }}</q-item-label></q-item-section>
              </q-item>
              <q-item>
                <q-item-section><q-item-label caption>Total Files</q-item-label><q-item-label class="text-h6">{{ totals.files.toLocaleString() }}</q-item-label></q-item-section>
              </q-item>
              <q-item>
                <q-item-section><q-item-label caption>Total Bytes</q-item-label><q-item-label class="text-h6">{{ fmtBytes(totals.bytes) }}</q-item-label></q-item-section>
              </q-item>
              <q-item>
                <q-item-section><q-item-label caption>Clients</q-item-label><q-item-label class="text-h6">{{ clientCount }}</q-item-label></q-item-section>
              </q-item>
              <q-item>
                <q-item-section><q-item-label caption>Storages</q-item-label><q-item-label class="text-h6">{{ storageCount }}</q-item-label></q-item-section>
              </q-item>
            </q-list>
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
                  <q-linear-progress :value="0.45" color="positive" class="q-mt-xs" style="height:6px; border-radius:3px" />
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
import { formatBytes } from '../mock/index.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import StatNumber from '../components/StatNumber.vue'

const director = useDirectorStore()
const fmtBytes = formatBytes

// ── data ─────────────────────────────────────────────────────────────────────
const rawJobs       = ref([])
const clientCount   = ref(0)
const storageCount  = ref(0)
const jobTotals     = ref(null)
const loadingJobs   = ref(false)

async function fetchJobs() {
  if (!director.isConnected) return
  loadingJobs.value = true
  try {
    const result = await director.call('list jobs')
    if (result?.jobs) rawJobs.value = result.jobs
  } catch { /* keep empty */ } finally {
    loadingJobs.value = false
  }
}

async function fetchTotals() {
  if (!director.isConnected) return
  try {
    const result = await director.call('list jobs totals')
    if (result) jobTotals.value = result
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
const jobs = computed(() => rawJobs.value.map(normaliseJob))

const recentCols = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right', sortable: true },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',  sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',  sortable: true },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center' },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',  sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right'  },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center' },
]

const recentJobs  = computed(() => jobs.value.slice(0, 8))
const runningJobs = computed(() => jobs.value.filter(j => j.status === 'R'))

const summaryStats = computed(() => {
  const s = (code) => jobs.value.filter(j => j.status === code).length
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
    jobs:     jobs.value.length,
    files:    jobs.value.reduce((a, j) => a + j.files, 0),
    bytes:    jobs.value.reduce((a, j) => a + j.bytes, 0),
    clients:  clientCount.value,
    storages: storageCount.value,
  }
})

// helper: keep status/bytes getters for template compatibility
function jobStatus(row) { return row.status ?? '?' }
function jobBytes(row)  { return fmtBytes(row.bytes ?? 0) }
function jobId(row) {
  return row.id ?? row.jobid
}
</script>
