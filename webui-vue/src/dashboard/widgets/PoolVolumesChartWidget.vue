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
  <div class="column items-center justify-center" style="height:100%; overflow:hidden; padding:8px; box-sizing:border-box">
    <q-spinner v-if="loading" size="40px" />
    <div v-else-if="error" class="text-negative text-caption text-center">{{ error }}</div>
    <div v-else-if="!chartData.labels.length" class="text-grey text-caption text-center">
      {{ t('No pool data available') }}
    </div>
    <template v-else>
      <div style="position:relative; width:100%; flex:1; min-height:0">
        <Pie :data="chartData" :options="chartOptions" />
      </div>
      <div class="q-mt-xs" style="font-size:0.72rem; color:#888; text-align:center">
        {{ t('Total') }}: {{ totalVolumes }} {{ t('volumes') }}
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
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE } from '../piePalette.js'

ChartJS.register(ArcElement, Tooltip, Legend)

const { t } = useI18n()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const pools = computed(() => ctx.aggregate.value.pools ?? [])
const loading = computed(() => ctx.loading.value)

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
  },
}))
</script>
