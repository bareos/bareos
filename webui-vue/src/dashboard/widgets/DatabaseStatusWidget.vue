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
  <div class="column full-height">
    <div class="row items-center q-gutter-sm q-px-sm q-pt-sm q-pb-xs">
      <q-badge
        rounded
        :color="statusColor(summary.status)"
        :label="summary.status || 'unavailable'"
        class="text-uppercase"
      />
      <span class="text-caption text-grey-6">
        {{ t('Directors') }}: {{ summary.directors }}
      </span>
      <span class="text-caption text-grey-6">
        {{ t('Total Size') }}: {{ formatBytes(summary.totalBytes ?? 0) }}
      </span>
    </div>

    <div v-if="!rows.length" class="col column items-center justify-center text-grey text-caption q-pa-md">
      {{ t('Database status unavailable') }}
    </div>

    <div v-else class="col column q-px-sm q-pb-sm" style="min-height:0; overflow:auto">
      <table class="db-status-table">
        <thead>
          <tr>
            <th>{{ t('Director') }}</th>
            <th>{{ t('Status') }}</th>
            <th>{{ t('Total Size') }}</th>
            <th>{{ t('Last Check') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="row in rows" :key="row.director">
            <td>{{ row.director }}</td>
            <td>
              <q-badge
                rounded
                :color="statusColor(row.status)"
                :label="row.status || 'unavailable'"
                class="text-uppercase"
              />
            </td>
            <td>
              <span v-if="row.database?.totalBytesAvailable">
                {{ formatBytes(row.database.totalBytes ?? 0) }}
              </span>
              <span v-else class="text-grey-6">—</span>
            </td>
            <td>
              <span :title="row.checkedAt || ''">
                {{ settings.relativeTime ? timeAgo(row.checkedAt, settings.locale) : (row.checkedAt || '—') }}
              </span>
            </td>
          </tr>
        </tbody>
      </table>

      <div v-if="tableChartData.labels.length" class="db-chart-wrap">
        <Pie :data="tableChartData" :options="tableChartOptions" />
      </div>
      <div v-else class="text-grey text-caption q-pb-sm q-pt-sm">
        {{ t('No table size data available') }}
      </div>
    </div>
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
import { formatBytes, timeAgo } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { PIE_PALETTE } from '../piePalette.js'

ChartJS.register(ArcElement, Tooltip, Legend)

const { t } = useI18n()
const settings = useSettingsStore()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const rows = computed(() => ctx.aggregate.value.databaseStatuses ?? [])
const summary = computed(() => ctx.aggregate.value.databaseStatusSummary ?? {
  status: 'unavailable',
  directors: 0,
  totalBytes: 0,
})

const tableChartRows = computed(() => {
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

const tableChartTotalBytes = computed(
  () => tableChartRows.value.reduce((sum, row) => sum + row.bytes, 0)
)

const reducedTableChartRows = computed(() => {
  const totalBytes = tableChartTotalBytes.value
  if (totalBytes <= 0) {
    return []
  }

  const minBytes = Math.max(10 * 1024 * 1024, totalBytes * 0.01)
  const kept = tableChartRows.value.filter(row => row.bytes >= minBytes)
  const visible = kept.length >= 3
    ? kept
    : tableChartRows.value

  const limited = visible.slice(0, 15).map(row => ({ ...row }))
  const hiddenBytes = visible
    .filter(row => !limited.some(entry => entry.name === row.name))
    .reduce((sum, row) => sum + row.bytes, 0)

  return hiddenBytes > 0
    ? [...limited, { name: 'Other', bytes: hiddenBytes }]
    : limited
})

const tableChartData = computed(() => ({
  labels: reducedTableChartRows.value.map(row => row.name),
  datasets: [{
    data: reducedTableChartRows.value.map(row => row.bytes),
    backgroundColor: reducedTableChartRows.value.map((_, i) => PIE_PALETTE[i % PIE_PALETTE.length]),
    borderWidth: 1,
  }],
}))

const tableChartOptions = computed(() => ({
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
          const total = tableChartTotalBytes.value
          const percent = total > 0 ? (bytes / total) * 100 : 0
          return ` ${context.label}: ${formatBytes(bytes)} (${percent.toFixed(1)}%)`
        },
      },
    },
  },
}))

function statusColor(status) {
  switch (String(status ?? '').toLowerCase()) {
    case 'ok': return 'positive'
    case 'warning': return 'warning'
    case 'error': return 'negative'
    default: return 'grey'
  }
}
</script>

<style scoped>
.db-status-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 0.82rem;
}

.db-chart-wrap {
  position: relative;
  width: 100%;
  min-height: 210px;
  height: 210px;
  margin-top: 0.5rem;
}

.db-status-table th,
.db-status-table td {
  padding: 0.35rem 0.45rem;
  border-bottom: 1px solid rgba(0, 0, 0, 0.08);
  text-align: left;
  white-space: nowrap;
}

.db-status-table th {
  color: #666;
  font-weight: 600;
  position: sticky;
  top: 0;
  background: var(--q-color-grey-1, #fafafa);
}
</style>
