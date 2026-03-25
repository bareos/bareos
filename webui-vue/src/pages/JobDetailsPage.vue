<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" label="Loading job…" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>

    <div v-else-if="!loading && job">
      <!-- Header row -->
      <div class="row items-center q-mb-md">
        <q-btn flat icon="arrow_back" label="Back to Jobs" :to="{ name: 'jobs' }" no-caps class="q-mr-md" />
        <div class="text-h6">Job #{{ job.id }} — {{ job.name }}</div>
        <q-space />
        <q-spinner v-if="isRunning" color="primary" size="18px" class="q-mr-sm" title="Auto-refreshing…" />
        <JobStatusBadge :status="job.status" />
      </div>

      <div class="row q-col-gutter-md">

        <!-- Summary -->
        <div class="col-12 col-md-6">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">Job Summary</q-card-section>
            <q-card-section>
              <q-list dense separator>
                <q-item v-for="row in summaryRows" :key="row.label">
                  <q-item-section class="text-weight-medium" style="max-width:140px">{{ row.label }}</q-item-section>
                  <q-item-section>{{ row.value }}</q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>
        </div>

        <!-- Volumes + Actions -->
        <div class="col-12 col-md-6 column q-gutter-md">

          <!-- Actions -->
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">Actions</q-card-section>
            <q-card-section class="q-gutter-sm">
              <q-btn icon="restart_alt" label="Rerun Job" color="primary" no-caps
                     :loading="rerunLoading" @click="confirmRerun" />
              <q-btn v-if="isRunning" icon="cancel" label="Cancel Job" color="negative" no-caps
                     :loading="cancelLoading" @click="confirmCancel" />
            </q-card-section>
          </q-card>

          <!-- Volumes used -->
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">Volumes Used</q-card-section>
            <q-card-section class="q-pa-none">
              <q-list dense separator>
                <q-item v-for="vol in volumes" :key="vol.volumename">
                  <q-item-section avatar>
                    <q-icon name="album" color="primary" />
                  </q-item-section>
                  <q-item-section>{{ vol.volumename }}</q-item-section>
                  <q-item-section side class="text-caption text-grey">
                    {{ vol.firstindex }} – {{ vol.lastindex }}
                  </q-item-section>
                </q-item>
                <q-item v-if="!volumes.length">
                  <q-item-section class="text-grey text-caption q-py-sm">No volumes recorded.</q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>

        </div>

        <!-- Job Log -->
        <div class="col-12">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header row items-center">
              <span>Job Log</span>
              <q-space />
              <q-btn flat round dense icon="content_copy" size="sm" color="white"
                     title="Copy log" @click="copyLog" />
            </q-card-section>
            <q-card-section class="q-pa-none">
              <pre v-if="jobLog" class="job-log q-pa-md q-ma-none">{{ jobLog }}</pre>
              <div v-else class="text-grey text-caption q-pa-md">No log entries found.</div>
            </q-card-section>
          </q-card>
        </div>

      </div>
    </div>

    <div v-else-if="!loading" class="text-center q-pa-xl text-grey">Job not found.</div>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { jobTypeMap, jobLevelMap, formatBytes } from '../mock/index.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route    = useRoute()
const router   = useRouter()
const $q       = useQuasar()
const director = useDirectorStore()

const jobid = route.params.id

// ── state ─────────────────────────────────────────────────────────────────────
const loading       = ref(true)
const jobData       = ref(null)
const logLines      = ref('')
const volumes       = ref([])
const error         = ref(null)
const rerunLoading  = ref(false)
const cancelLoading = ref(false)

// ── data loading ──────────────────────────────────────────────────────────────
async function loadJob() {
  const [jobRes, logRes, mediaRes] = await Promise.allSettled([
    director.call(`llist jobid=${jobid}`),
    director.call(`list joblog jobid=${jobid}`),
    director.call(`list jobmedia jobid=${jobid}`),
  ])

  if (jobRes.status === 'fulfilled') {
    const raw   = jobRes.value
    const entry = Array.isArray(raw?.jobs) ? raw.jobs[0] : (raw?.jobs ?? raw)
    jobData.value = entry ? normaliseJob(entry) : null
  } else {
    error.value = jobRes.reason?.message ?? 'Failed to load job'
  }

  if (logRes.status === 'fulfilled') {
    const raw = logRes.value
    if (Array.isArray(raw?.joblog)) {
      logLines.value = raw.joblog
        .map(l => `${(l.time ?? '').trim()} ${(l.logtext ?? '').trimEnd()}`.trim())
        .join('\n')
    } else if (typeof raw === 'string') {
      logLines.value = raw
    }
  }

  if (mediaRes.status === 'fulfilled') {
    volumes.value = mediaRes.value?.jobmedia ?? []
  }
}

