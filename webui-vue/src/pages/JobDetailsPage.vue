<template>
  <q-page class="q-pa-md">
    <div v-if="job">
      <div class="row items-center q-mb-md">
        <q-btn flat icon="arrow_back" label="Back to Jobs" :to="{ name: 'jobs' }" no-caps class="q-mr-md" />
        <div class="text-h6">Job #{{ job.id }} — {{ job.name }}</div>
        <q-space />
        <q-badge :color="statusMap[job.status]?.color" :label="statusMap[job.status]?.label" class="text-body2 q-px-md q-py-xs" />      </div>

      <div class="row q-col-gutter-md q-mt-none">
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
        <div class="col-12 col-md-6">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">Actions</q-card-section>
            <q-card-section class="q-gutter-sm">
              <q-btn icon="restart_alt" label="Rerun Job" color="primary" no-caps />
              <q-btn v-if="job.status === 'R'" icon="cancel" label="Cancel Job" color="negative" no-caps />
            </q-card-section>
          </q-card>
        </div>
        <div class="col-12">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header row items-center">
              <span>Job Log</span>
              <q-space />
              <q-btn flat round dense icon="content_copy" size="sm" color="white" title="Copy log" @click="copyLog" />
            </q-card-section>
            <q-card-section class="q-pa-none">
              <pre class="job-log q-pa-md q-ma-none">{{ jobLog }}</pre>
            </q-card-section>
          </q-card>
        </div>
      </div>
    </div>
    <div v-else class="text-center q-pa-xl text-grey">Job not found.</div>
  </q-page>
</template>

<script setup>
import { computed } from 'vue'
import { useRoute } from 'vue-router'
import { useQuasar } from 'quasar'
import { mockJobs, jobStatusMap, jobTypeMap, jobLevelMap, formatBytes } from '../mock/index.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route = useRoute()
const $q = useQuasar()
const job = computed(() => mockJobs.find(j => j.id === Number(route.params.id)))
const statusMap = jobStatusMap

const summaryRows = computed(() => job.value ? [
  { label: 'Job ID',     value: job.value.id },
  { label: 'Job Name',   value: job.value.name },
  { label: 'Client',     value: job.value.client },
  { label: 'Type',       value: jobTypeMap[job.value.type] || job.value.type },
  { label: 'Level',      value: jobLevelMap[job.value.level] || job.value.level },
  { label: 'Start Time', value: job.value.starttime },
  { label: 'End Time',   value: job.value.endtime || '—' },
  { label: 'Duration',   value: job.value.duration || '—' },
  { label: 'Files',      value: job.value.files.toLocaleString() },
  { label: 'Bytes',      value: formatBytes(job.value.bytes) },
  { label: 'Errors',     value: job.value.errors },
] : [])

const jobLog = computed(() => {
  if (!job.value) return ''
  return `23-Mar-26 ${job.value.starttime?.slice(11) || '00:00:00'} bareos-dir JobId ${job.value.id}: Start Backup JobId ${job.value.id}, Job=${job.value.name}.2026-03-23_080001_04
23-Mar-26 ${job.value.starttime?.slice(11) || '00:00:00'} bareos-dir JobId ${job.value.id}: Using Device "FileStorage" to write.
23-Mar-26 ${job.value.starttime?.slice(11) || '00:00:00'} bareos-fd  JobId ${job.value.id}: Connected to Storage daemon at bareos-sd:9103
23-Mar-26 ${job.value.starttime?.slice(11) || '00:00:00'} bareos-fd  JobId ${job.value.id}: Starting ${jobLevelMap[job.value.level] || job.value.level} Backup of FileSet "Linux All"
23-Mar-26 ${job.value.endtime?.slice(11) || '--:--:--'} bareos-dir JobId ${job.value.id}: Bareos bareos-dir ${job.value.status === 'T' ? 'OK' : 'Error'} -- ${job.value.client} ${jobTypeMap[job.value.type]} ${jobLevelMap[job.value.level] || ''} Backup -- Files: ${job.value.files.toLocaleString()}, Bytes: ${formatBytes(job.value.bytes)}, Termination: ${job.value.status === 'T' ? 'Backup OK' : 'Backup FAILED'}`
})

function copyLog() {
  navigator.clipboard?.writeText(jobLog.value)
  $q.notify({ message: 'Log copied to clipboard', color: 'positive', icon: 'check', timeout: 1500 })
}
</script>
