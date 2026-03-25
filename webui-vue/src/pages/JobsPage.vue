<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     label="Show"     no-caps />
      <q-tab name="actions"  label="Actions"  no-caps />
      <q-tab name="run"      label="Run"      no-caps />
      <q-tab name="rerun"    label="Rerun"    no-caps />
      <q-tab name="timeline" label="Timeline" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>

      <!-- ── SHOW ─────────────────────────────────────────────────────────── -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Job List</span>
            <q-space />
            <q-input v-model="search" dense outlined placeholder="Search…" class="q-mr-sm" style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <span class="text-white text-caption q-mr-sm" style="opacity:0.7">↻ {{ countdown }}s</span>
            <q-btn flat round dense icon="refresh" color="white" @click="manualRefresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
            <div v-if="statusFilter" class="q-px-md q-pt-sm">
              <q-chip removable color="primary" text-color="white" icon="filter_list"
                      @remove="statusFilter = ''" class="q-mb-xs">
                Status: {{ jobStatusMap[statusFilter]?.label ?? statusFilter }}
              </q-chip>
            </div>
            <q-table
              :rows="filteredJobs"
              :columns="columns"
              row-key="id"
              dense flat
              :loading="loading"
              :pagination="{ rowsPerPage: 15, sortBy: 'id', descending: true }"
            >
              <template #body-cell-id="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'job-details', params: { id: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props" class="text-center">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-type="props">
                <q-td :props="props">{{ typeMap[props.value] || props.value }}</q-td>
              </template>
              <template #body-cell-level="props">
                <q-td :props="props">{{ levelMap[props.value] || props.value }}</q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right" style="min-width:90px">
                  <div>{{ fmtBytes(props.row.bytes) }}</div>
                  <q-linear-progress
                    :value="bytesGauge(props.row.bytes)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-files="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ props.value.toLocaleString() }}</div>
                  <q-linear-progress
                    :value="filesGauge(props.row.files)"
                    color="teal" track-color="grey-3"
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
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center" style="white-space:nowrap">
                  <q-btn flat round dense size="sm" icon="restart_alt" title="Rerun"
                         @click="confirmRerun(props.row)" class="q-mr-xs" />
                  <q-btn v-if="isRunning(props.row.status)"
                         flat round dense size="sm" icon="cancel" color="negative" title="Cancel"
                         @click="confirmCancel(props.row)" class="q-mr-xs" />
                  <q-btn flat round dense size="sm" icon="info" title="Details"
                         :to="{ name: 'job-details', params: { id: props.row.id } }" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── ACTIONS ───────────────────────────────────────────────────────── -->
      <q-tab-panel name="actions" class="q-pa-none q-gutter-md">

        <!-- Running jobs -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Running Jobs</span>
            <q-space />
            <q-btn v-if="runningJobs.length" flat dense no-caps
                   icon="cancel" color="white" label="Cancel All"
                   @click="cancelAll" class="q-mr-xs" />
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="runningJobs"
              :columns="runningColumns"
              row-key="id"
              dense flat
              :pagination="{ rowsPerPage: 10 }"
              no-data-label="No jobs currently running"
            >
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm" icon="cancel" color="negative" title="Cancel"
                         @click="confirmCancel(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Enable / Disable jobs -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Enable / Disable Jobs</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="loadJobDefs" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="jobDefs"
              :columns="defsColumns"
              row-key="name"
              dense flat
              :loading="loadingDefs"
              :pagination="{ rowsPerPage: 15 }"
            >
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-chip dense square
                    :color="props.value ? 'positive' : 'grey'"
                    text-color="white"
                    :label="props.value ? 'enabled' : 'disabled'"
                    style="font-size:0.7rem" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center" style="white-space:nowrap">
                  <q-btn flat dense no-caps size="sm" icon="play_arrow" color="positive"
                         label="Enable"  class="q-mr-xs"
                         :disable="props.row.enabled"
                         @click="enableJob(props.row.name)" />
                  <q-btn flat dense no-caps size="sm" icon="pause" color="warning"
                         label="Disable" class="q-mr-xs"
                         :disable="!props.row.enabled"
                         @click="disableJob(props.row.name)" />
                  <q-btn flat dense no-caps size="sm" icon="send" color="primary"
                         label="Run"
                         @click="runThisJob(props.row.name)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── RUN ───────────────────────────────────────────────────────────── -->
      <q-tab-panel name="run">
        <q-card flat bordered class="bareos-panel" style="max-width:640px">
          <q-card-section class="panel-header">Run Job</q-card-section>
          <q-card-section>
            <q-form @submit.prevent="runJob" class="q-gutter-md">
              <q-select v-model="runForm.job"     :options="dotJobs"     label="Job *"     outlined dense
                        @update:model-value="onJobSelected" />
              <q-select v-model="runForm.client"  :options="dotClients"  label="Client"    outlined dense clearable />
              <q-select v-model="runForm.fileset" :options="dotFilesets" label="Fileset"   outlined dense clearable />
              <q-select v-model="runForm.pool"    :options="dotPools"    label="Pool"      outlined dense clearable />
              <q-select v-model="runForm.storage" :options="dotStorages" label="Storage"   outlined dense clearable />
              <q-select v-model="runForm.level"   :options="levels"      label="Level"     outlined dense />
              <q-input  v-model="runForm.when"                           label="When (optional)" outlined dense
                        placeholder="YYYY-MM-DD HH:MM:SS" />
              <q-input  v-model.number="runForm.priority" type="number" label="Priority" outlined dense style="max-width:140px" />
              <div>
                <q-btn type="submit" color="primary" label="Run Job" icon="play_arrow"
                       no-caps :loading="runLoading" :disable="!runForm.job" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── RERUN ─────────────────────────────────────────────────────────── -->
      <q-tab-panel name="rerun" class="q-pa-none q-gutter-md">

        <!-- Completed jobs table -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Completed Jobs</span>
            <q-space />
            <q-input v-model="rerunSearch" dense outlined placeholder="Search…" class="q-mr-sm"
                     style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="rerunableJobs"
              :columns="rerunColumns"
              row-key="id"
              dense flat
              :pagination="{ rowsPerPage: 15, sortBy: 'id', descending: true }"
            >
              <template #body-cell-status="props">
                <q-td :props="props" class="text-center">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-id="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'job-details', params: { id: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right">{{ fmtBytes(props.row.bytes) }}</q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm" icon="restart_alt" color="primary" title="Rerun"
                         @click="confirmRerun(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Manual ID fallback -->
        <q-card flat bordered class="bareos-panel" style="max-width:400px">
          <q-card-section class="panel-header">Rerun by Job ID</q-card-section>
          <q-card-section>
            <q-form @submit.prevent="submitRerun" class="q-gutter-md">
              <q-input v-model="rerunJobId" label="Job ID *" outlined dense
                       type="number" style="max-width:180px"
                       hint="Enter the ID of any completed job" />
              <div>
                <q-btn type="submit" color="primary" label="Rerun" icon="restart_alt"
                       no-caps :loading="rerunLoading" :disable="!rerunJobId" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>

      </q-tab-panel>

      <!-- ── TIMELINE ──────────────────────────────────────────────────────── -->
      <q-tab-panel name="timeline">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Job Timeline</q-card-section>
          <q-card-section>
            <div class="text-grey text-center q-py-xl">
              <q-icon name="timeline" size="48px" class="q-mb-md" />
              <div>Timeline chart will be rendered here (requires Chart.js integration)</div>
            </div>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useQuasar } from 'quasar'
