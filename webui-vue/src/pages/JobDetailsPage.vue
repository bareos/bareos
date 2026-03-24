<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" label="Loading job…" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>
    <div v-else-if="!loading && job">
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
    <div v-else-if="!loading" class="text-center q-pa-xl text-grey">Job not found.</div>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { useQuasar } from 'quasar'
import { jobStatusMap, jobTypeMap, jobLevelMap, formatBytes } from '../mock/index.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route   = useRoute()
const $q      = useQuasar()
const director = useDirectorStore()
const statusMap = jobStatusMap

const loading  = ref(true)
const jobData  = ref(null)
const logLines = ref('')
const error    = ref(null)

onMounted(async () => {
  try {
    const jobid = route.params.id
    const [jobRes, logRes] = await Promise.allSettled([
      director.call(`list job jobid=${jobid}`),
      director.call(`list joblog jobid=${jobid}`),
    ])
    if (jobRes.status === 'fulfilled') {
      const raw = jobRes.value
      // "list job jobid=X" may return {jobs:[...]} or a direct job dict
      const entry = Array.isArray(raw?.jobs) ? raw.jobs[0] : raw
      jobData.value = entry ? normaliseJob(entry) : null
    }
    if (logRes.status === 'fulfilled') {
      const raw = logRes.value
      if (Array.isArray(raw?.joblog)) {
        logLines.value = raw.joblog.map(l => `${l.time ?? ''} ${l.logtext ?? ''}`.trim()).join('\n')
      } else if (typeof raw === 'string') {
        logLines.value = raw
      } else {
        logLines.value = JSON.stringify(raw, null, 2)
      }
    }
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
})

const job = computed(() => jobData.value)

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

const jobLog = computed(() => logLines.value)

function copyLog() {
  navigator.clipboard?.writeText(jobLog.value)
  $q.notify({ message: 'Log copied to clipboard', color: 'positive', icon: 'check', timeout: 1500 })
}
</script>