onMounted(async () => {
  try {
    await loadJob()
  } finally {
    loading.value = false
  }
})

// ── computed ──────────────────────────────────────────────────────────────────
const job = computed(() => jobData.value)

const summaryRows = computed(() => {
  if (!job.value) return []
  const j = job.value
  return [
    { label: 'Job ID',     value: j.id },
    { label: 'Job Name',   value: j.name },
    { label: 'Client',     value: j.client },
    { label: 'Type',       value: jobTypeMap[j.type]  || j.type  || '—' },
    { label: 'Level',      value: jobLevelMap[j.level] || j.level || '—' },
    { label: 'Start Time', value: j.starttime || '—' },
    { label: 'End Time',   value: j.endtime   || '—' },
    { label: 'Duration',   value: j.duration  || '—' },
    { label: 'Files',      value: Number(j.files).toLocaleString() },
    { label: 'Bytes',      value: formatBytes(j.bytes) },
    { label: 'Errors',     value: j.errors },
  ]
})

const jobLog = computed(() => logLines.value)

// ── auto-refresh for running jobs ─────────────────────────────────────────────
const isRunning = computed(() => job.value?.status === 'R' || job.value?.status === 'l')

let _pollTimer = null

function startPolling() {
  stopPolling()
  _pollTimer = setInterval(async () => {
    if (!isRunning.value) { stopPolling(); return }
    await loadJob()
  }, 5000)
}

function stopPolling() {
  clearInterval(_pollTimer)
  _pollTimer = null
}

watch(isRunning, (running) => {
  if (running) startPolling()
  else stopPolling()
}, { immediate: true })

onUnmounted(stopPolling)

// ── actions ───────────────────────────────────────────────────────────────────
function confirmRerun() {
  $q.dialog({
    title: 'Rerun Job',
    message: `Rerun job <b>${job.value.name}</b> (ID&nbsp;${job.value.id})?`,
    html: true,
    ok:     { label: 'Rerun', color: 'primary', flat: true },
    cancel: { label: 'Cancel', flat: true },
  }).onOk(doRerun)
}

async function doRerun() {
  rerunLoading.value = true
  try {
    const res   = await director.call(`rerun jobid=${jobid} yes`)
    const newId = res?.run?.jobid ?? res?.jobid ?? null
    $q.notify({ type: 'positive', message: `Job restarted${newId ? ` as ID ${newId}` : ''}.` })
    if (newId) router.push({ name: 'job-details', params: { id: newId } })
  } catch (e) {
    $q.notify({ type: 'negative', message: `Rerun failed: ${e.message}` })
  } finally {
    rerunLoading.value = false
  }
}

function confirmCancel() {
  $q.dialog({
    title: 'Cancel Job',
    message: `Cancel job <b>${job.value.name}</b> (ID&nbsp;${job.value.id})?`,
    html: true,
    ok:     { label: 'Cancel Job', color: 'negative', flat: true },
    cancel: { label: 'Keep Running', flat: true },
  }).onOk(doCancel)
}

async function doCancel() {
  cancelLoading.value = true
  try {
    await director.call(`cancel jobid=${jobid} yes`)
    $q.notify({ type: 'positive', message: `Job ${jobid} cancelled.` })
    await loadJob()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Cancel failed: ${e.message}` })
  } finally {
    cancelLoading.value = false
  }
}

function copyLog() {
  navigator.clipboard?.writeText(jobLog.value)
  $q.notify({ message: 'Log copied to clipboard', color: 'positive', icon: 'check', timeout: 1500 })
}
</script>

<style scoped>
.job-log {
  font-family: monospace;
  font-size: 0.8rem;
  white-space: pre-wrap;
  word-break: break-all;
  max-height: 500px;
  overflow-y: auto;
  background: #1e1e1e;
  color: #d4d4d4;
  border-radius: 4px;
}
</style>