import { jobTypeMap, jobLevelMap, jobStatusMap, formatBytes } from '../mock/index.js'
import { useDirectorFetch, normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route    = useRoute()
const $q       = useQuasar()
const director = useDirectorStore()

const tab          = ref(route.query.action || 'list')
const search       = ref('')
const statusFilter = ref(route.query.status || '')
const typeMap   = jobTypeMap
const levelMap  = jobLevelMap
const fmtBytes  = formatBytes

// ── job list ──────────────────────────────────────────────────────────────────
const { data: rawJobs, loading, error, refresh } = useDirectorFetch('list jobs', 'jobs')

const jobs = computed(() => (rawJobs.value ?? []).map(normaliseJob))

const filteredJobs = computed(() => {
  let list = jobs.value
  if (statusFilter.value) list = list.filter(j => j.status === statusFilter.value)
  if (!search.value) return list
  const q = search.value.toLowerCase()
  return list.filter(j =>
    j.name.toLowerCase().includes(q) ||
    j.client.toLowerCase().includes(q) ||
    String(j.id).includes(q)
  )
})

const maxBytesLog = computed(() => Math.log(Math.max(1, ...filteredJobs.value.map(j => j.bytes)) + 1))
const maxFilesLog = computed(() => Math.log(Math.max(1, ...filteredJobs.value.map(j => j.files)) + 1))

function bytesGauge(val)  { return Math.log(val + 1) / maxBytesLog.value }
function filesGauge(val)  { return Math.log(val + 1) / maxFilesLog.value }

function parseDurationSecs(str) {
  if (!str) return 0
  const parts = String(str).split(':').map(Number)
  if (parts.length === 3) return parts[0] * 3600 + parts[1] * 60 + parts[2]
  if (parts.length === 2) return parts[0] * 60 + parts[1]
  return Number(str) || 0
}

const maxDurationLog = computed(() => {
  const max = Math.max(1, ...filteredJobs.value.map(j => parseDurationSecs(j.duration)))
  return Math.log(max + 1)
})

function durationGauge(str) {
  const secs = parseDurationSecs(str)
  return Math.log(secs + 1) / maxDurationLog.value
}

const runningJobs  = computed(() => jobs.value.filter(j => isRunning(j.status)))

// Rerunable = any finished job (not currently running)
const rerunSearch  = ref('')
const rerunableJobs = computed(() => {
  const base = jobs.value.filter(j => !isRunning(j.status))
  if (!rerunSearch.value) return base
  const q = rerunSearch.value.toLowerCase()
  return base.filter(j =>
    j.name.toLowerCase().includes(q) ||
    j.client.toLowerCase().includes(q) ||
    String(j.id).includes(q)
  )
})

function isRunning(status) {
  // R = Running, l = data saved (still active)
  return status === 'R' || status === 'l'
}

// ── columns ───────────────────────────────────────────────────────────────────
const columns = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true, style: 'width:60px' },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',   sortable: true },
  { name: 'type',      label: 'Type',     field: 'type',      align: 'center'  },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center'  },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',   sortable: true },
  { name: 'endtime',   label: 'End',      field: 'endtime',   align: 'left'    },
  { name: 'duration',  label: 'Duration', field: 'duration',  align: 'right',  sortable: true,
    sort: (a, b) => parseDurationSecs(a) - parseDurationSecs(b) },
  { name: 'files',     label: 'Files',    field: 'files',     align: 'right',  sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right',  sortable: true },
  { name: 'errors',    label: 'Errors',   field: 'errors',    align: 'center'  },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center', sortable: true },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:100px' },
]

