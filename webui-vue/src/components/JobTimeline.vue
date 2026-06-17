<template>
  <q-card flat bordered class="bareos-panel">
    <q-card-section class="panel-header row items-center">
      <span>{{ t('Job Timeline') }}</span>
      <q-space />
      <q-btn-toggle
        v-model="tlDays"
        dense unelevated no-caps
        :options="timelineRangeOptions"
        text-color="grey-9"
        color="grey-3"
        toggle-color="primary"
        toggle-text-color="white"
        class="q-mr-sm job-timeline-range-toggle"
      />
      <q-btn flat round dense icon="refresh" color="white" @click="tlRefresh" />
    </q-card-section>

    <!-- Status legend -->
    <q-card-section class="q-py-sm row items-center q-gutter-md">
      <div v-for="(info, key) in jobStatusMap" :key="key"
           class="row items-center" style="gap:5px">
        <span class="tl-legend-dot" :style="{ background: tlColorOf(key) }" />
        <span class="text-caption">{{ info.label }}</span>
      </div>
    </q-card-section>

    <q-card-section class="q-pt-none">
      <div v-if="tlLoading" class="text-center q-py-xl">
        <q-spinner-dots color="primary" size="40px" />
      </div>
      <div v-else-if="tlError" class="text-negative q-py-md">{{ tlError }}</div>
      <div v-else-if="!tlGroups.length" class="text-grey text-center q-py-xl">
        {{ t('No jobs in this period.') }}
      </div>
      <div v-else ref="svgContainer" class="tl-container" @mouseleave="hideTooltip">
        <div v-for="group in tlGroups" :key="`group-${group.director ?? 'single'}`" class="tl-group">
          <div
            v-if="multiDirectorTimeline"
            class="text-caption text-weight-bold text-primary q-mb-xs"
          >
            {{ group.director }}
          </div>
          <svg :width="svgW" :height="group.rows.length * TL_ROW_H + TL_AXIS_H">
            <!-- Alternating row backgrounds -->
            <rect v-for="(row, i) in group.rows" :key="'bg-' + (row.director ?? '') + '-' + row.client + '-' + row.name"
                  x="0" :y="i * TL_ROW_H"
                  :width="svgW" :height="TL_ROW_H"
                  :fill="i % 2 === 0
                    ? ($q.dark.isActive ? 'rgba(255,255,255,0.04)' : 'rgba(0,0,0,0.025)')
                    : 'transparent'" />

            <!-- Vertical grid lines for time axis -->
            <line v-for="t in tlTicks" :key="'grid-' + (group.director ?? '') + '-' + t.label"
                  :x1="tlColsW + t.x" y1="0"
                  :x2="tlColsW + t.x" :y2="group.rows.length * TL_ROW_H"
                  :stroke="$q.dark.isActive ? '#3a3a3a' : '#ebebeb'"
                  stroke-width="1" />

            <!-- Axis tick labels -->
            <text v-for="t in tlTicks" :key="'tick-' + (group.director ?? '') + '-' + t.label"
                  :x="tlColsW + t.x" :y="group.rows.length * TL_ROW_H + TL_AXIS_H - 5"
                  font-size="10" text-anchor="middle"
                  :fill="$q.dark.isActive ? '#777' : '#bbb'"
                  style="font-family:sans-serif; user-select:none">
              {{ t.label }}
            </text>

            <!-- Client column: label centered over all rows for that client -->
            <g v-for="span in group.clientSpans" :key="'client-' + (span.director ?? '') + '-' + span.client">
              <line v-if="span.startRow > 0"
                    x1="0" :y1="span.startRow * TL_ROW_H"
                    :x2="svgW" :y2="span.startRow * TL_ROW_H"
                    :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                    stroke-width="1" />
              <text :x="TL_CLIENT_W / 2"
                    :y="span.startRow * TL_ROW_H + span.rowCount * TL_ROW_H / 2 + 4"
                    font-size="11" text-anchor="middle" font-weight="600"
                    :fill="$q.dark.isActive ? '#90caf9' : '#1565c0'"
                    style="font-family:sans-serif; user-select:none; cursor:pointer"
                    @click="router.push(clientDetailsRoute(span))">
                {{ span.label.length > 14 ? span.label.slice(0, 13) + '\u2026' : span.label }}
              </text>
            </g>

            <line :x1="TL_CLIENT_W" y1="0"
                  :x2="TL_CLIENT_W" :y2="group.rows.length * TL_ROW_H"
                  :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                  stroke-width="1" />

            <line :x1="tlColsW" y1="0"
                  :x2="tlColsW" :y2="group.rows.length * TL_ROW_H"
                  :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                  stroke-width="1" />

            <!-- Job name labels -->
            <text v-for="(row, i) in group.rows" :key="'name-' + (row.director ?? '') + '-' + row.client + '-' + row.name"
                  :x="tlColsW - 6" :y="i * TL_ROW_H + TL_ROW_H / 2 + 4"
                  font-size="11" text-anchor="end"
                  :fill="$q.dark.isActive ? '#ccc' : '#555'"
                  style="font-family:sans-serif; user-select:none">
              {{ row.name.length > 18 ? row.name.slice(0, 17) + '\u2026' : row.name }}
            </text>

            <!-- Job bars — all runs for each job name in the same row -->
            <g v-for="(row, i) in group.rows" :key="'grp-' + (row.director ?? '') + '-' + row.client + '-' + row.name">
              <rect v-for="run in row.runs" :key="'bar-' + run.id"
                    :x="tlColsW + tlBarStart(run)"
                    :y="i * TL_ROW_H + TL_BAR_PAD"
                    :width="Math.max(2, tlBarWidth(run))"
                    :height="TL_ROW_H - TL_BAR_PAD * 2"
                    :fill="tlColorOf(run.status)"
                    rx="3" style="cursor:pointer"
                    @click="router.push(jobDetailsRoute(run))"
                    @mouseenter="(e) => showTlTooltip(e, run)"
                    @mousemove="moveTlTooltip" />
            </g>
          </svg>
        </div>
      </div>
      <div v-if="tlTooltip.visible" class="tl-tooltip"
           :style="`left:${tlTooltip.x}px; top:${tlTooltip.y}px`">
        <div class="text-weight-bold q-mb-xs">{{ tlTooltip.job.name }}</div>
        <div v-if="multiDirectorTimeline">{{ t('Director') }}: {{ tlTooltip.job.director }}</div>
        <div>{{ t('Client') }}: {{ tlTooltip.job.client }}</div>
        <div>{{ t('ID') }}: {{ tlTooltip.job.id }}</div>
        <div>{{ t('Status') }}: {{ displayJobStatus(tlTooltip.job) }}</div>
        <div>{{ t('Start') }}: {{ tlTooltip.job.starttime }}</div>
        <div v-if="tlTooltip.job.endtime">{{ t('End') }}: {{ tlTooltip.job.endtime }}</div>
        <div>{{ t('Duration') }}: {{ tlTooltip.job.duration || '—' }}</div>
        <div>{{ t('Files') }}: {{ formatNumber(tlTooltip.job.files ?? 0, settings.locale) }}</div>
        <div>{{ t('Bytes') }}: {{ fmtBytes(tlTooltip.job.bytes ?? 0) }}</div>
      </div>
    </q-card-section>
  </q-card>
