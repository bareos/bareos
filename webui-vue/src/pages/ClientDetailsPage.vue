<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" label="Loading client…" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>
    <template v-else-if="!loading">
    <div class="row items-center q-mb-md">
      <q-btn flat icon="arrow_back" label="Back to Clients" :to="{ name: 'clients' }" no-caps class="q-mr-md" />
      <div class="text-h6">{{ client?.name }}</div>
    </div>
    <div v-if="client" class="row q-col-gutter-md">
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">Client Details</q-card-section>
          <q-card-section>
            <q-list dense separator>
              <q-item v-for="row in details" :key="row.label">
                <q-item-section class="text-weight-medium" style="max-width:140px">{{ row.label }}</q-item-section>
                <q-item-section>{{ row.value }}</q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Recent Jobs</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="clientJobs" :columns="jobCols" row-key="id" dense flat hide-bottom
                     :pagination="{ rowsPerPage: 6 }">
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props">{{ fmtBytes(props.row.bytes) }}</q-td>
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
import { formatBytes } from '../mock/index.js'
import { normaliseClient, normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route    = useRoute()
const director = useDirectorStore()
const fmtBytes = formatBytes

const loading    = ref(true)
const clientData = ref(null)
const clientJobs = ref([])
const error      = ref(null)

onMounted(async () => {
  const name = route.params.name
  try {
    const [clientRes, jobsRes] = await Promise.allSettled([
      director.call(`list clients`),
      director.call(`list jobs client=${name}`),
    ])
    if (clientRes.status === 'fulfilled') {
      const list = clientRes.value?.clients ?? []
      const found = list.find(c => c.name === name)
      clientData.value = found ? normaliseClient(found) : null
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

const details = computed(() => client.value ? [
  { label: 'Name',           value: client.value.name },
  { label: 'OS/Arch',        value: client.value.uname },
  { label: 'Version',        value: client.value.version },
  { label: 'Address',        value: client.value.address || '—' },
  { label: 'Port',           value: client.value.port || '—' },
  { label: 'Status',         value: client.value.enabled ? 'Enabled' : 'Disabled' },
  { label: 'File Retention', value: client.value.fileretention || '—' },
  { label: 'Job Retention',  value: client.value.jobretention  || '—' },
] : [])

const jobCols = [
  { name: 'id',        label: 'ID',     field: 'id',        align: 'right' },
  { name: 'name',      label: 'Job',    field: 'name',      align: 'left'  },
  { name: 'level',     label: 'Level',  field: 'level',     align: 'center'},
  { name: 'starttime', label: 'Start',  field: 'starttime', align: 'left'  },
  { name: 'bytes',     label: 'Bytes',  field: 'bytes',     align: 'right' },
  { name: 'status',    label: 'Status', field: 'status',    align: 'center'},
]
</script>