const runningColumns = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left'    },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left'    },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:60px' },
]

const rerunColumns = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true, style: 'width:60px' },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',   sortable: true },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center'  },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',   sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right'   },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center', sortable: true },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:60px' },
]

const defsColumns = [
  { name: 'name',    label: 'Job Name', field: 'name',    align: 'left',   sortable: true },
  { name: 'type',    label: 'Type',     field: 'type',    align: 'center'  },
  { name: 'enabled', label: 'Status',   field: 'enabled', align: 'center'  },
  { name: 'actions', label: '',         field: 'actions', align: 'center', style: 'width:220px' },
]

// ── job definitions (for enable/disable) ──────────────────────────────────────
const jobDefs       = ref([])
const loadingDefs   = ref(false)

async function loadJobDefs() {
  if (!director.isConnected) return
  loadingDefs.value = true
  try {
    const res = await director.call('show jobs')
    // show jobs returns {jobs: {"JobName": {name, type, enabled, ...}, ...}}
    const raw = res?.jobs ?? {}
    const list = Array.isArray(raw) ? raw : Object.values(raw)
    jobDefs.value = list.map(j => ({
      name:    j.name,
      type:    j.type ?? '',
      enabled: j.enabled !== false,   // treat undefined as enabled
    })).sort((a, b) => a.name.localeCompare(b.name))
  } catch (e) {
    $q.notify({ type: 'negative', message: `Could not load job definitions: ${e.message}` })
  } finally {
    loadingDefs.value = false
  }
}

