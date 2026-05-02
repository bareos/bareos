<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" :label="t('Loading job…')" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>

    <div v-else-if="!loading && job">
      <!-- Header row -->
      <div class="row items-center q-mb-md">
        <q-btn
          flat
          icon="arrow_back"
          :label="backLabel"
          :to="backLocation"
          no-caps
          class="q-mr-md"
        />
        <div class="text-h6">Job #{{ job.id }} — {{ job.name }}</div>
        <q-space />
        <q-spinner v-if="isRunning" color="primary" size="18px" class="q-mr-sm" :title="t('Auto-refreshing…')" />
        <JobStatusBadge :status="job.status" />
      </div>

      <div class="row q-col-gutter-md">

        <!-- Summary -->
        <div class="col-12 col-md-6 column q-gutter-md">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Job Summary') }}</q-card-section>
            <q-card-section>
              <q-list dense separator>
                <q-item v-for="row in summaryRows" :key="row.label">
                  <q-item-section class="text-weight-medium" style="max-width:140px">{{ row.label }}</q-item-section>
                  <q-item-section>
                    <span v-if="row.level">
                      <JobLevelBadge :level="row.level" />
                      <span class="q-ml-xs text-grey-7">{{ row.value }}</span>
                    </span>
                    <span v-else-if="row.type">
                      <JobTypeBadge :type="row.type" />
                      <span class="q-ml-xs text-grey-7">{{ row.value }}</span>
                    </span>
                    <span v-else>{{ row.value }}</span>
                  </q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>

          <q-card v-if="runtimeRows.length" flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Live Storage Runtime') }}</q-card-section>
            <q-card-section>
              <q-list dense separator>
                <q-item v-for="row in runtimeRows" :key="row.label">
                  <q-item-section class="text-weight-medium" style="max-width:160px">{{ row.label }}</q-item-section>
                  <q-item-section>{{ row.value }}</q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>

          <q-card v-if="jobHistoryEntries.length" flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Daemon Status History') }}</q-card-section>
            <q-card-section class="q-pa-none">
              <q-list dense separator>
                <q-item v-for="entry in jobHistoryEntries" :key="`${entry.timestamp}-${entry.source}-${entry.newStatus}`" class="items-start">
                  <q-item-section>
                    <div class="text-weight-medium">
                      {{ entry.newStatusLong || entry.newStatus || '—' }}
                      <span class="text-grey-7">· {{ entry.sourceLabel }}</span>
                    </div>
                    <div class="text-caption text-grey-7">
                      {{ entry.timestampLabel }}
                      <span v-if="entry.previousStatusLong">
                        · {{ t('from') }} {{ entry.previousStatusLong }}
                      </span>
                    </div>
                    <div v-if="entry.detailLine" class="text-caption q-mt-xs">
                      {{ entry.detailLine }}
                    </div>
                  </q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>
        </div>

        <!-- Volumes + Actions -->
        <div class="col-12 col-md-6 column q-gutter-md">

          <!-- Actions -->
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Actions') }}</q-card-section>
            <q-card-section class="q-gutter-sm">
              <q-btn icon="restart_alt" :label="t('Rerun Job')" color="primary" no-caps
                     :loading="rerunLoading" @click="confirmRerun" />
              <q-btn v-if="isRunning" icon="cancel" :label="t('Cancel Job')" color="negative" no-caps
                     :loading="cancelLoading" @click="confirmCancel" />
            </q-card-section>
          </q-card>

          <!-- Volumes used -->
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Volumes Used') }}</q-card-section>
            <q-card-section class="q-pa-none">
              <q-table
                :rows="volumes"
                :columns="volumeCols"
                row-key="volumename"
                dense flat
                hide-bottom
                :pagination="{ rowsPerPage: 0 }"
                :no-data-label="t('No volumes recorded.')"
              >
                <template #body-cell-volumename="props">
                  <q-td :props="props">
                    <q-icon name="album" color="primary" size="xs" class="q-mr-xs" />
                    <VolumeNameLink
                      :name="props.value"
                      :volume="props.row"
                      :query="{
                        ...currentJobDetailsQuery,
                        ...buildVolumeDetailsQuery({
                          director: currentJobDirector,
                          jobId: currentJobId,
                        }),
                      }"
                    />
                  </q-td>
                </template>
                <template #header-cell-firstindex="props">
                  <q-th :props="props">
                    {{ props.col.label }}
                    <q-icon name="help_outline" size="xs" class="q-ml-xs text-grey-5">
                      <q-tooltip>{{ t('Catalog index of the first file written to this volume for this job. Used internally by Bareos to locate and restore files.') }}</q-tooltip>
                    </q-icon>
                  </q-th>
                </template>
                <template #header-cell-lastindex="props">
                  <q-th :props="props">
                    {{ props.col.label }}
                    <q-icon name="help_outline" size="xs" class="q-ml-xs text-grey-5">
                      <q-tooltip>{{ t('Catalog index of the last file written to this volume for this job. The range first–last covers all files stored on this volume for this job.') }}</q-tooltip>
                    </q-icon>
                  </q-th>
                </template>
              </q-table>
            </q-card-section>
          </q-card>

        </div>

        <!-- Job Log -->
        <div class="col-12">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header row items-center">
                <span>{{ t('Job Log') }}</span>
              <q-space />
              <q-btn flat round dense icon="content_copy" size="sm" color="white"
                     :title="t('Copy log')" @click="copyLog" />
            </q-card-section>
            <q-card-section class="q-pa-none">
              <div v-if="highlightedLines.length" class="job-log q-pa-md" ref="logContainer">
                <div v-for="(line, i) in highlightedLines" :key="i"
                     :class="['log-line', `log-line--${line.type}`]">{{ line.text }}</div>
              </div>
               <div v-else class="text-grey text-caption q-pa-md">{{ t('No log entries found.') }}</div>
            </q-card-section>
          </q-card>
        </div>

      </div>
    </div>

    <div v-else-if="!loading" class="text-center q-pa-xl text-grey">{{ t('Job not found.') }}</div>
  </q-page>
