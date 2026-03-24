<template>
  <q-page class="q-pa-md">
    <div class="row q-col-gutter-md">
      <!-- Left: summary -->
      <div class="col-12 col-md-3">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Overall</q-card-section>
          <q-card-section>
            <q-list dense>
              <q-item v-for="s in overallStats" :key="s.label">
                <q-item-section>
                  <q-item-label caption>{{ s.label }}</q-item-label>
                  <q-item-label class="text-h6">{{ s.value }}</q-item-label>
                </q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
      <!-- Right: charts placeholder -->
      <div class="col-12 col-md-9">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">Stored Data per Client (last 30 days)</q-card-section>
          <q-card-section style="height:260px" class="flex flex-center text-grey column">
            <q-icon name="bar_chart" size="64px" class="q-mb-sm" />
            <div>Bar chart — requires Chart.js integration</div>
            <div class="text-caption q-mt-xs">Showing bytes stored per client over time</div>
          </q-card-section>
        </q-card>
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Job Success Rate</q-card-section>
          <q-card-section style="height:220px" class="flex flex-center text-grey column">
            <q-icon name="pie_chart" size="64px" class="q-mb-sm" />
            <div>Pie chart — requires Chart.js integration</div>
          </q-card-section>
        </q-card>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { computed } from 'vue'
import { formatBytes } from '../mock/index.js'
import { useDirectorFetch, normaliseJob } from '../composables/useDirectorFetch.js'

const { data: rawJobs } = useDirectorFetch('list jobs', 'jobs')
const { data: rawClients } = useDirectorFetch('list clients', 'clients')
const jobs = computed(() => (rawJobs.value ?? []).map(normaliseJob))

const overallStats = computed(() => [
  { label: 'Total Jobs',   value: jobs.value.length },
  { label: 'Total Bytes',  value: formatBytes(jobs.value.reduce((a, j) => a + j.bytes, 0)) },
  { label: 'Total Files',  value: jobs.value.reduce((a, j) => a + j.files, 0).toLocaleString() },
  { label: 'Clients',      value: (rawClients.value ?? []).length },
  { label: 'Successful',   value: jobs.value.filter(j => j.status === 'T').length },
  { label: 'Failed',       value: jobs.value.filter(j => j.status === 'f' || j.status === 'E').length },
])
</script>