// ── row-level actions ─────────────────────────────────────────────────────────
function confirmCancel(job) {
  $q.dialog({
    title: 'Cancel Job',
    message: `Cancel job <b>${job.name}</b> (ID&nbsp;${job.id})?`,
    html: true,
    ok:     { label: 'Cancel Job', color: 'negative', flat: true },
    cancel: { label: 'Keep', flat: true },
  }).onOk(() => doCancel(job))
}

async function doCancel(job) {
  try {
    await director.call(`cancel jobid=${job.id} yes`)
    $q.notify({ type: 'positive', message: `Job ${job.id} cancelled.` })
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Cancel failed: ${e.message}` })
  }
}

function confirmRerun(job) {
  $q.dialog({
    title: 'Rerun Job',
    message: `Rerun job <b>${job.name}</b> (ID&nbsp;${job.id})?`,
    html: true,
    ok:     { label: 'Rerun', color: 'primary', flat: true },
    cancel: { label: 'Cancel', flat: true },
  }).onOk(() => doRerun(job.id))
}

async function doRerun(jobId) {
  try {
    const res = await director.call(`rerun jobid=${jobId} yes`)
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `Job restarted as ID ${newId}.` })
    tab.value = 'list'
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Rerun failed: ${e.message}` })
  }
}

// ── bulk cancel ───────────────────────────────────────────────────────────────
function cancelAll() {
  const n = runningJobs.value.length
  $q.dialog({
    title: 'Cancel All Running Jobs',
    message: `Cancel all <b>${n}</b> running job${n === 1 ? '' : 's'}?`,
    html: true,
    ok:     { label: 'Cancel All', color: 'negative', flat: true },
    cancel: { label: 'Keep', flat: true },
  }).onOk(async () => {
    const results = await Promise.allSettled(
      runningJobs.value.map(j => director.call(`cancel jobid=${j.id} yes`))
    )
    const failed = results.filter(r => r.status === 'rejected').length
    if (failed) {
      $q.notify({ type: 'warning', message: `Cancelled ${n - failed} / ${n} jobs (${failed} failed).` })
    } else {
      $q.notify({ type: 'positive', message: `All ${n} running jobs cancelled.` })
    }
    refresh()
  })
}

// ── enable / disable ──────────────────────────────────────────────────────────
async function enableJob(name) {
  try {
    await director.call(`enable job="${name}" yes`)
    $q.notify({ type: 'positive', message: `Job "${name}" enabled.` })
    const j = jobDefs.value.find(d => d.name === name)
    if (j) j.enabled = true
  } catch (e) {
    $q.notify({ type: 'negative', message: `Enable failed: ${e.message}` })
  }
}

async function disableJob(name) {
  try {
    await director.call(`disable job="${name}" yes`)
    $q.notify({ type: 'positive', message: `Job "${name}" disabled.` })
    const j = jobDefs.value.find(d => d.name === name)
    if (j) j.enabled = false
  } catch (e) {
    $q.notify({ type: 'negative', message: `Disable failed: ${e.message}` })
  }
}

function runThisJob(name) {
  if (dotJobs.value.length === 0) loadRunOptions()
  runForm.value = { job: name, client: null, fileset: null, pool: null, storage: null, level: 'Incremental', when: '', priority: 10 }
  onJobSelected(name)
  tab.value = 'run'
}

// ── run form ──────────────────────────────────────────────────────────────────
const dotJobs     = ref([])
const dotClients  = ref([])
const dotFilesets = ref([])
const dotPools    = ref([])
const dotStorages = ref([])
const levels      = ['Full', 'Incremental', 'Differential']
const runLoading  = ref(false)

