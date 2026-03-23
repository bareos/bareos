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
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="refresh" />
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
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="refresh" />
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
                  <q-badge :color="statusMap[props.value]?.color || 'grey'" :label="statusMap[props.value]?.label || props.value" />
                </q-td>
              </template>
              <template #body-cell-name="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'job-details', params: { id: props.row.id } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right">{{ fmtBytes(props.row.bytes) }}</q-td>
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
            </q-list>
          </q-card-section>
        </q-card>

        <!-- Running Jobs -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Running Jobs</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" size="sm" @click="refresh" />
          </q-card-section>
          <q-card-section style="max-height:320px; overflow-y:auto; padding:0">
            <q-list separator>
              <q-item v-for="job in runningJobs" :key="job.id" class="q-py-sm">
                <q-item-section>
                  <q-item-label>
                    <router-link :to="{ name: 'job-details', params: { id: job.id } }" class="text-primary text-weight-medium">
                      {{ job.name }}
                    </router-link>
                    <span class="text-grey-6 text-caption q-ml-xs">({{ job.client }})</span>
                  </q-item-label>
                  <q-item-label caption>
                    {{ job.files.toLocaleString() }} files &middot; {{ fmtBytes(job.bytes) }}
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
import { computed } from 'vue'
import { mockJobs, formatBytes } from '../mock/index.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import StatNumber from '../components/StatNumber.vue'

const fmtBytes = formatBytes

const recentCols = [
  { name: 'id',     label: 'ID',       field: 'id',       align: 'right', sortable: true },
  { name: 'name',   label: 'Job Name', field: 'name',     align: 'left',  sortable: true },
  { name: 'client', label: 'Client',   field: 'client',   align: 'left',  sortable: true },
  { name: 'level',  label: 'Level',    field: 'level',    align: 'center' },
  { name: 'starttime', label: 'Start', field: 'starttime',align: 'left',  sortable: true },
  { name: 'bytes',  label: 'Bytes',    field: 'bytes',    align: 'right'  },
  { name: 'status', label: 'Status',   field: 'status',   align: 'center' },
]

const recentJobs = mockJobs.slice(0, 8)
const runningJobs = mockJobs.filter(j => j.status === 'R')

const last24h = mockJobs
const summaryStats = [
  { label: 'Running',    status: 'R', color: 'info',     count: last24h.filter(j => j.status === 'R').length },
  { label: 'Waiting',    status: 'C', color: 'grey',     count: last24h.filter(j => j.status === 'C').length },
  { label: 'Successful', status: 'T', color: 'positive', count: last24h.filter(j => j.status === 'T').length },
  { label: 'Warning',    status: 'W', color: 'warning',  count: last24h.filter(j => j.status === 'W').length },
  { label: 'Failed',     status: 'f', color: 'negative', count: last24h.filter(j => j.status === 'f').length },
]

const totals = computed(() => ({
  jobs:  mockJobs.length,
  files: mockJobs.reduce((a, j) => a + j.files, 0),
  bytes: mockJobs.reduce((a, j) => a + j.bytes, 0),
}))

function refresh() { /* would trigger API reload */ }
</script>
