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

      <!-- STATUS: upcoming scheduled jobs -->
      <q-tab-panel name="status" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Upcoming Scheduled Jobs</span>
            <q-space />
            <q-select v-model="statusDays" :options="[1,3,7,14,30]" dense outlined
                      prefix="Next " suffix=" days" color="white" dark
                      style="min-width:130px" @update:model-value="refreshStatus" />
            <q-btn flat round dense icon="refresh" color="white" class="q-ml-sm" @click="refreshStatus" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="statusError" dense class="bg-negative text-white">{{ statusError }}</q-banner>
            <q-table :rows="scheduledJobs" :columns="statusCols" row-key="idx" dense flat
                     :loading="statusLoading" :pagination="{ rowsPerPage: 25 }">
              <template #body-cell-level="props">
                <q-td :props="props">
                  <q-badge color="blue-grey" :label="props.value" />
                </q-td>
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
const scheduledJobs  = ref([])
const statusDays     = ref(1)

async function refreshStatus() {
  statusLoading.value = true
  statusError.value   = null
  try {
    // status director returns scheduled jobs in "scheduled" key
    const res = await director.call(`status director days=${statusDays.value}`)
    const raw = res?.scheduled ?? res?.['scheduled-jobs'] ?? []
    scheduledJobs.value = (Array.isArray(raw) ? raw : []).map((r, idx) => ({
      idx,
      level:    r.level    ?? r.Level    ?? '',
      type:     r.type     ?? r.Type     ?? r.jobtype ?? '',
      priority: r.priority ?? r.Priority ?? '',
      runtime:  r.scheduled ?? r.runtime ?? r.Runtime ?? '',
      name:     r.name     ?? r.Job      ?? r.job     ?? '',
      volume:   r.volume   ?? r.Volume   ?? '',
    }))
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

const statusCols = [
  { name: 'runtime',  label: 'Scheduled',  field: 'runtime',  align: 'left',   sortable: true },
  { name: 'name',     label: 'Job',        field: 'name',     align: 'left',   sortable: true },
  { name: 'level',    label: 'Level',      field: 'level',    align: 'center'  },
  { name: 'type',     label: 'Type',       field: 'type',     align: 'center'  },
  { name: 'priority', label: 'Pri',        field: 'priority', align: 'center'  },
  { name: 'volume',   label: 'Volume',     field: 'volume',   align: 'left'    },
]

// Load show tab immediately; load status tab lazily on first visit
refreshSchedules()
watch(tab, t => { if (t === 'status' && !scheduledJobs.value.length) refreshStatus() })
</script>