const runForm = ref({ job: null, client: null, fileset: null, pool: null, storage: null, level: 'Incremental', when: '', priority: 10 })

async function loadRunOptions() {
  if (!director.isConnected) return
  try {
    const [j, c, f, p, s] = await Promise.all([
      director.call('.jobs'),
      director.call('.clients'),
      director.call('.filesets'),
      director.call('.pools'),
      director.call('.storage'),
    ])
    dotJobs.value     = (j?.jobs     ?? []).map(x => x.name).sort()
    dotClients.value  = (c?.clients  ?? []).map(x => x.name).sort()
    dotFilesets.value = (f?.filesets ?? []).map(x => x.name).sort()
    dotPools.value    = (p?.pools    ?? []).map(x => x.name).sort()
    dotStorages.value = (s?.storages ?? []).map(x => x.name).sort()
  } catch (e) {
    // non-fatal — user can still type values
    console.warn('Could not load run form options:', e.message)
  }
}

async function onJobSelected(name) {
  if (!name) return
  try {
    const res = await director.call(`.defaults job="${name}"`)
    const d   = res?.defaults ?? res ?? {}
    if (d.client)   runForm.value.client   = d.client
    if (d.fileset)  runForm.value.fileset  = d.fileset
    if (d.pool)     runForm.value.pool     = d.pool
    if (d.storage)  runForm.value.storage  = d.storage
    if (d.level)    runForm.value.level    = d.level
    if (d.priority) runForm.value.priority = Number(d.priority)
  } catch { /* ignore */ }
}

async function runJob() {
  const f = runForm.value
  if (!f.job) return
  let cmd = `run job="${f.job}"`
  if (f.client)   cmd += ` client="${f.client}"`
  if (f.fileset)  cmd += ` fileset="${f.fileset}"`
  if (f.pool)     cmd += ` pool="${f.pool}"`
  if (f.storage)  cmd += ` storage="${f.storage}"`
  if (f.level)    cmd += ` level=${f.level}`
  if (f.when)     cmd += ` when="${f.when}"`
  if (f.priority) cmd += ` priority=${f.priority}`
  cmd += ' yes'

  runLoading.value = true
  try {
    const res   = await director.call(cmd)
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `Job started — ID ${newId}` })
    tab.value = 'list'
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Run failed: ${e.message}` })
  } finally {
    runLoading.value = false
  }
}

// ── rerun tab ─────────────────────────────────────────────────────────────────
const rerunJobId   = ref('')
const rerunLoading = ref(false)

async function submitRerun() {
  if (!rerunJobId.value) return
  rerunLoading.value = true
  try {
    await doRerun(rerunJobId.value)
    rerunJobId.value = ''
  } finally {
    rerunLoading.value = false
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
const REFRESH_INTERVAL = 10
const countdown = ref(REFRESH_INTERVAL)
let _timer = null

function startAutoRefresh() {
  stopAutoRefresh()
  countdown.value = REFRESH_INTERVAL
  _timer = setInterval(() => {
    countdown.value -= 1
    if (countdown.value <= 0) {
      refresh()
      countdown.value = REFRESH_INTERVAL
    }
  }, 1000)
}

function stopAutoRefresh() {
  clearInterval(_timer)
  _timer = null
}

function manualRefresh() {
  refresh()
  countdown.value = REFRESH_INTERVAL
}

onMounted(() => {
  if (director.isConnected) {
    loadJobDefs()
    loadRunOptions()
  }
  startAutoRefresh()
})

onUnmounted(stopAutoRefresh)

watch(() => director.isConnected, (connected) => {
  if (connected) {
    loadJobDefs()
    loadRunOptions()
    startAutoRefresh()
  }
})

// Reload defs when switching to the Actions tab
watch(tab, (t) => {
  if (t === 'actions' && jobDefs.value.length === 0) loadJobDefs()
  if (t === 'run'     && dotJobs.value.length === 0)  loadRunOptions()
})
</script>

