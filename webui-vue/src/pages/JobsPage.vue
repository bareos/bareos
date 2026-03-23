<template>
  <q-page class="q-pa-md">
    <!-- Sub-tabs -->
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     label="Show"     no-caps />
      <q-tab name="actions"  label="Actions"  no-caps />
      <q-tab name="run"      label="Run"      no-caps />
      <q-tab name="rerun"    label="Rerun"    no-caps />
      <q-tab name="timeline" label="Timeline" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- SHOW -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Job List</span>
            <q-space />
            <q-input v-model="search" dense outlined placeholder="Search…" class="q-mr-sm" style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="filteredJobs"
              :columns="columns"
              row-key="id"
              dense flat
              :filter="search"
              :pagination="{ rowsPerPage: 15 }"
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
                <q-td :props="props" class="text-right">{{ fmtBytes(props.row.bytes) }}</q-td>
              </template>
              <template #body-cell-files="props">
                <q-td :props="props" class="text-right">{{ props.value.toLocaleString() }}</q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm" icon="restart_alt" title="Rerun" class="q-mr-xs" />
                  <q-btn v-if="props.row.status === 'R'" flat round dense size="sm" icon="cancel" color="negative" title="Cancel" />
                  <q-btn flat round dense size="sm" icon="info" title="Details"
                         :to="{ name: 'job-details', params: { id: props.row.id } }" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ACTIONS -->
      <q-tab-panel name="actions">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Bulk Actions</q-card-section>
          <q-card-section>
            <div class="row q-col-gutter-md">
              <div class="col-auto">
                <q-btn color="warning" label="Cancel All Running Jobs" icon="cancel" />
              </div>
              <div class="col-auto">
                <q-btn color="info" label="Disable Scheduled Jobs" icon="pause" />
              </div>
              <div class="col-auto">
                <q-btn color="positive" label="Enable Scheduled Jobs" icon="play_arrow" />
              </div>
            </div>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- RUN -->
      <q-tab-panel name="run">
        <q-card flat bordered class="bareos-panel" style="max-width:600px">
          <q-card-section class="panel-header">Run Job</q-card-section>
          <q-card-section>
            <q-form class="q-gutter-md">
              <q-select v-model="runForm.job"      :options="jobNames"    label="Job"      outlined dense />
              <q-select v-model="runForm.client"   :options="clientNames" label="Client"   outlined dense />
              <q-select v-model="runForm.level"    :options="levels"      label="Level"    outlined dense />
              <q-select v-model="runForm.pool"     :options="pools"       label="Pool"     outlined dense />
              <q-select v-model="runForm.storage"  :options="storages"    label="Storage"  outlined dense />
              <q-input  v-model="runForm.when"                            label="When (optional)" outlined dense
                        placeholder="YYYY-MM-DD HH:MM:SS" />
              <q-btn color="primary" label="Run Job" icon="play_arrow" @click="runJob" />
            </q-form>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- RERUN -->
      <q-tab-panel name="rerun">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Rerun Job</q-card-section>
          <q-card-section>
            <q-input v-model="rerunJobId" label="Job ID" outlined dense style="max-width:200px" class="q-mb-md" />
            <q-btn color="primary" label="Rerun" icon="restart_alt" />
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- TIMELINE -->
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
import { ref, computed } from 'vue'
import { useRoute } from 'vue-router'
import { mockJobs, jobTypeMap, jobLevelMap, formatBytes } from '../mock/index.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route = useRoute()
const tab = ref(route.query.action || 'list')
const search = ref('')
const typeMap   = jobTypeMap
const levelMap  = jobLevelMap
const fmtBytes  = formatBytes

const columns = [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true, style: 'width:60px' },
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',   sortable: true },
  { name: 'type',      label: 'Type',     field: 'type',      align: 'center'  },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center'  },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',   sortable: true },
  { name: 'endtime',   label: 'End',      field: 'endtime',   align: 'left'    },
  { name: 'duration',  label: 'Duration', field: 'duration',  align: 'right'   },
  { name: 'files',     label: 'Files',    field: 'files',     align: 'right',  sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right',  sortable: true },
  { name: 'errors',    label: 'Errors',   field: 'errors',    align: 'center'  },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center', sortable: true },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:80px' },
]

const filteredJobs = computed(() => mockJobs)

const runForm = ref({ job: null, client: null, level: 'Incremental', pool: null, storage: null, when: '' })
const rerunJobId = ref('')
const jobNames   = [...new Set(mockJobs.map(j => j.name))]
const clientNames = ['bareos-fd', 'fileserver-fd', 'db-server-fd', 'mail-fd', 'web-fd']
const levels     = ['Full', 'Incremental', 'Differential']
const pools      = ['Full', 'Incremental', 'Differential', 'Scratch']
const storages   = ['File', 'Tape', 'Cloud']

function refresh() {}
function runJob() {}
</script>
