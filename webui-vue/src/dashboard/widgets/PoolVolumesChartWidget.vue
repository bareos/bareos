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
  <div class="column items-center justify-center pool-chart-root" style="height:100%; overflow:hidden; padding:8px; box-sizing:border-box">
    <div v-if="loading && chartData.labels.length" class="pool-refresh-indicator">
      <q-spinner size="14px" color="primary" />
    </div>
    <q-spinner v-if="loading && !chartData.labels.length" size="40px" />
    <div v-else-if="!chartData.labels.length" class="text-grey text-caption text-center">
      {{ t('No pool data available') }}
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
import { Doughnut } from 'vue-chartjs'
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
} from 'chart.js'
import ChartDataLabels from 'chartjs-plugin-datalabels'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE, getContrastTextColor } from '../piePalette.js'
import { CenterTextPlugin } from '../centerTextPlugin.js'

ChartJS.register(ArcElement, Tooltip, Legend, CenterTextPlugin, ChartDataLabels)

const { t } = useI18n()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const pools = computed(() => ctx.pools?.value ?? ctx.aggregate.value.pools ?? [])
const loading = computed(() => ctx.poolLoading?.value ?? ctx.loading.value)

const totalVolumes = computed(() => pools.value.reduce((s, p) => s + (p.totalvolumes ?? 0), 0))

const chartData = computed(() => {
  const sorted = [...pools.value]
    .filter(p => (p.totalvolumes ?? 0) > 0)
    .sort((a, b) => (b.totalvolumes ?? 0) - (a.totalvolumes ?? 0))

  return {
    labels: sorted.map(p => p.name),
    datasets: [{
      data: sorted.map(p => p.totalvolumes ?? 0),
      backgroundColor: sorted.map((_, i) => PIE_PALETTE[i % PIE_PALETTE.length]),
      borderWidth: 1,
    }],
  }
})

const chartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  cutout: '65%',
  plugins: {
    legend: {
      position: 'bottom',
      labels: { font: { size: 11 }, boxWidth: 12 },
    },
    tooltip: {
      callbacks: {
        label(ctx) {
          return ` ${ctx.label}: ${ctx.raw} volume${ctx.raw !== 1 ? 's' : ''}`
        },
      },
    },
    centerText: {
      lines: [String(totalVolumes.value), t('volumes')],
      fonts: ['600 15px sans-serif', '11px sans-serif'],
      colors: ['#333', '#888'],
    },
    datalabels: {
      color: (context) => getContrastTextColor(
        PIE_PALETTE[context.dataIndex % PIE_PALETTE.length]
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
.pool-chart-root {
  position: relative;
}

.pool-refresh-indicator {
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
