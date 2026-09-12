<!--
  BAREOS® - Backup Archiving REcovery Open Sourced

  Copyright (C) 2026 Bareos GmbH & Co. KG

  This program is Free Software; you can redistribute it and/or
  modify it under the terms of version three of the GNU Affero General Public
  License as published by the Free Software Foundation and included
  in the file LICENSE.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
-->
<template>
  <div class="column items-center justify-center job-status-chart-root" style="height:100%; overflow:hidden; padding:8px; box-sizing:border-box">
    <div v-if="loading && chartData.labels.length" class="job-status-refresh-indicator">
      <q-spinner size="14px" color="primary" />
    </div>
    <q-spinner v-if="loading && !chartData.labels.length" size="40px" />
    <div v-else-if="!chartData.labels.length" class="text-grey text-caption text-center">
      {{ t('No job data available') }}
    </div>
    <template v-else>
      <div style="position:relative; width:100%; flex:1; min-height:0">
        <Doughnut :data="chartData" :options="chartOptions" />
      </div>
    </template>
  </div>
</template>

<script setup>
import { inject, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRouter } from 'vue-router'
import { Doughnut } from 'vue-chartjs'
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
} from 'chart.js'
import ChartDataLabels from 'chartjs-plugin-datalabels'
import { withJobsStatusFilterQuery } from '../../utils/jobs.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { getContrastTextColor } from '../piePalette.js'
import { CenterTextPlugin } from '../centerTextPlugin.js'
import { buildJobStatus24hChartSegments } from '../jobStatus24hChart.js'

ChartJS.register(ArcElement, Tooltip, Legend, CenterTextPlugin, ChartDataLabels)

const { t } = useI18n()
const router = useRouter()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const past24hStatusCounts = computed(() => (
  ctx.aggregate.value.jobsPast24hStatusCounts ?? {}
))
const loading = computed(() => ctx.loading.value)
const chartSegments = computed(() => buildJobStatus24hChartSegments(
  past24hStatusCounts.value
).map(segment => ({
  ...segment,
  label: t(segment.labelKey),
})))
const totalJobs = computed(() => chartSegments.value.reduce(
  (sum, segment) => sum + segment.count,
  0
))

const chartData = computed(() => ({
  labels: chartSegments.value.map(segment => segment.label),
  datasets: [{
    data: chartSegments.value.map(segment => segment.count),
    backgroundColor: chartSegments.value.map(segment => segment.color),
    borderWidth: 1,
  }],
}))

function goToJobsStatus(status) {
  return router.push({
    name: 'jobs',
    query: withJobsStatusFilterQuery({}, status),
  })
}

function goToSegmentIndex(index) {
  const segment = chartSegments.value[index]
  if (!segment?.status) {
    return
  }

  void goToJobsStatus(segment.status)
}

const chartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  cutout: '65%',
  onClick: (_event, elements) => {
    if (!elements?.length) {
      return
    }

    goToSegmentIndex(elements[0].index)
  },
  onHover: (event, elements) => {
    if (event?.native?.target?.style) {
      event.native.target.style.cursor = elements?.length ? 'pointer' : 'default'
    }
  },
  plugins: {
    legend: {
      position: 'bottom',
      onClick: (_event, legendItem) => {
        goToSegmentIndex(legendItem.index)
      },
      labels: { font: { size: 11 }, boxWidth: 12 },
    },
    tooltip: {
      callbacks: {
        label(ctx) {
          return ` ${ctx.label}: ${ctx.raw}`
        },
      },
    },
    centerText: {
      lines: [String(totalJobs.value), t('Total')],
      fonts: ['600 15px sans-serif', '11px sans-serif'],
      colors: ['#333', '#888'],
    },
    datalabels: {
      color: (context) => getContrastTextColor(
        chartSegments.value[context.dataIndex]?.color ?? '#333333'
      ),
      font: { weight: 'bold', size: 11 },
      formatter: (value, context) => {
        const total = context.dataset.data.reduce((a, b) => a + b, 0)
        const percent = total > 0 ? (value / total) * 100 : 0
        return percent >= 5 ? `${percent.toFixed(0)}%` : ''
      },
    },
  },
}))
</script>

<style scoped>
.job-status-chart-root {
  position: relative;
}

.job-status-refresh-indicator {
  position: absolute;
  top: 6px;
  right: 6px;
  z-index: 2;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.82);
  padding: 3px;
  line-height: 0;
}
</style>
