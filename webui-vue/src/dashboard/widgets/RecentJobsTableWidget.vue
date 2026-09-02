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
  <div class="recent-jobs-widget" style="height:100%; width:100%">
    <q-banner v-if="error" dense class="bg-negative text-white">
      {{ error }}
    </q-banner>
    <q-banner v-if="truncated" dense class="bg-warning text-white">
      {{ t('Showing the first {limit} of {total} matching jobs. Narrow your filter to see the rest.', { limit: formatNumber(maxJobsFetchLimit), total: formatNumber(totalJobs) }) }}
    </q-banner>
    <q-table
      :rows="recentJobs"
      :columns="recentCols"
      row-key="scopeKey"
      dense flat
      :loading="loading"
      style="height:100%; width:100%"
      virtual-scroll
      :rows-per-page-options="recentJobsRowsPerPageOptions"
      v-model:pagination="pagination"
      @request="onRequest"
    >
      <template #body-cell-id="props">
        <q-td :props="props" class="text-right">
          <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
            {{ props.value }}
          </a>
        </q-td>
      </template>
      <template #body-cell-director="props">
        <q-td :props="props">
          <div class="row items-center q-gutter-sm no-wrap">
            <span :style="directorSwatchStyle(props.row.director || props.value || '', directorOptions)" />
            <span>{{ props.row.director || props.value || '—' }}</span>
          </div>
        </q-td>
      </template>
      <template #body-cell-status="props">
        <q-td :props="props">
          <span
            v-if="isWaitingStatus(jobStatus(props.row))"
            class="row items-center no-wrap q-gutter-x-xs cursor-pointer"
            :title="t('Jump to log')"
            @click="openJobDetails(props.row, resolveJobLogFocus(props.row.status))"
          >
            <q-icon name="hourglass_empty" color="orange-7" size="16px" class="animated-spin" />
            <span class="text-orange-7 text-caption">{{ jobStatus(props.row) }}</span>
          </span>
          <JobStatusBadge
            v-else
            clickable
            :status="jobStatus(props.row)"
            @click="openJobDetails(props.row, resolveJobLogFocus(props.row.status))"
          />
        </q-td>
      </template>
      <template #body-cell-client="props">
        <q-td :props="props">
          <a href="#" class="text-primary" @click.prevent="openClientDetails(props.row)">
            {{ props.value }}
          </a>
        </q-td>
      </template>
      <template #body-cell-starttime="props">
        <q-td :props="props">
          <span :title="settings.relativeTime ? props.value : timeAgo(props.value, settings.locale)">
            {{ settings.relativeTime ? timeAgo(props.value, settings.locale) : props.value }}
          </span>
        </q-td>
      </template>
      <template #body-cell-name="props">
        <q-td :props="props">
          <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
            {{ props.value }}
          </a>
        </q-td>
      </template>
      <template #body-cell-level="props">
        <q-td :props="props" class="text-center">
          <JobLevelBadge v-if="props.value" :level="props.value" />
          <span v-else>—</span>
        </q-td>
      </template>
      <template #body-cell-bytes="props">
        <q-td :props="props" class="text-right" style="min-width:90px">
          <div>{{ formatBytes(props.row.bytes ?? 0) }}</div>
          <q-linear-progress
            v-if="isRunningJob(props.row)"
            indeterminate color="primary" track-color="grey-3"
            size="4px" class="q-mt-xs" rounded
          />
          <q-linear-progress
            v-else
            :value="bytesGauge(props.row.bytes)"
            color="primary" track-color="grey-3"
            size="4px" class="q-mt-xs" rounded
          />
        </q-td>
      </template>
      <template #body-cell-duration="props">
        <q-td :props="props" class="text-right" style="min-width:80px">
          <div>{{ props.value || '—' }}</div>
          <q-linear-progress
            :value="durationGauge(props.value)"
            color="orange" track-color="grey-3"
            size="4px" class="q-mt-xs" rounded
          />
        </q-td>
      </template>
      <template #body-cell-speed="props">
        <q-td :props="props" class="text-right" style="min-width:80px">
          <span v-if="isRunningJob(props.row)" class="text-grey-5">—</span>
          <template v-else>
            <div>{{ fmtSpeed(props.row.bytes, props.row.duration) }}</div>
            <q-linear-progress
              :value="speedGauge(props.row)"
              color="cyan-7" track-color="grey-3"
              size="4px" class="q-mt-xs" rounded
            />
          </template>
        </q-td>
      </template>
    </q-table>
  </div>
