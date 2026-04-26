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
    <!-- Back -->
    <q-btn flat no-caps icon="arrow_back" :label="t('Volumes')" class="q-mb-md"
           :to="{ name: 'storages', query: { tab: 'volumes' } }" />

    <!-- Loading / error -->
    <q-spinner v-if="loading" size="40px" class="block q-mx-auto q-mt-xl" />
    <q-banner v-else-if="error" class="bg-negative text-white q-mb-md">{{ error }}</q-banner>

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
            >
              <q-tooltip>{{ t('Encryption key stored in catalog') }}</q-tooltip>
            </q-icon>
          </div>
          <div class="row q-gutter-xs q-mt-xs">
            <q-badge :color="statusColor(vol.volstatus)" :label="vol.volstatus" />
            <q-badge :color="vol.enabled !== '0' && vol.enabled !== false ? 'positive' : 'grey'">
               {{ vol.enabled !== '0' && vol.enabled !== false ? t('Enabled') : t('Disabled') }}
            </q-badge>
            <q-badge v-if="vol.inchanger === '1' || vol.inchanger === true"
                     color="teal" :label="t('In Changer')" />
          </div>
        </div>
        <q-space />
        <!-- Status change -->
         <q-btn flat dense icon="edit" :label="t('Set Status')" color="primary">
          <q-menu anchor="bottom right" self="top right" auto-close>
            <q-list dense style="min-width:130px">
              <q-item-label header class="text-caption">{{ t('Set status') }}</q-item-label>
              <q-item v-for="s in volStatuses" :key="s" clickable
                      :active="vol.volstatus === s"
                      @click="setVolStatus(s)">
                <q-item-section>
                  <q-badge :color="statusColor(s)" :label="s" />
                </q-item-section>
              </q-item>
            </q-list>
          </q-menu>
        </q-btn>
      </div>

      <q-banner v-if="statusMsg.show"
                :class="statusMsg.ok ? 'bg-positive' : 'bg-negative'"
                class="text-white q-mb-md">
        {{ statusMsg.text }}
      </q-banner>

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
                    <component :is="row.component ?? 'span'" v-if="row.component"
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

              <!-- Job usage breakdown -->
              <div v-if="jobUsageSegments.length" class="q-mt-md">
                <div class="row justify-between text-caption text-grey-6 q-mb-xs">
                  <span>{{ t('Space by Job') }}</span>
                  <span>{{ t('{count} job(s)', { count: jobUsageSegments.length }) }}</span>
                </div>
                <!-- stacked bar -->
                <div style="height:14px; display:flex; overflow:hidden; background:#e0e0e0; border-radius:4px">
                  <q-tooltip>
                    <div v-for="seg in jobUsageSegments" :key="seg.jobid" class="text-caption">
                      #{{ seg.jobid }} {{ seg.name }}: {{ formatBytes(seg.jobbytes) }} ({{ seg.pct.toFixed(1) }}%)
                    </div>
                  </q-tooltip>
                  <div
                    v-for="seg in jobUsageSegments"
                    :key="seg.jobid"
                    :style="{ width: seg.width + '%', background: seg.color, flexShrink: 0 }"
                  />
                </div>
                <!-- legend: top 8 jobs -->
                <div class="q-mt-sm">
                  <div
                    v-for="seg in jobUsageSegments.slice(0, 8)"
                    :key="seg.jobid"
                    class="row items-center q-mb-xs text-caption"
                  >
                    <div
                      :style="{ background: seg.color, width: '10px', height: '10px', borderRadius: '2px', flexShrink: 0 }"
                      class="q-mr-sm"
                    />
                    <router-link
                      :to="{ name: 'job-details', params: { id: seg.jobid } }"
                      class="text-primary q-mr-xs"
                      style="white-space:nowrap"
                    >#{{ seg.jobid }}</router-link>
                    <span class="text-grey-7 ellipsis">{{ seg.name }}</span>
                    <q-space />
                    <span class="text-grey-6 q-ml-sm" style="white-space:nowrap">
                      {{ formatBytes(seg.jobbytes) }} ({{ seg.pct.toFixed(1) }}%)
                    </span>
                  </div>
                  <div v-if="jobUsageSegments.length > 8" class="text-caption text-grey-5">
                     + {{ t('{count} more jobs', { count: jobUsageSegments.length - 8 }) }}
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
                       dense flat :pagination="{ rowsPerPage: 15, sortBy: 'jobid', descending: true }">
                <template #body-cell-jobid="props">
                  <q-td :props="props">
                    <router-link :to="{ name: 'job-details', params: { id: props.value } }"
                                 class="text-primary">
                      {{ props.value }}
                    </router-link>
                  </q-td>
                </template>
                <template #body-cell-jobstatus="props">
                  <q-td :props="props" class="text-center">
                    <q-badge :color="jobStatusColor(props.value)" :label="props.value" />
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
import { ref, computed, onMounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute } from 'vue-router'
import { useDirectorStore } from '../stores/director.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import { volumeHasEncryptionKey } from '../utils/volumes.js'