</template>

<script setup>
import { ref, computed, onUnmounted, watch, nextTick } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { jobLevelMap, formatBytes, formatSpeed } from '../mock/index.js'
import {
  directorCollection,
  mergeStorageRuntime,
  normaliseJobHistory,
  normaliseJob,
  quoteDirectorString,
} from '../composables/useDirectorFetch.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildClientDetailsQuery } from '../utils/clients.js'
import { buildDirectorPageQuery } from '../utils/director.js'
import { formatNumber } from '../utils/locales.js'
import {
  buildJobDetailsQuery,
  resolveJobDetailsClientOrigin,
  resolveJobDetailsQuery,
  resolveJobDetailsDashboardOrigin,
  resolveJobDetailsDirectorOrigin,
  resolveJobDetailsRestoreOrigin,
  resolveJobDetailsVolumeOrigin,
  resolveJobsListQuery,
} from '../utils/jobs.js'
import { buildVolumeDetailsQuery } from '../utils/volumes.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'
import JobTypeBadge from '../components/JobTypeBadge.vue'
import VolumeNameLink from '../components/VolumeNameLink.vue'

const route    = useRoute()
const router   = useRouter()
const $q       = useQuasar()
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const { t } = useI18n()

const currentJobId = computed(() => route.params.id)
const requestedDirector = computed(() => (
  typeof route.query.director === 'string' ? route.query.director : ''
))
const currentJobDirector = computed(() => (
  requestedDirector.value || auth.user?.director || settings.directorName || ''
))
const backToJobsQuery = computed(() => resolveJobsListQuery(route.query))
const dashboardOrigin = computed(() => resolveJobDetailsDashboardOrigin(route.query))
const clientOrigin = computed(() => resolveJobDetailsClientOrigin(route.query))
const directorOrigin = computed(() => resolveJobDetailsDirectorOrigin(route.query))
const restoreOrigin = computed(() => resolveJobDetailsRestoreOrigin(route.query))
const volumeOrigin = computed(() => resolveJobDetailsVolumeOrigin(route.query))
const currentJobDetailsQuery = computed(() => resolveJobDetailsQuery(route.query))
const backLabel = computed(() => (
  clientOrigin.value
    ? t('Back to Client')
    : (dashboardOrigin.value
      ? t('Back to Dashboard')
      : (
        directorOrigin.value
          ? t('Back to Director')
          : (
            restoreOrigin.value
              ? t('Back to Restore')
              : (volumeOrigin.value ? t('Back to Volume') : t('Back to Jobs'))
          )
      ))
))
const backLocation = computed(() => {
  if (clientOrigin.value) {
    return {
      name: 'client-details',
      params: { name: clientOrigin.value.name },
      query: buildClientDetailsQuery({
        director: clientOrigin.value.director,
        clientsTab: clientOrigin.value.clientsTab,
        clientsScopeDirector: clientOrigin.value.scopeDirector,
        jobsAction: clientOrigin.value.jobsAction,
        jobsStatus: clientOrigin.value.jobsStatus,
        jobsSearch: clientOrigin.value.jobsSearch,
        jobsScopeDirector: clientOrigin.value.jobsScopeDirector,
        dashboardOrigin: clientOrigin.value.dashboardOrigin,
      }),
    }
  }

  if (dashboardOrigin.value) {
    return { name: 'dashboard' }
  }

  if (directorOrigin.value) {
    return {
      name: 'director',
      query: buildDirectorPageQuery({}, {
        tab: directorOrigin.value.tab,
        targetDirector: directorOrigin.value.targetDirector,
      }),
    }
  }

  if (restoreOrigin.value) {
    return {
      name: 'restore',
      query: {
        client: restoreOrigin.value.client,
        director: restoreOrigin.value.director,
        jobid: restoreOrigin.value.jobid,
      },
    }
  }

  if (volumeOrigin.value) {
    return {
      name: 'volume-details',
      params: { name: volumeOrigin.value.name },
      query: volumeOrigin.value.director ? { director: volumeOrigin.value.director } : {},
    }
  }

  return {
    name: 'jobs',
    query: backToJobsQuery.value,
  }
})

