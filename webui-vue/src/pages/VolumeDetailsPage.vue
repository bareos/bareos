<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
  <q-page class="q-pa-md">
    <!-- Breadcrumbs -->
    <Breadcrumbs :items="breadcrumbItems" />

    <!-- Loading / error -->
    <q-inner-loading :showing="loading" :label="t('Loading volume…')" />
    <q-banner v-if="error" dense rounded class="bg-negative text-white q-mb-md">{{ error }}</q-banner>

    <template v-else-if="vol">
      <!-- ── Header ────────────────────────────────────────────────────── -->
      <div class="row items-center q-mb-md q-gutter-sm">
        <q-icon name="mdi-tape-drive" size="2rem" color="primary" />
        <div>
          <div class="row items-center q-gutter-xs">
            <div class="text-h5">{{ vol.volumename }}</div>
            <q-icon
              v-if="hasEncryptionKey"
              name="vpn_key"
              color="amber-8"
              role="img"
              :aria-label="t('Encryption key stored in catalog')"
            >
              <q-tooltip>{{ t('Encryption key stored in catalog') }}</q-tooltip>
            </q-icon>
          </div>
          <div class="row q-gutter-xs q-mt-xs">
            <VolumeStatusBadge :status="vol.volstatus" />
            <q-badge :color="vol.enabled !== '0' && vol.enabled !== false ? 'positive' : 'grey'">
               {{ vol.enabled !== '0' && vol.enabled !== false ? t('Enabled') : t('Disabled') }}
            </q-badge>
            <q-badge v-if="vol.inchanger === '1' || vol.inchanger === true"
                     color="teal" :label="t('In Changer')" />
          </div>
        </div>
        <q-space />
      </div>

      <!-- ── Two-column layout ─────────────────────────────────────────── -->
      <div class="row q-col-gutter-md">

        <!-- Left: details -->
        <div class="col-12 col-md-5">
          <q-card flat bordered class="bareos-panel q-mb-md">
            <q-card-section class="panel-header">{{ t('Volume Properties') }}</q-card-section>
            <q-card-section class="q-pa-none">
              <q-list dense separator>
                <q-item v-for="row in detailRows" :key="row.label">
                  <q-item-section class="text-grey-6" style="max-width:160px">
                    {{ row.label }}
                  </q-item-section>
                  <q-item-section>
                    <router-link v-if="row.link" :to="row.link" class="text-primary">
                      {{ row.value }}
                    </router-link>
                    <component :is="row.component ?? 'span'" v-else-if="row.component"
                               v-bind="row.componentProps" />
                    <template v-else>{{ row.value }}</template>
                  </q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>

          <!-- Gauges card -->
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Usage') }}</q-card-section>
            <q-card-section>
              <!-- Used bytes gauge -->
              <div class="q-mb-md">
                <div class="row justify-between text-caption text-grey-6 q-mb-xs">
                  <span>{{ t('Used Bytes') }}</span>
                  <span>{{ formatBytes(vol.volbytes) }}
                    <template v-if="Number(vol.maxvolbytes) > 0">
                      / {{ formatBytes(vol.maxvolbytes) }}
                    </template>
                  </span>
                </div>
                <q-linear-progress
                  :value="bytesRatio"
                  :color="bytesRatio > 0.9 ? 'negative' : bytesRatio > 0.7 ? 'warning' : 'primary'"
                  track-color="grey-3" size="10px" rounded
                />
              </div>

              <!-- Retention gauge -->
              <div class="q-mb-md">
                <div class="row justify-between text-caption text-grey-6 q-mb-xs">
                  <span>{{ t('Retention') }}</span>
                  <span>{{ formatDuration(vol.volretention) }}</span>
                </div>
                <q-linear-progress
                  :value="retentionGauge"
                  color="orange" track-color="grey-3"
                  size="10px" rounded
                />
              </div>

              <!-- Vol jobs / max jobs -->
              <div v-if="Number(vol.maxvoljobs) > 0">
                <div class="row justify-between text-caption text-grey-6 q-mb-xs">
                  <span>{{ t('Jobs on Volume') }}</span>
                  <span>{{ vol.voljobs }} / {{ vol.maxvoljobs }}</span>
                </div>
                <q-linear-progress
                  :value="Number(vol.voljobs) / Number(vol.maxvoljobs)"
                  color="deep-purple" track-color="grey-3"
                  size="10px" rounded
                />
              </div>

              <!-- Volume tape contents -->
              <div v-if="volumeTapeSegments.length" class="q-mt-md">
                <div class="row justify-between text-caption text-grey-6 q-mb-xs">
                  <span>{{ t('Tape contents') }}</span>
                  <span>{{ formatRangeCount(volumeTapeSegments.length) }}</span>
                </div>
                <div
                  v-if="volumeTapeHasOverlaps"
                  class="text-caption text-grey-6 q-mb-xs"
                >
                  {{ t('Overlapping JobMedia ranges detected. Each lane is a seek range; other jobs may have blocks inside that range.') }}
                </div>
                <div v-if="volumeTapeHasOverlaps" class="volume-tape-lanes">
                  <div
                    v-for="seg in volumeTapeSegments"
                    :key="seg.key"
                    class="volume-tape-lane-row"
                  >
                    <router-link
                      :to="{
                        name: 'job-details',
                        params: { id: seg.jobid },
                        query: buildVolumeJobDetailsQuery(currentVolumeDirector),
                      }"
                      class="volume-tape-lane-label text-primary text-caption"
                    >#{{ seg.jobid }}</router-link>
                    <div class="volume-tape-lane bg-grey-4">
                      <div
                        class="volume-tape-range"
                        :style="{
                          left: seg.leftPct + '%',
                          width: seg.widthPct + '%',
                          background: segmentColor(seg.colorIndex),
                        }"
                      >
                        <q-tooltip>
                          <div class="text-caption text-weight-medium">
                            #{{ seg.jobid }} {{ seg.name || t('Unknown Job') }}
                          </div>
                          <div class="text-caption">{{ t('FileIndex') }}: {{ seg.fileIndexLabel }}</div>
                          <div class="text-caption">{{ t('Media') }}: {{ seg.mediaPositionLabel }}</div>
                          <div class="text-caption">{{ t('Bytes') }}: {{ formatBytes(seg.jobbytes) }}</div>
                        </q-tooltip>
                      </div>
                    </div>
                  </div>
                </div>
                <div v-else class="volume-tape-bar bg-grey-4">
                  <div
                    v-for="seg in volumeTapeSegments"
                    :key="seg.key"
                    class="volume-tape-segment"
                    :style="{
                      width: seg.pct + '%',
                      background: segmentColor(seg.colorIndex),
                    }"
                  >
                    <q-tooltip>
                      <div class="text-caption text-weight-medium">
                        #{{ seg.jobid }} {{ seg.name || t('Unknown Job') }}
                      </div>
                      <div class="text-caption">{{ t('FileIndex') }}: {{ seg.fileIndexLabel }}</div>
                      <div class="text-caption">{{ t('Media') }}: {{ seg.mediaPositionLabel }}</div>
                      <div class="text-caption">{{ t('Bytes') }}: {{ formatBytes(seg.jobbytes) }}</div>
                    </q-tooltip>
                  </div>
                </div>

                <div class="q-mt-sm">
                  <div
                    v-for="seg in volumeTapeSegments.slice(0, 8)"
                    :key="seg.key"
                    class="row items-center q-mb-xs text-caption"
                  >
                    <div
                      :style="{ background: segmentColor(seg.colorIndex), width: '10px', height: '10px', borderRadius: '2px', flexShrink: 0 }"
                      class="q-mr-sm"
                    />
                    <router-link
                      :to="{
                        name: 'job-details',
                        params: { id: seg.jobid },
                        query: buildVolumeJobDetailsQuery(currentVolumeDirector),
                      }"
                      class="text-primary q-mr-xs"
                      style="white-space:nowrap"
                    >#{{ seg.jobid }}</router-link>
                    <span class="text-grey-7 ellipsis">{{ seg.name }}</span>
                    <q-space />
                    <span class="text-grey-6 q-ml-sm" style="white-space:nowrap">
                      {{ t('FileIndex') }} {{ seg.fileIndexLabel }}
                    </span>
                  </div>
                  <div v-if="volumeTapeSegments.length > 8" class="text-caption text-grey-5">
                     + {{ formatMoreRangesCount(volumeTapeSegments.length - 8) }}
                  </div>
                </div>
              </div>
            </q-card-section>
          </q-card>
        </div>

        <!-- Right: jobs that used this volume -->
        <div class="col-12 col-md-7">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header row items-center">
               <span>{{ t('Jobs using this Volume') }} ({{ jobs.length }})</span>
              <q-space />
              <q-spinner v-if="jobsLoading" size="18px" />
            </q-card-section>
            <q-card-section class="q-pa-none">
              <q-table :rows="jobs" :columns="jobCols" row-key="jobid"
                       dense flat v-model:pagination="jobsPagination"
                       :rows-per-page-options="jobsRowsPerPageOptions">
                <template #body-cell-jobid="props">
                  <q-td :props="props">
                    <router-link
                      :to="{
                        name: 'job-details',
                        params: { id: props.value },
                        query: buildVolumeJobDetailsQuery(props.row.director),
                      }"
                      class="text-primary"
                    >
                      {{ props.value }}
                    </router-link>
                  </q-td>
                </template>
                <template #body-cell-jobstatus="props">
                  <q-td :props="props" class="text-center">
                    <JobStatusBadge :status="props.value" />
                  </q-td>
                </template>
                <template #body-cell-jobbytes="props">
                  <q-td :props="props" class="text-right">
                    {{ formatBytes(props.value) }}
                  </q-td>
                </template>
              </q-table>
            </q-card-section>
          </q-card>
        </div>
      </div>
    </template>
  </q-page>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  usePersistedTablePagination,
  UNBOUNDED_TABLE_ROWS_PER_PAGE,
} from '../composables/usePersistedTablePagination.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { buildDirectorPageQuery } from '../utils/director.js'
import { buildJobDetailsQuery, resolveJobDetailsQuery } from '../utils/jobs.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import { formatNumber } from '../utils/locales.js'
import { buildPoolDetailsQuery } from '../utils/pools.js'
import {
  buildAutochangerSelectionQuery,
  buildStoragesTabQuery,
  resolveAutochangerSelectionQuery,
  withStoragesScopeDirectorQuery,
} from '../utils/storagesRoute.js'
import {
  buildVolumeTapeSegments,
  resolveVolumeDetailsDirectorOrigin,
  resolveVolumeDetailsJobOrigin,
  resolveVolumeDetailsPoolOrigin,
  resolveVolumeDetailsStoragesOrigin,
  volumeHasEncryptionKey,
  volumeUsageSegmentsFromResponse,
} from '../utils/volumes.js'
import Breadcrumbs from '../components/Breadcrumbs.vue'
import VolumeStatusBadge from '../components/VolumeStatusBadge.vue'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route      = useRoute()
const auth       = useAuthStore()
const director   = useDirectorStore()
const settings   = useSettingsStore()
const { t } = useI18n()
const jobsPagination = usePersistedTablePagination('volume-details.jobs', {
  rowsPerPage: 10,
  sortBy: 'jobid',
  descending: true,
}, { allowedRowsPerPage: UNBOUNDED_TABLE_ROWS_PER_PAGE })
const jobsRowsPerPageOptions = UNBOUNDED_TABLE_ROWS_PER_PAGE
const volumeName = computed(() => route.params.name)
const requestedDirector = computed(() => (
  typeof route.query.director === 'string' ? route.query.director : ''
))
const currentVolumeDirector = computed(() => (
  requestedDirector.value || auth.user?.director || settings.directorName || ''
))
const autochangerOrigin = computed(() => resolveAutochangerSelectionQuery(route.query))
const jobOrigin = computed(() => resolveVolumeDetailsJobOrigin(route.query))
const poolOrigin = computed(() => resolveVolumeDetailsPoolOrigin(route.query))
const storagesOrigin = computed(() => resolveVolumeDetailsStoragesOrigin(route.query))
const directorOrigin = computed(() => resolveVolumeDetailsDirectorOrigin(route.query))
const backLabel = computed(() => {
  if (jobOrigin.value) {
    return t('Job')
  }

  if (poolOrigin.value) {
    return t('Pool')
  }

  if (autochangerOrigin.value) {
    return t('Autochanger')
  }

  if (storagesOrigin.value) {
    return t('Storages')
  }

  return directorOrigin.value ? t('Director') : t('Volumes')
})
const backLocation = computed(() => {
  if (jobOrigin.value) {
    return {
      name: 'job-details',
      params: { id: jobOrigin.value.id },
      query: resolveJobDetailsQuery(route.query),
    }
  }

  if (poolOrigin.value) {
    return {
      name: 'pool-details',
      params: { name: poolOrigin.value.name },
      query: buildPoolDetailsQuery({
        director: requestedDirector.value,
        poolName: poolOrigin.value.name,
        volumeQuery: route.query,
      }),
    }
  }

  if (autochangerOrigin.value) {
    return {
      name: 'storages',
      query: buildAutochangerSelectionQuery({}, autochangerOrigin.value),
    }
  }

  if (storagesOrigin.value) {
    return {
      name: 'storages',
      query: withStoragesScopeDirectorQuery(
        buildStoragesTabQuery({}, storagesOrigin.value.tab),
        storagesOrigin.value.scopeDirector
      ),
    }
  }

  if (directorOrigin.value) {
    return {
      name: 'director',
      query: buildDirectorPageQuery({}, {
        tab: directorOrigin.value.tab,
        targetDirector: directorOrigin.value.targetDirector,
      }),
    }
  }

  return {
    name: 'storages',
    query: { tab: 'volumes' },
  }
})
const breadcrumbItems = computed(() => [
  { label: backLabel.value, icon: 'arrow_back', to: backLocation.value },
  { label: vol.value?.volumename ?? volumeName.value },
])

