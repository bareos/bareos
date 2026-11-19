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
  <div class="column items-center justify-center running-jobs-status-chart-root" style="height:100%; overflow:hidden; padding:8px; box-sizing:border-box">
    <div v-if="loading && chartData.labels.length" class="running-jobs-status-refresh-indicator">
      <q-spinner size="14px" color="primary" />
    </div>
    <q-spinner v-if="loading && !chartData.labels.length" size="40px" />
    <div v-else-if="!chartData.labels.length" class="text-grey text-caption text-center">
      {{ t('No running jobs') }}
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
import {
  groupRunningJobsByStatus,
  isWaitingStatus,
} from '../../utils/runningJobsGrouping.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE, getContrastTextColor } from '../piePalette.js'
import { CenterTextPlugin } from '../centerTextPlugin.js'

ChartJS.register(ArcElement, Tooltip, Legend, CenterTextPlugin, ChartDataLabels)

const RUNNING_BUCKET_COLOR = '#28a745'

const { t } = useI18n()
const router = useRouter()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const runningJobs = computed(() => ctx.aggregate.value.runningJobs ?? [])
const loading = computed(() => ctx.loading.value)
const statusBuckets = computed(() => groupRunningJobsByStatus(runningJobs.value))
const totalJobs = computed(() => runningJobs.value.length)
const sliceColors = computed(() => statusBuckets.value.map((bucket, index) => (
  isWaitingStatus(bucket.key)
    ? PIE_PALETTE[index % PIE_PALETTE.length]
    : RUNNING_BUCKET_COLOR
)))

const chartData = computed(() => ({
  labels: statusBuckets.value.map(bucket => bucket.key),
  datasets: [{
    data: statusBuckets.value.map(bucket => bucket.count),
    backgroundColor: sliceColors.value,
    borderWidth: 1,
  }],
}))

function handleBucketNavigation(index) {
  const bucket = statusBuckets.value[index]
  if (!bucket || isWaitingStatus(bucket.key)) {
    return
  }

  // Waiting reasons do not have a matching Jobs-page status filter; only the
  // active/non-waiting bucket maps safely to the existing "Running" filter.
  void router.push({
    name: 'jobs',
    query: withJobsStatusFilterQuery({}, 'R'),
  })
}

const chartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  cutout: '65%',
  onClick(_event, elements) {
    handleBucketNavigation(elements?.[0]?.index)
  },
  plugins: {
    legend: {
      position: 'bottom',
      labels: { font: { size: 11 }, boxWidth: 12 },
      onClick(_event, legendItem) {
        handleBucketNavigation(legendItem.index)
      },
    },
    tooltip: {
      callbacks: {
        label(context) {
          return ` ${context.label}: ${context.raw}`
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
        String(
          context.dataset.backgroundColor?.[context.dataIndex]
            ?? sliceColors.value[context.dataIndex]
            ?? PIE_PALETTE[context.dataIndex % PIE_PALETTE.length]
        )
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
.running-jobs-status-chart-root {
  position: relative;
}

.running-jobs-status-refresh-indicator {
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
