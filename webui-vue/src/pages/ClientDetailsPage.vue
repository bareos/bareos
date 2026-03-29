<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" label="Loading client…" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>
    <template v-else-if="!loading">
    <div class="row items-center q-mb-md">
      <q-btn flat icon="arrow_back" label="Back to Clients" :to="{ name: 'clients' }" no-caps class="q-mr-md" />
      <q-icon v-if="client" :name="osIcon(client)" :color="osColor(client)" size="28px" class="q-mr-sm" />
      <div class="text-h6">{{ client?.name }}</div>
      <q-badge v-if="client?.version" color="grey-6" :label="'v' + client.version" class="q-ml-sm text-mono" />
    </div>
    <div v-if="client" class="row q-col-gutter-md">
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">Client Details</q-card-section>
          <q-card-section>
            <q-list dense separator>
              <q-item v-for="row in details" :key="row.label">
                <q-item-section class="text-weight-medium" style="max-width:140px">{{ row.label }}</q-item-section>
                <q-item-section>
                  <span v-if="row.icon" class="row items-center no-wrap">
                    <q-icon :name="row.icon" :color="row.iconColor" size="18px" class="q-mr-xs" />
                    {{ row.value }}
                  </span>
                  <span v-else>{{ row.value }}</span>
                </q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Recent Jobs</span>
            <q-space />
            <q-btn flat round dense color="white" size="sm" class="q-mr-xs"
                   :icon="relativeStart ? 'calendar_today' : 'schedule'"
                   :title="relativeStart ? 'Show absolute start time' : 'Show relative start time'"
                   @click="relativeStart = !relativeStart" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="clientJobs" :columns="jobCols" row-key="id" dense flat hide-bottom
                     :pagination="{ rowsPerPage: 6 }">
              <template #body-cell-id="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'job-details', params: { id: props.value } }" class="text-primary">
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
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-starttime="props">
                <q-td :props="props">
                  <span :title="relativeStart ? props.value : timeAgo(props.value)">
                    {{ relativeStart ? timeAgo(props.value) : props.value }}
                  </span>
                </q-td>
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
            </q-table>
          </q-card-section>
        </q-card>
      </div>
    </div>
    <div v-else class="text-grey text-center q-pa-xl">Client not found.</div>
    </template>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { formatBytes, timeAgo } from '../mock/index.js'
import { normaliseClient, normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'

const route         = useRoute()
const director      = useDirectorStore()
const fmtBytes      = formatBytes
const relativeStart = ref(false)

const loading    = ref(true)
const clientData = ref(null)
const clientJobs = ref([])
const error      = ref(null)

function osIcon(client)  { return osIconName(client)  }
function osColor(client) { return osIconColor(client) }

onMounted(async () => {
  const name = route.params.name
  try {
    const [clientRes, jobsRes, defaultsRes] = await Promise.allSettled([
      director.call(`llist clients`),
      director.call(`list jobs client=${name} reverse`),
      director.call(`.defaults client=${name}`),
    ])
    if (clientRes.status === 'fulfilled') {
      const list  = clientRes.value?.clients ?? []
      const found = list.find(c => c.name === name)
      if (found) {
        const defaults = defaultsRes.status === 'fulfilled' ? (defaultsRes.value ?? {}) : {}
        clientData.value = normaliseClient({ ...found, ...defaults })
      } else {
        clientData.value = null
      }
    }
    if (jobsRes.status === 'fulfilled') {
      clientJobs.value = (jobsRes.value?.jobs ?? []).map(normaliseJob)
    }
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
})

const client = computed(() => clientData.value)

const maxBytes = computed(() =>
  Math.max(1, ...clientJobs.value.map(j => Number(j.bytes) || 0))
)
function bytesGauge(val) {
  const n = Number(val) || 0
  return n / maxBytes.value
}

const details = computed(() => {
  if (!client.value) return []
  const c = client.value
  return [
    { label: 'OS',             value: osLabel(c.os) + (c.osInfo ? ` — ${c.osInfo}` : ''),
      icon: osIcon(c), iconColor: osColor(c) },
    { label: 'Architecture',   value: c.arch      || '—' },
    { label: 'Version',        value: c.version   || '—' },
    { label: 'Build Date',     value: c.buildDate || '—' },
    { label: 'Address',        value: c.address   || '—' },
    { label: 'Port',           value: c.port      || '—' },
    { label: 'Status',         value: c.enabled   ? 'Enabled' : 'Disabled' },
    { label: 'File Retention', value: c.fileretention || '—' },
    { label: 'Job Retention',  value: c.jobretention  || '—' },
  ]
})

const jobCols = [
  { name: 'id',        label: 'ID',     field: 'id',        align: 'right' },
  { name: 'name',      label: 'Job',    field: 'name',      align: 'left'  },
  { name: 'level',     label: 'Level',  field: 'level',     align: 'center'},
  { name: 'starttime', label: 'Start',  field: 'starttime', align: 'left'  },
  { name: 'bytes',     label: 'Bytes',  field: 'bytes',     align: 'right' },
  { name: 'status',    label: 'Status', field: 'status',    align: 'center'},
]
</script>