function buildVolumeJobDetailsQuery(jobDirector) {
  return buildJobDetailsQuery({
    director: jobDirector,
    volumeName: volumeName.value,
    volumeDirector: currentVolumeDirector.value,
  })
}

const vol         = ref(null)
const jobs        = ref([])
const volumeUsage = ref([])
const loading     = ref(true)
const jobsLoading = ref(false)
const error       = ref(null)
function formatRangeCount(count) {
  return `${formatNumber(count, settings.locale)} ${t('range(s)')}`
}

function formatMoreRangesCount(count) {
  return `${formatNumber(count, settings.locale)} ${t('more ranges')}`
}

// Retention reference scale: 1 year in seconds
const ONE_YEAR_S = 365 * 24 * 3600

const RETENTION_STEPS = [
  7 * 24 * 3600,       // 1 week
  30 * 24 * 3600,      // 1 month
  90 * 24 * 3600,      // 3 months
  365 * 24 * 3600,     // 1 year
  3 * 365 * 24 * 3600, // 3 years
]

async function ensureVolumeDirector() {
  if (!requestedDirector.value) {
    return
  }

  if (auth.user?.director === requestedDirector.value && director.isConnected) {
    return
  }

  await switchActiveDirector(requestedDirector.value)
}

async function loadVolume() {
  await ensureVolumeDirector()

  const [volRes, jobRes, usageRes] = await Promise.all([
    director.call(`llist volume=${quoteDirectorString(volumeName.value)}`),
    fetchJobs(),
    director.call(`llist volumeusage volume=${quoteDirectorString(volumeName.value)}`),
  ])
  const raw = volRes?.volumes ?? volRes?.volume ?? null
  if (Array.isArray(raw)) {
    vol.value = raw[0] ?? null
  } else {
    vol.value = raw
  }
  if (vol.value) {
    vol.value = {
      ...vol.value,
      director: currentVolumeDirector.value || null,
    }
  }
  jobs.value = jobRes
  volumeUsage.value = volumeUsageSegmentsFromResponse(usageRes)
}

