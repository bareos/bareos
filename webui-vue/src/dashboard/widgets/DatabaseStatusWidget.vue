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
  <div class="column items-center justify-center db-chart-root" style="height:100%; overflow:hidden; padding:8px; box-sizing:border-box">
    <div v-if="loading && chartData.labels.length" class="db-refresh-indicator">
      <q-spinner size="14px" color="primary" />
    </div>
    <q-badge
      v-if="summary.status === 'warning' || summary.status === 'error'"
      rounded
      :color="summary.status === 'error' ? 'negative' : 'warning'"
      :label="summary.status"
      class="text-uppercase db-status-indicator"
    >
      <q-tooltip v-if="errorMessages.length">
        <div v-for="(msg, i) in errorMessages" :key="i">{{ msg }}</div>
      </q-tooltip>
    </q-badge>
    <q-spinner v-if="loading && !chartData.labels.length" size="40px" />
    <div v-else-if="!chartData.labels.length" class="text-grey text-caption text-center">
      {{ summary.status === 'error' || summary.status === 'unavailable'
        ? t('Database status unavailable')
        : t('No table size data available') }}
    </div>
    <template v-else>
      <div style="position:relative; width:100%; flex:1; min-height:0">
        <Pie :data="chartData" :options="chartOptions" />
      </div>
      <div class="q-mt-xs" style="font-size:0.72rem; color:#888; text-align:center">
        {{ t('Total') }}: {{ formatBytes(summary.totalBytes ?? 0) }}
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
import { formatBytes } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE } from '../piePalette.js'

ChartJS.register(ArcElement, Tooltip, Legend)

const { t } = useI18n()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const rows = computed(() => ctx.aggregate.value.databaseStatuses ?? [])
const summary = computed(() => ctx.aggregate.value.databaseStatusSummary ?? {
  status: 'unavailable',
  totalBytes: 0,
})
const loading = computed(() => ctx.loading.value)

const errorMessages = computed(() => rows.value
  .flatMap(row => row.errors ?? [])
  .filter(Boolean))

const tableRows = computed(() => {
  const merged = new Map()

  for (const row of rows.value) {
    for (const table of row.tables ?? []) {
      const name = String(table.name ?? '')
      const bytes = Number(table.bytes ?? 0)
      if (!name || !Number.isFinite(bytes) || bytes <= 0) {
        continue
      }
      merged.set(name, (merged.get(name) ?? 0) + bytes)
    }
  }

  return [...merged.entries()]
    .map(([name, bytes]) => ({ name, bytes }))
    .sort((a, b) => b.bytes - a.bytes)
})

const tableBytesTotal = computed(
  () => tableRows.value.reduce((sum, row) => sum + row.bytes, 0)
)

const reducedTableRows = computed(() => {
  const total = tableBytesTotal.value
  if (total <= 0) {
    return []
  }

  const minBytes = Math.max(10 * 1024 * 1024, total * 0.01)
  const kept = tableRows.value.filter(row => row.bytes >= minBytes)
  const visible = kept.length >= 3
    ? kept
    : tableRows.value

  const limited = visible.slice(0, 15).map(row => ({ ...row }))
  const hiddenBytes = visible
    .filter(row => !limited.some(entry => entry.name === row.name))
    .reduce((sum, row) => sum + row.bytes, 0)

  return hiddenBytes > 0
    ? [...limited, { name: 'Other', bytes: hiddenBytes }]
    : limited
})

const chartData = computed(() => ({
  labels: reducedTableRows.value.map(row => row.name),
  datasets: [{
    data: reducedTableRows.value.map(row => row.bytes),
    backgroundColor: reducedTableRows.value.map((_, i) => PIE_PALETTE[i % PIE_PALETTE.length]),
    borderWidth: 1,
  }],
}))

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
        label(context) {
          const bytes = Number(context.raw ?? 0)
          const total = tableBytesTotal.value
          const percent = total > 0 ? (bytes / total) * 100 : 0
          return ` ${context.label}: ${formatBytes(bytes)} (${percent.toFixed(1)}%)`
        },
      },
    },
  },
}))
</script>

<style scoped>
.db-chart-root {
  position: relative;
}

.db-refresh-indicator {
  position: absolute;
  top: 6px;
  right: 6px;
  z-index: 2;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.82);
  padding: 3px;
  line-height: 0;
}

.db-status-indicator {
  position: absolute;
  top: 6px;
  left: 6px;
  z-index: 2;
}
</style>