// ── state ─────────────────────────────────────────────────────────────────────
const loading       = ref(true)
const jobData       = ref(null)
const logLines      = ref('')
const logContainer  = ref(null)
const volumes       = ref([])
const volumeDetailsByName = ref({})
const jobHistory = ref({ available: false, events: [] })
const error         = ref(null)
const rerunLoading  = ref(false)
const cancelLoading = ref(false)

// Scroll the log panel to the bottom whenever new log content arrives.
watch(logLines, () => {
  nextTick(() => {
    if (logContainer.value) logContainer.value.scrollTop = logContainer.value.scrollHeight
  })
})

// ── data loading ──────────────────────────────────────────────────────────────
async function ensureJobDirector() {
  if (!requestedDirector.value) {
    return
  }

  if (auth.user?.director === requestedDirector.value && director.isConnected) {
    return
  }

  await switchActiveDirector(requestedDirector.value)
}

async function loadJob() {
  await ensureJobDirector()
  const [jobRes, logRes, mediaRes, statusRes, historyRes] = await Promise.allSettled([
    director.call(`llist jobid=${currentJobId.value}`),
    director.call(`list joblog jobid=${currentJobId.value}`),
    director.call(`list jobmedia jobid=${currentJobId.value}`),
    director.call('status director'),
    director.call(`.jobhistory jobid=${currentJobId.value}`),
  ])

  if (jobRes.status === 'fulfilled') {
    const raw   = jobRes.value
    const entry = Array.isArray(raw?.jobs) ? raw.jobs[0] : (raw?.jobs ?? raw)
    const normalizedJob = entry
      ? {
        ...normaliseJob(entry),
        director: currentJobDirector.value || null,
      }
      : null

    if (normalizedJob && statusRes.status === 'fulfilled') {
      const runtimeEntry = directorCollection(statusRes.value?.running)
        .find(runningJob => Number(runningJob?.jobid) === normalizedJob.id)
      jobData.value = mergeStorageRuntime(normalizedJob, runtimeEntry)
    } else {
      jobData.value = normalizedJob
    }
  } else {
    error.value = jobRes.reason?.message ?? t('Failed to load job')
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
    const mediaRows = mediaRes.value?.jobmedia ?? []
    const volumeNames = [...new Set(
      mediaRows.map(row => row.volumename).filter(Boolean)
    )]
    const volumeResults = await Promise.allSettled(
      volumeNames.map(name => director.call(`llist volume=${quoteDirectorString(name)}`))
    )

    volumeDetailsByName.value = volumeNames.reduce((acc, name, index) => {
      const result = volumeResults[index]
      if (result.status !== 'fulfilled') {
        return acc
      }

      const raw = result.value?.volumes ?? result.value?.volume ?? null
      acc[name] = Array.isArray(raw) ? (raw[0] ?? null) : raw
      return acc
    }, {})

    volumes.value = mediaRows.map(row => ({
      ...volumeDetailsByName.value[row.volumename],
      ...row,
    }))
  }

  if (historyRes.status === 'fulfilled') {
    jobHistory.value = normaliseJobHistory(historyRes.value)
  } else {
    jobHistory.value = { available: false, events: [] }
  }
}

watch(() => `${currentJobId.value}\u0000${requestedDirector.value}`, async () => {
  loading.value = true
  error.value = null
  jobData.value = null
  logLines.value = ''
  volumes.value = []
  volumeDetailsByName.value = {}
  jobHistory.value = { available: false, events: [] }
  try {
    await loadJob()
  } catch (loadError) {
    error.value = loadError.message ?? t('Failed to load job')
  } finally {
    loading.value = false
  }
}, { immediate: true })