watch(() => `${volumeName.value}\u0000${requestedDirector.value}`, async () => {
  loading.value = true
  error.value = null
  vol.value = null
  jobs.value = []
  volumeUsage.value = []
  try {
    await loadVolume()
  } catch (loadError) {
    error.value = loadError.message
  } finally {
    loading.value = false
  }
}, { immediate: true })

async function fetchJobs() {
  jobsLoading.value = true
  try {
    const res = await director.call(`llist jobs volume=${quoteDirectorString(volumeName.value)}`)
    const raw = res?.jobs ?? []
    const rows = Array.isArray(raw) ? raw : Object.values(raw).flat()
    return rows.map(job => ({
      ...job,
      director: currentVolumeDirector.value || null,
    }))
  } catch {
    return []
  } finally {
    jobsLoading.value = false
  }
}

// ── Computed ──────────────────────────────────────────────────────────────────

const bytesRatio = computed(() => {
  if (!vol.value) return 0
  const used = Number(vol.value.volbytes) || 0
  const max  = Number(vol.value.maxvolbytes)
  if (max > 0) return Math.min(1, used / max)
  // No cap: show relative to 1 TiB as 100%
  return Math.min(1, used / (1024 ** 4))
})

const retentionGauge = computed(() => {
  if (!vol.value) return 0
  const secs = Number(vol.value.volretention) || 0
  // Find smallest reference step larger than this value, or use 3 years
  const ref = RETENTION_STEPS.find(s => s >= secs) ?? RETENTION_STEPS[RETENTION_STEPS.length - 1]
  return Math.min(1, secs / ref)
})

