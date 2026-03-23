<template>
  <q-page class="q-pa-md">
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
                <q-item-section class="text-weight-medium" style="max-width:120px">{{ row.label }}</q-item-section>
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
  </q-page>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { mockClients, mockJobs, formatBytes } from '../mock/index.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route = useRoute()
const client = computed(() => mockClients.find(c => c.name === route.params.name))
const clientJobs = computed(() => mockJobs.filter(j => j.client === route.params.name))
const fmtBytes = formatBytes

const details = computed(() => client.value ? [
  { label: 'Name',    value: client.value.name },
  { label: 'OS',      value: client.value.uname },
  { label: 'Version', value: client.value.version },
  { label: 'Status',  value: client.value.enabled ? 'Enabled' : 'Disabled' },
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