// ── computed ──────────────────────────────────────────────────────────────────
const job = computed(() => jobData.value)

const volumeCols = computed(() => [
  { name: 'volumename', label: 'Volume',
    field: 'volumename', align: 'left', sortable: true,
    headerClasses: 'text-weight-bold' },
  { name: 'firstindex', label: 'First File Index',
    field: 'firstindex', align: 'right', sortable: true,
    headerTitle: 'Catalog index of the first file written to this volume for this job' },
  { name: 'lastindex',  label: 'Last File Index',
    field: 'lastindex',  align: 'right', sortable: true,
    headerTitle: 'Catalog index of the last file written to this volume for this job' },
].map((col) => ({
  ...col,
  label: t(col.label),
  headerTitle: col.headerTitle ? t(col.headerTitle) : col.headerTitle,
})))

const summaryRows = computed(() => {
  if (!job.value) return []
  const j = job.value
  return [
    { label: t('Job ID'),     value: j.id },
    { label: t('Job Name'),   value: j.name },
    { label: t('Client'),     value: j.client },
    { label: t('Type'),       value: j.type  || '—', type: j.type  || null },
    { label: t('Level'),      value: jobLevelMap[j.level] || j.level || '—', level: j.level || null },
    { label: t('Start Time'), value: j.starttime || '—' },
    { label: t('End Time'),   value: j.endtime   || '—' },
    { label: t('Duration'),   value: j.duration  || '—' },
    { label: t('Files'),      value: formatNumber(j.files, settings.locale) },
    { label: t('Bytes'),      value: formatBytes(j.bytes) },
    { label: t('Speed'),      value: formatSpeed(j.bytes, j.duration) },
    { label: t('Errors'),     value: j.errors },
  ]
})

function formatRuntimeRate(value) {
  return value == null ? '—' : `${formatBytes(value)}/s`
}

const runtimeRows = computed(() => {
  const runtime = job.value?.runtime
  if (!runtime) return []

  const rows = [
    { label: t('Current File'), value: runtime.currentFile || '—' },
    { label: t('Files'), value: runtime.files == null ? '—' : formatNumber(runtime.files, settings.locale) },
    { label: t('Bytes'), value: runtime.bytes == null ? '—' : formatBytes(runtime.bytes) },
    { label: t('Average Rate'), value: formatRuntimeRate(runtime.averageBytesPerSecond) },
    { label: t('Last Rate'), value: formatRuntimeRate(runtime.lastBytesPerSecond) },
  ]

  const writePath = [
    runtime.pool ? `${t('Pool')}: ${runtime.pool}` : null,
    runtime.writeVolume ? `${t('Volume')}: ${runtime.writeVolume}` : null,
    runtime.writeDevice ? `${t('Device')}: ${runtime.writeDevice}` : null,
  ].filter(Boolean).join(' · ')
  if (writePath) {
    rows.push({ label: t('Writing'), value: writePath })
  }

  const readPath = [
    runtime.readVolume ? `${t('Volume')}: ${runtime.readVolume}` : null,
    runtime.readDevice ? `${t('Device')}: ${runtime.readDevice}` : null,
  ].filter(Boolean).join(' · ')
  if (readPath) {
    rows.push({ label: t('Reading'), value: readPath })
  }

  rows.push(
    { label: t('Spooling'), value: runtime.spooling ? t('Yes') : t('No') },
    { label: t('Despooling'), value: runtime.despooling ? t('Yes') : t('No') },
    { label: t('Waiting for despool'), value: runtime.despoolWait ? t('Yes') : t('No') },
  )

  return rows
})

const jobLog = computed(() => logLines.value)

function formatTimestamp(value) {
  if (!value) return '—'
  const date = new Date(value * 1000)
  return Number.isNaN(date.getTime()) ? '—' : date.toLocaleString(settings.locale)
}

const sourceLabels = {
  director: 'Director',
  'storage-daemon': 'Storage Daemon',
  'file-daemon': 'File Daemon',
}

const jobHistoryEntries = computed(() => (
  jobHistory.value.events.map((event) => {
    const detailParts = [
      event.currentFile ? `${t('Current File')}: ${event.currentFile}` : null,
      event.jobFiles != null ? `${t('Files')}: ${formatNumber(event.jobFiles, settings.locale)}` : null,
      event.jobBytes != null ? `${t('Bytes')}: ${formatBytes(event.jobBytes)}` : null,
    ].filter(Boolean)

    return {
      ...event,
      sourceLabel: t(sourceLabels[event.source] ?? 'Director'),
      timestampLabel: formatTimestamp(event.timestamp),
      detailLine: detailParts.join(' · '),
    }
  })
))