const hasEncryptionKey = computed(() => volumeHasEncryptionKey(vol.value))

const detailRows = computed(() => {
  if (!vol.value) return []
  const v = vol.value
  return [
    { label: t('Media ID'), value: v.mediaid ?? v.mediaId ?? '—' },
    {
      label: t('Pool'),
      value: v.pool ?? v.Pool ?? '—',
      link: v.pool
        ? {
          name: 'pool-details',
          params: { name: v.pool },
          query: buildPoolDetailsQuery({
            director: currentVolumeDirector.value,
            volumeName: volumeName.value,
            volumeQuery: route.query,
          }),
        }
        : null,
    },
    { label: t('Storage'),     value: v.storage ?? v.storagename ?? '—' },
    { label: t('Media Type'),  value: v.mediatype ?? v.MediaType ?? '—' },
    { label: t('Encryption Key'), value: hasEncryptionKey.value ? t('Present') : '—' },
    { label: t('Slot'),        value: v.slot ?? '0' },
    { label: t('Label Date'),  value: v.labeldate ?? '—' },
    { label: t('First Written'), value: v.firstwritten ?? '—' },
    { label: t('Last Written'),  value: v.lastwritten ?? '—' },
    { label: t('Jobs on Vol'), value: `${v.voljobs ?? 0}${Number(v.maxvoljobs) > 0 ? ` / ${v.maxvoljobs}` : ''}` },
    { label: t('Files on Vol'), value: v.volfiles ?? '0' },
    { label: t('Blocks on Vol'), value: v.volblocks ?? '0' },
    { label: t('Vol Writes'), value: v.volwrites ?? '0' },
    { label: t('Vol Reads'), value: v.volreads ?? '0' },
    { label: t('Read Time'), value: v.volreadtime ? formatDuration(v.volreadtime) : '—' },
    { label: t('Write Time'), value: v.volwritetime ? formatDuration(v.volwritetime) : '—' },
    { label: t('Used Bytes'), value: formatBytes(v.volbytes) },
    { label: t('Max Bytes'), value: Number(v.maxvolbytes) > 0 ? formatBytes(v.maxvolbytes) : '∞' },
    { label: t('Retention'), value: formatDuration(v.volretention) },
    { label: t('Use Duration'), value: Number(v.voluseduration) > 0 ? formatDuration(v.voluseduration) : '—' },
    { label: t('Max Jobs/Vol'), value: Number(v.maxvoljobs) > 0 ? v.maxvoljobs : '∞' },
    { label: t('Max Files/Vol'), value: Number(v.maxvolfiles) > 0 ? v.maxvolfiles : '∞' },
    { label: t('Recycle'), value: v.recycle === '1' ? t('Yes') : t('No') },
    { label: t('Auto Prune'), value: v.autoprune === '1' ? t('Yes') : t('No') },
    { label: t('Recycle Count'), value: v.recyclecount ?? '0' },
    { label: t('Comment'), value: v.comment || '—' },
  ].filter(r => r.value !== '—' || r.label === t('Storage') || r.label === t('Comment'))
})

