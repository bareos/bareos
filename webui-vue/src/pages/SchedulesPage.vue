<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="show"   label="Show"    no-caps />
      <q-tab name="status" label="Status"  no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>

      <!-- SHOW: all configured schedules -->
      <q-tab-panel name="show" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Schedules</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshSchedules" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="schedError" dense class="bg-negative text-white">{{ schedError }}</q-banner>
            <q-table :rows="schedules" :columns="schedCols" row-key="name" dense flat
                     :loading="schedLoading" :pagination="{ rowsPerPage: 20 }">
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'"
                           :label="props.value ? 'Enabled' : 'Disabled'" />
                </q-td>
              </template>
              <template #body-cell-run="props">
                <q-td :props="props">
                  <div v-for="(r, i) in props.value" :key="i" class="text-caption text-mono">{{ r }}</div>
                  <span v-if="!props.value?.length" class="text-grey-5">—</span>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm"
                         :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                         :color="props.row.enabled ? 'warning' : 'positive'"
                         :title="props.row.enabled ? 'Disable' : 'Enable'"
                         :loading="togglingName === props.row.name"
                         @click="toggleSchedule(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- STATUS: schedule→jobs mapping + hour-by-hour preview -->
      <q-tab-panel name="status" class="q-pa-none">

        <!-- header / controls -->
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header row items-center">
            <span>Scheduler Status</span>
            <q-space />
            <q-select v-model="statusDays" :options="[1,3,7,14,30]" dense outlined
                      prefix="Next " suffix=" days" color="white" dark
                      style="min-width:130px" @update:model-value="refreshStatus" />
            <q-btn flat round dense icon="refresh" color="white" class="q-ml-sm" @click="refreshStatus" />
          </q-card-section>
          <q-card-section v-if="statusError" class="q-pa-none">
            <q-banner dense class="bg-negative text-white">{{ statusError }}</q-banner>
          </q-card-section>
        </q-card>

        <!-- Scheduler Jobs: which schedules trigger which jobs -->
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">
            <span>Scheduler Jobs</span>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="scheduleJobRows" :columns="scheduleJobCols"
                     row-key="idx" dense flat :loading="statusLoading"
                     :pagination="{ rowsPerPage: 20 }">
              <template #body-cell-schedEnabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'"
                           :label="props.value ? 'Enabled' : 'Disabled'" />
                </q-td>
              </template>
              <template #body-cell-jobEnabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'"
                           :label="props.value ? 'Enabled' : 'Disabled'" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Scheduler Preview: hour-by-hour run timeline -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">
            <span>Scheduler Preview</span>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="previewRows" :columns="previewCols"
                     row-key="idx" dense flat :loading="statusLoading"
                     :pagination="{ rowsPerPage: 50 }">
              <template #body-cell-level="props">
                <q-td :props="props">
                  <q-badge v-if="props.value" color="blue-grey" :label="props.value" />
                  <span v-else class="text-grey-5">—</span>
                </q-td>
              </template>
              <template #body-cell-pool="props">
                <q-td :props="props">{{ props.value || '—' }}</q-td>
              </template>
              <template #body-cell-storage="props">
                <q-td :props="props">{{ props.value || '—' }}</q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

      </q-tab-panel>

    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useDirectorStore } from '../stores/director.js'
import { useQuasar } from 'quasar'

const director = useDirectorStore()
const $q = useQuasar()

const tab = ref('show')

// ── Show tab ──────────────────────────────────────────────────────────────────
const schedLoading  = ref(false)
const schedError    = ref(null)
const rawSchedules  = ref({})
const togglingName  = ref(null)

async function refreshSchedules() {
  schedLoading.value = true
  schedError.value   = null
  try {
    const res = await director.call('show schedules')
    // show schedules returns {"schedules": {"Name": {name, enabled, run:[...]}}}
    const raw = res?.schedules ?? {}
    rawSchedules.value = Array.isArray(raw) ? Object.fromEntries(raw.map(s => [s.name, s])) : raw
  } catch (e) {
    schedError.value = e.message
  } finally {
    schedLoading.value = false
  }
}