const route      = useRoute()
const director   = useDirectorStore()
const volumeName = route.params.name
const { t } = useI18n()

const vol         = ref(null)
const jobs        = ref([])
const loading     = ref(true)
const jobsLoading = ref(false)
const error       = ref(null)
const statusMsg   = ref({ show: false, ok: true, text: '' })

function quoteDirectorString(value) {
  return `"${String(value).replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`
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

onMounted(async () => {
  try {
    const [volRes, jobRes] = await Promise.all([
      director.call(`llist volume=${quoteDirectorString(volumeName)}`),
      fetchJobs(),
    ])
    // `llist volume=xxx` returns { volume: { <fields> } } (flat object, not
    // array). `llist volumes` would return { volumes: [ ... ] }.
    const raw = volRes?.volumes ?? volRes?.volume ?? null
    if (Array.isArray(raw)) {
      vol.value = raw[0] ?? null
    } else {
      vol.value = raw  // flat object – is the volume record itself
    }
    jobs.value = jobRes
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
})

async function fetchJobs() {
  jobsLoading.value = true
  try {
    const res = await director.call(`llist jobs volume=${quoteDirectorString(volumeName)}`)
    const raw = res?.jobs ?? []
    return Array.isArray(raw) ? raw : Object.values(raw).flat()
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
    { label: t('Media ID'),    value: v.mediaid ?? v.mediaId ?? '—' },
    { label: t('Pool'),        value: v.pool ?? v.Pool ?? '—',
      link: v.pool ? { name: 'pool-details', params: { name: v.pool } } : null },
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

// ── Volume status ─────────────────────────────────────────────────────────────

const volStatuses = ['Append', 'Full', 'Used', 'Purged', 'Recycled', 'Read-Only', 'Error', 'Cleaning']

async function setVolStatus(newStatus) {
  try {
    await director.call(
      `update volume=${quoteDirectorString(volumeName)} volstatus=${quoteDirectorString(newStatus)}`
    )
    if (vol.value) vol.value.volstatus = newStatus
    statusMsg.value = { show: true, ok: true, text: t('Status set to {status}', { status: newStatus }) }
    setTimeout(() => { statusMsg.value.show = false }, 4000)
  } catch (e) {
    statusMsg.value = { show: true, ok: false, text: e.message }
  }
}

// ── Jobs table ────────────────────────────────────────────────────────────────

const JOB_COLORS = [
  '#1976D2', '#388E3C', '#F57C00', '#7B1FA2', '#C62828',
  '#00838F', '#558B2F', '#EF6C00', '#6A1B9A', '#AD1457',
  '#00695C', '#283593', '#E65100', '#4527A0', '#2E7D32',
]

const jobUsageSegments = computed(() => {
  if (!jobs.value.length || !vol.value) return []
  const totalBytes = Number(vol.value.volbytes) || 1
  const sorted = [...jobs.value]
    .filter(j => Number(j.jobbytes) > 0)
    .sort((a, b) => Number(b.jobbytes) - Number(a.jobbytes))
  // Normalize widths: each segment is its share of the total volume bytes,
  // capped so the sum never exceeds 100 % in the bar.
  let remaining = 100
  return sorted.map((j, i) => {
    const pct = (Number(j.jobbytes) / totalBytes) * 100
    const width = Math.min(pct, remaining)
    remaining = Math.max(0, remaining - width)
    return {
      jobid:    j.jobid,
      name:     j.name,
      jobbytes: Number(j.jobbytes),
      pct,
      width,
      color: JOB_COLORS[i % JOB_COLORS.length],
    }
  })
})

const jobCols = computed(() => [
  { name: 'jobid',      label: t('ID'),       field: 'jobid',      align: 'right',  sortable: true },
  { name: 'name',       label: t('Job Name'), field: 'name',       align: 'left',   sortable: true },
  { name: 'client',     label: t('Client'),   field: 'client',     align: 'left',   sortable: true },
  { name: 'level',      label: t('Level'),    field: 'level',      align: 'center' },
  { name: 'starttime',  label: t('Start'),    field: 'starttime',  align: 'left',   sortable: true },
  { name: 'jobstatus',  label: t('Status'),   field: 'jobstatus',  align: 'center', sortable: true },
  { name: 'jobbytes',   label: t('Bytes'),    field: 'jobbytes',   align: 'right',  sortable: true },
])

// ── Color helpers ─────────────────────────────────────────────────────────────

function statusColor(s) {
  return {
    Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative',
    Purged: 'grey', Used: 'orange', 'Read-Only': 'blue-grey', Cleaning: 'teal',
  }[s] ?? 'info'
}

function jobStatusColor(s) {
  return {
    T: 'positive', W: 'warning', E: 'negative', f: 'negative',
    R: 'blue', C: 'blue', A: 'warning', e: 'negative',
  }[s] ?? 'grey'
}
</script>