// ── Jobs table ────────────────────────────────────────────────────────────────

const JOB_COLORS = [
  '#1976D2', '#388E3C', '#F57C00', '#7B1FA2', '#C62828',
  '#00838F', '#558B2F', '#EF6C00', '#6A1B9A', '#AD1457',
  '#00695C', '#283593', '#E65100', '#4527A0', '#2E7D32',
]

function segmentColor(index) {
  return JOB_COLORS[index % JOB_COLORS.length]
}

const volumeTapeSegments = computed(() => buildVolumeTapeSegments(
  volumeUsage.value,
  jobs.value
))
const volumeTapeHasOverlaps = computed(() => (
  volumeTapeSegments.value.some(segment => segment.hasOverlaps)
))

const jobCols = computed(() => [
  { name: 'jobid',      label: t('ID'),       field: 'jobid',      align: 'right',  sortable: true },
  { name: 'name',       label: t('Job Name'), field: 'name',       align: 'left',   sortable: true },
  { name: 'client',     label: t('Client'),   field: 'client',     align: 'left',   sortable: true },
  { name: 'level',      label: t('Level'),    field: 'level',      align: 'center', sortable: true },
  { name: 'starttime',  label: t('Start'),    field: 'starttime',  align: 'left',   sortable: true },
  { name: 'jobstatus',  label: t('Status'),   field: 'jobstatus',  align: 'center', sortable: true },
  { name: 'jobbytes',   label: t('Bytes'),    field: 'jobbytes',   align: 'right',  sortable: true },
])
</script>

<style scoped>
.volume-tape-bar {
  border-radius: 4px;
  display: flex;
  height: 16px;
  min-width: 0;
  overflow: hidden;
}

.volume-tape-segment {
  flex-shrink: 0;
  min-width: 2px;
}

.volume-tape-lane-row {
  align-items: center;
  display: flex;
  gap: 8px;
  margin-bottom: 4px;
}

.volume-tape-lane-label {
  flex: 0 0 48px;
  overflow: hidden;
  text-align: right;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.volume-tape-lane {
  border-radius: 4px;
  flex: 1 1 auto;
  height: 14px;
  min-width: 0;
  position: relative;
}

.volume-tape-range {
  border-radius: 4px;
  height: 100%;
  min-width: 2px;
  position: absolute;
  top: 0;
}
</style>
