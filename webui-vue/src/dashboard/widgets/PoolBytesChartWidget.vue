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
        <Pie :data="chartData" :options="chartOptions" />
      </div>
      <div class="q-mt-xs" style="font-size:0.72rem; color:#888; text-align:center">
        {{ t('Total') }}: {{ formatBytes(totalBytes) }}
      </div>
    </template>
  </div>
</template>

<script setup>
import { inject, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { Pie } from 'vue-chartjs'
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
} from 'chart.js'
import { useSettingsStore } from '../../stores/settings.js'
import { formatBytes } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE } from '../piePalette.js'

ChartJS.register(ArcElement, Tooltip, Legend)

const { t } = useI18n()
const settings = useSettingsStore()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const pools = computed(() => ctx.pools?.value ?? ctx.aggregate.value.pools ?? [])
const loading = computed(() => ctx.poolLoading?.value ?? ctx.loading.value)

const totalBytes = computed(() => pools.value.reduce((s, p) => s + (p.totalbytes ?? 0), 0))

const chartData = computed(() => {
  const sorted = [...pools.value]
    .filter(p => (p.totalbytes ?? 0) > 0)
    .sort((a, b) => (b.totalbytes ?? 0) - (a.totalbytes ?? 0))

  return {
    labels: sorted.map(p => p.name),
    datasets: [{
      data: sorted.map(p => p.totalbytes ?? 0),
      backgroundColor: sorted.map((_, i) => PIE_PALETTE[i % PIE_PALETTE.length]),
      borderWidth: 1,
    }],
  }
})

const chartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  plugins: {
    legend: {
      position: 'bottom',
      labels: { font: { size: 11 }, boxWidth: 12 },
    },
    tooltip: {
      callbacks: {
        label(ctx) {
          return ` ${ctx.label}: ${formatBytes(ctx.raw)}`
        },
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