</template>

<script setup>
import { ref, computed, reactive, watch, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { jobStatusMap, formatBytes } from '../mock/index.js'
import {
  directorCollection,
  displayJobStatus,
  isRunningJobStatus,
  normaliseJob,
  overlayRuntimeStatuses,
} from '../composables/useDirectorFetch.js'
import { createDirectorCommandClient } from '../composables/directorAggregate.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildTimelineGroups } from '../utils/jobTimeline.js'
import { formatNumber } from '../utils/locales.js'

const {
  clientDetailsQuery = null,
  jobDetailsQuery = null,
  directors = null,
} = defineProps({
  clientDetailsQuery: {
    type: Object,
    default: null,
  },
  jobDetailsQuery: {
    type: Object,
    default: null,
  },
  directors: {
    type: Array,
    default: null,
  },
})

const $q      = useQuasar()
const router  = useRouter()
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const fmtBytes = formatBytes
const { t } = useI18n()
const currentDirector = computed(() => auth.user?.director || settings.directorName || '')
const timelineDirectors = computed(() => (
  Array.isArray(directors) && directors.length > 0
    ? directors.filter(value => typeof value === 'string' && value)
    : (currentDirector.value ? [currentDirector.value] : [])
))
const multiDirectorTimeline = computed(() => timelineDirectors.value.length > 1)

// ── Layout constants ──────────────────────────────────────────────────────────
const TL_CLIENT_W = 110
const TL_LABEL_W  = 140
const TL_ROW_H    = 28
const TL_BAR_PAD  = 5
const TL_AXIS_H   = 24

// ── State ─────────────────────────────────────────────────────────────────────
const tlDays       = ref(1)
const tlRawJobs    = ref([])
const tlLoading    = ref(false)
const tlError      = ref('')
const svgContainer = ref(null)
const containerW   = ref(700)
let _tlResizeObs   = null

const tlTooltip = reactive({ visible: false, x: 0, y: 0, job: null })
const timelineRangeOptions = computed(() => [
  { label: '24h', value: 1 },
  { label: t('7 days'), value: 7 },
  { label: t('30 days'), value: 30 },
])
// ── Computed geometry ─────────────────────────────────────────────────────────
const tlStart  = computed(() => Date.now() - tlDays.value * 24 * 3600 * 1000)
const tlRange  = computed(() => Date.now() - tlStart.value)
const svgW     = computed(() => Math.max(containerW.value, 600))
const tlColsW  = computed(() => TL_CLIENT_W + TL_LABEL_W)
const tlChartW = computed(() => Math.max(svgW.value - tlColsW.value, 1))
// ── Data helpers ──────────────────────────────────────────────────────────────
function tlParseTS(str) {
  if (!str || str.startsWith('0000')) return null
  return new Date(str.replace(' ', 'T')).getTime()
}

const tlGroups = computed(() => buildTimelineGroups(tlRawJobs.value, {
  start: tlStart.value,
  now: Date.now(),
  multiDirectorTimeline: multiDirectorTimeline.value,
}))

function clientDetailsRoute(span) {
  const query = clientDetailsQuery
    ? {
      ...clientDetailsQuery,
      director: span.director || clientDetailsQuery.director || currentDirector.value || '',
    }
    : (span.director ? { director: span.director } : (currentDirector.value ? { director: currentDirector.value } : {}))

  return {
    name: 'client-details',
    params: { name: span.client },
    query,
  }
}

function jobDetailsRoute(run) {
  const query = jobDetailsQuery
    ? {
      ...jobDetailsQuery,
      director: run.director || jobDetailsQuery.director || currentDirector.value || '',
    }
    : (run.director ? { director: run.director } : (currentDirector.value ? { director: currentDirector.value } : {}))

  return {
    name: 'job-details',
    params: { id: run.id },
    query,
  }
}

const tlTicks = computed(() => {
  const steps = 6
  const w     = tlChartW.value
  const start = tlStart.value
  const range = tlRange.value
  return Array.from({ length: steps + 1 }, (_, i) => {
    const ratio = i / steps
    const d     = new Date(start + ratio * range)
    let label
    const yy = d.getFullYear().toString()
    const mm = (d.getMonth() + 1).toString().padStart(2, '0')
    const dd = d.getDate().toString().padStart(2, '0')
    if (tlDays.value === 1) {
      label = d.getHours().toString().padStart(2, '0') + ':' + d.getMinutes().toString().padStart(2, '0')
    } else {
      label = dd + '.' + mm + '.' + yy
    }
    return { x: Math.round(ratio * w), label }
  })
})

function tlBarStart(job) {
  const s = tlParseTS(job.starttime)
  if (s === null) return 0
  return Math.min(Math.max(0, ((s - tlStart.value) / tlRange.value) * tlChartW.value), tlChartW.value)
}

function tlBarWidth(job) {
  const now  = Date.now()
  const sRaw = tlParseTS(job.starttime) ?? tlStart.value
  const eRaw = tlParseTS(job.endtime)   ?? now
  const s    = Math.max(sRaw, tlStart.value)
  const e    = Math.min(eRaw, now)
  return Math.max(0, ((e - s) / tlRange.value) * tlChartW.value)
}

const tlStatusColors = {
  T: '#21ba45', W: '#f2c037', f: '#c10015', E: '#c10015',
  A: '#9e9e9e', R: '#31ccec', C: '#bdbdbd',
}
function tlColorOf(status) { return tlStatusColors[status] ?? '#9e9e9e' }

function showTlTooltip(event, job) {
  tlTooltip.x       = event.clientX + 14
  tlTooltip.y       = event.clientY + 14
  tlTooltip.job     = job
  tlTooltip.visible = true
}
function moveTlTooltip(event) {
  if (!tlTooltip.visible) return
  tlTooltip.x = event.clientX + 14
  tlTooltip.y = event.clientY + 14
}
function hideTooltip() { tlTooltip.visible = false }

async function tlRefresh() {
  if (!director.isConnected && timelineDirectors.value.length === 0) return
  tlLoading.value = true
  tlError.value   = ''
  try {
    const credentials = auth.getCredentials()
    if (!credentials?.password) {
      throw new Error(t('Not logged in.'))
    }

    const results = await Promise.allSettled(timelineDirectors.value.map(async (directorName) => {
      const client = await createDirectorCommandClient({
        ...credentials,
        director: directorName,
      })

      try {
        const [jobsResult, statusResult] = await Promise.all([
          client.call(`llist jobs days=${tlDays.value}`),
          client.call('status director'),
        ])
        const jobs = directorCollection(jobsResult?.jobs).map((job) => ({
          ...normaliseJob(job),
          director: directorName,
        }))
        return jobs.some(job => isRunningJobStatus(job.status))
          ? overlayRuntimeStatuses(jobs, statusResult?.running)
          : jobs
      } finally {
        client.disconnect()
      }
    }))

    const successful = results
      .filter(result => result.status === 'fulfilled')
      .flatMap(result => result.value)
    if (successful.length === 0 && results.some(result => result.status === 'rejected')) {
      throw results.find(result => result.status === 'rejected').reason
    }
    tlRawJobs.value = successful
  } catch (e) {
    tlError.value = e.message
  } finally {
    tlLoading.value = false
  }
}

// When svgContainer is first assigned (data loaded, panel visible), set up
// the ResizeObserver and read the initial width.
watch(svgContainer, (el) => {
  if (!el) return
  containerW.value = el.offsetWidth || 700
  _tlResizeObs?.disconnect()
  _tlResizeObs = new ResizeObserver(([e]) => {
    if (e.contentRect.width > 0) containerW.value = e.contentRect.width
  })
  _tlResizeObs.observe(el)
})

watch(tlDays, () => tlRefresh())

watch(() => director.isConnected, (connected) => {
  if (connected) tlRefresh()
}, { immediate: true })

onUnmounted(() => { _tlResizeObs?.disconnect() })
</script>

<style scoped>
.job-timeline-range-toggle {
  border: 1px solid rgba(255, 255, 255, 0.55);
  border-radius: 6px;
  overflow: hidden;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.12);
}

.job-timeline-range-toggle :deep(.q-btn) {
  min-width: 4.25rem;
  padding-inline: 0.75rem;
}

.job-timeline-range-toggle :deep(.q-btn + .q-btn) {
  border-left: 1px solid rgba(21, 101, 192, 0.16);
}

.tl-group + .tl-group {
  margin-top: 0.75rem;
}
</style>
