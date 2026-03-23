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
import { mockJobs, formatBytes } from '../mock/index.js'

const totalBytes = mockJobs.reduce((a, j) => a + j.bytes, 0)
const totalFiles = mockJobs.reduce((a, j) => a + j.files, 0)

const overallStats = [
  { label: 'Total Jobs',   value: mockJobs.length },
  { label: 'Total Bytes',  value: formatBytes(totalBytes) },
  { label: 'Total Files',  value: totalFiles.toLocaleString() },
  { label: 'Clients',      value: 7 },
  { label: 'Successful',   value: mockJobs.filter(j => j.status === 'T').length },
  { label: 'Failed',       value: mockJobs.filter(j => j.status === 'f').length },
]
</script>