const highlightedLines = computed(() => {
  if (!logLines.value) return []
  return logLines.value.split('\n').map(line => {
    const l = line.toLowerCase()
    if (/error|fatal|failed/.test(l))               return { text: line, type: 'error'   }
    if (/warning|warn/.test(l))                      return { text: line, type: 'warning' }
    if (/\bok\b|termination:.*ok|backup ok/.test(l)) return { text: line, type: 'ok'      }
    return { text: line, type: 'normal' }
  })
})

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
    title: t('Rerun Job'),
    message: t('Rerun job {name} (ID {id})?', { name: job.value.name, id: job.value.id }),
    ok:     { label: t('Rerun'), color: 'primary', flat: true },
    cancel: { label: t('Cancel'), flat: true },
  }).onOk(doRerun)
}

async function doRerun() {
  rerunLoading.value = true
  try {
    const res   = await director.call(`rerun jobid=${currentJobId.value} yes`)
    const newId = res?.run?.jobid ?? res?.jobid ?? null
    $q.notify({ type: 'positive', message: newId ? t('Job restarted as ID {id}.', { id: newId }) : t('Job restarted.') })
    if (newId) {
      router.push({
        name: 'job-details',
        params: { id: newId },
        query: buildJobDetailsQuery({
          director: currentJobDirector.value,
          jobsAction: typeof route.query.jobsAction === 'string' ? route.query.jobsAction : '',
          jobsStatus: typeof route.query.jobsStatus === 'string' ? route.query.jobsStatus : '',
          jobsSearch: typeof route.query.jobsSearch === 'string' ? route.query.jobsSearch : '',
          clientName: clientOrigin.value?.name,
          clientDirector: clientOrigin.value?.director,
          clientsTab: clientOrigin.value?.clientsTab,
          clientsScopeDirector: clientOrigin.value?.scopeDirector,
          clientDashboardOrigin: clientOrigin.value?.dashboardOrigin,
          clientJobsAction: clientOrigin.value?.jobsAction,
          clientJobsStatus: clientOrigin.value?.jobsStatus,
          clientJobsSearch: clientOrigin.value?.jobsSearch,
          clientJobsScopeDirector: clientOrigin.value?.jobsScopeDirector,
          volumeName: volumeOrigin.value?.name,
          volumeDirector: volumeOrigin.value?.director,
          restoreClient: restoreOrigin.value?.client,
          restoreDirector: restoreOrigin.value?.director,
          restoreJobid: restoreOrigin.value?.jobid,
          directorTab: directorOrigin.value?.tab,
          directorTarget: directorOrigin.value?.targetDirector,
          dashboardOrigin: dashboardOrigin.value,
        }),
      })
    }
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Rerun failed')}: ${e.message}` })
  } finally {
    rerunLoading.value = false
  }
}

function confirmCancel() {
  $q.dialog({
    title: t('Cancel Job'),
    message: t('Cancel job {name} (ID {id})?', { name: job.value.name, id: job.value.id }),
    ok:     { label: t('Cancel Job'), color: 'negative', flat: true },
    cancel: { label: t('Keep Running'), flat: true },
  }).onOk(doCancel)
}

async function doCancel() {
  cancelLoading.value = true
  try {
    await director.call(`cancel jobid=${currentJobId.value} yes`)
    $q.notify({ type: 'positive', message: t('Job {id} cancelled.', { id: currentJobId.value }) })
    await loadJob()
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Cancel failed')}: ${e.message}` })
  } finally {
    cancelLoading.value = false
  }
}

function copyLog() {
  navigator.clipboard?.writeText(jobLog.value)
  $q.notify({ message: t('Log copied to clipboard'), color: 'positive', icon: 'check', timeout: 1500 })
}
</script>

<style scoped>
.job-log {
  font-family: monospace;
  font-size: 0.8rem;
  max-height: 500px;
  overflow-y: auto;
  background: #1e1e1e;
  border-radius: 4px;
}

.log-line {
  white-space: pre-wrap;
  word-break: break-all;
  line-height: 1.55;
  padding: 0 2px;
  border-radius: 2px;
}

.log-line--normal  { color: #d4d4d4; }
.log-line--ok      { color: #89d185; }
.log-line--warning { color: #f2c037; }
.log-line--error   { color: #f48771; font-weight: 600; background: rgba(244, 135, 113, 0.08); }
</style>