</template>

<script setup>
import { inject, computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { formatBytes, formatSpeed, parseDurationSecs, timeAgo } from '../../mock/index.js'
import { buildJobDetailsQuery, withJobsStatusFilterQuery, resolveJobLogFocus } from '../../utils/jobs.js'
import { formatNumber } from '../../utils/locales.js'
import { buildClientDetailsQuery } from '../../utils/clients.js'
import { resolveDirectorColors } from '../../utils/directorColors.js'
import { useSettingsStore } from '../../stores/settings.js'
import { useAuthStore } from '../../stores/auth.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { usePersistedTablePagination } from '../../composables/usePersistedTablePagination.js'
import {
  fetchAggregatedRecentJobsPage,
  MAX_JOBS_FETCH_LIMIT,
} from '../../composables/jobsAggregate.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import JobStatusBadge from '../../components/JobStatusBadge.vue'
import JobLevelBadge from '../../components/JobLevelBadge.vue'

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()
const settings = useSettingsStore()
const auth = useAuthStore()

const ctx = inject(DASHBOARD_CONTEXT_KEY)
const recentJobsLoading = ref(false)
const loading = computed(() => ctx.loading.value || recentJobsLoading.value)
const recentJobs = ref([])
const totalJobs = ref(0)
const truncated = ref(false)
const error = ref('')
const directorOptions = computed(() => ctx.directorOptions.value)
const recentJobsRowsPerPageOptions = [10, 25, 50]
const maxJobsFetchLimit = MAX_JOBS_FETCH_LIMIT

const fmtSpeed = formatSpeed

const pagination = usePersistedTablePagination('dashboard.recentJobs', {
  rowsPerPage: 10,
  sortBy: 'id',
  descending: true,
}, {
  allowedRowsPerPage: recentJobsRowsPerPageOptions,
})

const showDirectorColumn = computed(() => directorOptions.value.length > 1)

const recentCols = computed(() => {
  const columns = [
    { name: 'id',        label: 'ID',         field: 'id',        align: 'right', sortable: true },
    { name: 'name',      label: t('Job Name'), field: 'name',      align: 'left',  sortable: true },
    { name: 'client',    label: t('Client'),   field: 'client',    align: 'left',  sortable: true },
    { name: 'level',     label: t('Level'),    field: 'level',     align: 'center',sortable: true },
    { name: 'status',    label: t('Status'),   field: 'status',    align: 'center',sortable: true },
    { name: 'starttime', label: t('Start'),    field: 'starttime', align: 'left',  sortable: true },
    {
      name: 'duration',
      label: t('Duration'),
      field: 'duration',
      align: 'right',
      sortable: true,
      sort: (a, b) => parseDurationSecs(a) - parseDurationSecs(b),
    },
    { name: 'bytes', label: t('Bytes'), field: 'bytes', align: 'right', sortable: true },
    {
      name: 'speed',
      label: t('Speed'),
      field: 'speed',
      align: 'right',
      sortable: true,
      sort: (_a, _b, rowA, rowB) => jobSpeedBps(rowA) - jobSpeedBps(rowB),
    },
  ]

  if (showDirectorColumn.value) {
    columns.splice(1, 0, {
      name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
    })
  }

  return columns
})

const maxBytes = computed(() => recentJobs.value.reduce(
  (max, job) => Math.max(max, Number(job.bytes) || 0), 1
))
const maxDurationSecs = computed(() => recentJobs.value.reduce(
  (max, job) => Math.max(max, parseDurationSecs(job.duration)), 1
))

function jobSpeedBps(row) {
  const secs = parseDurationSecs(row.duration)
  if (!secs) return 0
  const bytes = typeof row.bytes === 'string' ? parseFloat(row.bytes) : (row.bytes || 0)
  return bytes / secs
}

const maxSpeedBps = computed(() => recentJobs.value.reduce(
  (max, job) => isRunningJob(job) ? max : Math.max(max, jobSpeedBps(job)), 1
))

function bytesGauge(val)  { return val / maxBytes.value }
function durationGauge(str) { return parseDurationSecs(str) / maxDurationSecs.value }
function speedGauge(row)  { return jobSpeedBps(row) / maxSpeedBps.value }

let latestRequestId = 0

async function fetchRecentJobs() {
  const requestId = ++latestRequestId
  const credentials = auth.getCredentials()
  const directors = ctx.activeDirectors.value
  if (!credentials || directors.length === 0) {
    recentJobs.value = []
    totalJobs.value = 0
    truncated.value = false
    pagination.value = { ...pagination.value, rowsNumber: 0 }
    return
  }

  recentJobsLoading.value = true
  error.value = ''
  try {
    const result = await fetchAggregatedRecentJobsPage(
      credentials,
      directors,
      pagination.value
    )
    if (requestId !== latestRequestId) return

    recentJobs.value = result.jobs
    totalJobs.value = result.totalJobs
    truncated.value = result.truncated
    pagination.value = { ...pagination.value, rowsNumber: result.totalJobs }
    error.value = result.directorErrors.map(entry => entry.message).join(' ')
  } catch (fetchError) {
    if (requestId !== latestRequestId) return
    recentJobs.value = []
    totalJobs.value = 0
    truncated.value = false
    pagination.value = { ...pagination.value, rowsNumber: 0 }
    error.value = fetchError.message
  } finally {
    if (requestId === latestRequestId) {
      recentJobsLoading.value = false
    }
  }
}

function onRequest(props) {
  pagination.value = { ...pagination.value, ...props.pagination }
}

watch(
  () => [
    ctx.refreshToken.value,
    ctx.activeDirectors.value.join('\0'),
    pagination.value.page,
    pagination.value.rowsPerPage,
    pagination.value.sortBy,
    pagination.value.descending,
  ],
  fetchRecentJobs,
  { immediate: true }
)

function jobStatus(row) { return row.runtimeStatus ?? row.status ?? '?' }
function isWaitingStatus(status) {
  return typeof status === 'string' && status.toLowerCase().includes('is waiting')
}
function isRunningJob(row) { return row?.status === 'R' || row?.runtimeStatus != null }

function directorSwatchStyle(name, options) {
  const colors = resolveDirectorColors(name, options.map(o => o.value))
  return {
    display: 'inline-block',
    width: '10px',
    height: '10px',
    borderRadius: '999px',
    backgroundColor: colors.background,
    border: `1px solid ${colors.border}`,
    flexShrink: 0,
  }
}

async function openJobDetails(row, logFocus) {
  try {
    await switchActiveDirector(row.director)
    await router.push({
      name: 'job-details',
      params: { id: row.id ?? row.jobid },
      query: buildJobDetailsQuery({ director: row.director, dashboardOrigin: true, logFocus }),
    })
  } catch (error) {
    $q.notify({ type: 'negative', message: error.message })
  }
}

async function openClientDetails(row) {
  try {
    await switchActiveDirector(row.director)
    await router.push({
      name: 'client-details',
      params: { name: row.client },
      query: buildClientDetailsQuery({ director: row.director, dashboardOrigin: true }),
    })
  } catch (error) {
    $q.notify({ type: 'negative', message: error.message })
  }
}
</script>

<style scoped>
.animated-spin {
  animation: hourglass-spin 1.4s ease-in-out infinite;
}
@keyframes hourglass-spin {
  0%   { transform: rotate(0deg); }
  45%  { transform: rotate(0deg); }
  55%  { transform: rotate(180deg); }
  100% { transform: rotate(180deg); }
}
</style>

<style>
.recent-jobs-widget .q-table__middle {
  overflow-x: auto !important;
}
.recent-jobs-widget table {
  table-layout: auto !important;
  width: max-content !important;
  min-width: 100%;
}
.recent-jobs-widget th,
.recent-jobs-widget td {
  white-space: nowrap !important;
}
</style>