const schedules = computed(() =>
  Object.values(rawSchedules.value).map(s => ({
    name:    s.name ?? '',
    enabled: s.enabled !== false && s.enabled !== 0,
    run:     Array.isArray(s.run) ? s.run : (s.run ? [s.run] : []),
  }))
)

const schedCols = [
  { name: 'name',    label: 'Name',    field: 'name',    align: 'left', sortable: true },
  { name: 'enabled', label: 'Status',  field: 'enabled', align: 'center' },
  { name: 'run',     label: 'Run Directives', field: 'run', align: 'left' },
  { name: 'actions', label: '',        field: 'actions', align: 'center', style: 'width:60px' },
]

async function toggleSchedule(row) {
  const action = row.enabled ? 'disable' : 'enable'
  togglingName.value = row.name
  try {
    await director.call(`${action} schedule=${row.name}`)
    $q.notify({ type: 'positive', message: `Schedule "${row.name}" ${action}d` })
    await refreshSchedules()
  } catch (e) {
    $q.notify({ type: 'negative', message: e.message })
  } finally {
    togglingName.value = null
  }
}

// ── Status tab ────────────────────────────────────────────────────────────────
const statusLoading  = ref(false)
const statusError    = ref(null)
const statusDays     = ref(1)
const schedulesData  = ref([])   // from "schedules" key
const previewData    = ref([])   // from "preview" key

async function refreshStatus() {
  statusLoading.value = true
  statusError.value   = null
  try {
    const res = await director.call(`status scheduler days=${statusDays.value}`)
    schedulesData.value = Array.isArray(res?.schedules) ? res.schedules : []
    previewData.value   = Array.isArray(res?.preview)   ? res.preview   : []
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

// Flatten schedules→jobs into table rows
const scheduleJobRows = computed(() => {
  const rows = []
  for (const sched of schedulesData.value) {
    const jobs = Array.isArray(sched.jobs) ? sched.jobs : []
    if (!jobs.length) {
      rows.push({ idx: rows.length, schedule: sched.name, schedEnabled: sched.enabled, job: '—', jobEnabled: null })
    } else {
      for (const j of jobs) {
        rows.push({ idx: rows.length, schedule: sched.name, schedEnabled: sched.enabled, job: j.name, jobEnabled: j.enabled })
      }
    }
  }
  return rows
})

const scheduleJobCols = [
  { name: 'schedule',    label: 'Schedule', field: 'schedule',    align: 'left',   sortable: true },
  { name: 'schedEnabled',label: 'Status',   field: 'schedEnabled',align: 'center'               },
  { name: 'job',         label: 'Job',      field: 'job',         align: 'left',   sortable: true },
  { name: 'jobEnabled',  label: 'Job Status',field: 'jobEnabled', align: 'center'               },
]

// Map preview array into table rows
const previewRows = computed(() =>
  previewData.value.map((r, idx) => ({
    idx,
    datetime: r.datetime ?? '',
    schedule: r.schedule ?? '',
    level:    r.level    ?? '',
    priority: r.priority ?? '',
    pool:     r.pool     ?? '',
    storage:  r.storage  ?? '',
  }))
)

const previewCols = [
  { name: 'datetime', label: 'Date/Time', field: 'datetime', align: 'left',   sortable: true },
  { name: 'schedule', label: 'Schedule',  field: 'schedule', align: 'left',   sortable: true },
  { name: 'level',    label: 'Level',     field: 'level',    align: 'center'                 },
  { name: 'priority', label: 'Pri',       field: 'priority', align: 'center', sortable: true },
  { name: 'pool',     label: 'Pool',      field: 'pool',     align: 'left'                   },
  { name: 'storage',  label: 'Storage',   field: 'storage',  align: 'left'                   },
]

// Load show tab immediately; load status tab lazily on first visit
refreshSchedules()
watch(tab, t => { if (t === 'status') refreshStatus() })
</script>
