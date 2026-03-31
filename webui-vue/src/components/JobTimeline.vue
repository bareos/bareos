<template>
  <q-card flat bordered class="bareos-panel">
    <q-card-section class="panel-header row items-center">
      <span>Job Timeline</span>
      <q-space />
      <q-btn-toggle
        v-model="tlDays"
        dense unelevated no-caps
        :options="[{label:'24h',value:1},{label:'7 days',value:7},{label:'30 days',value:30}]"
        text-color="grey-8" toggle-color="primary" color="white"
        class="q-mr-sm bg-white"
        style="border-radius:4px"
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
      <div v-else-if="!tlRows.length" class="text-grey text-center q-py-xl">
        No jobs in this period.
      </div>
      <div v-else ref="svgContainer" class="tl-container" @mouseleave="hideTooltip">
        <svg :width="svgW" :height="svgH">
          <!-- Alternating row backgrounds -->
          <rect v-for="(row, i) in tlRows" :key="'bg-' + row.client + row.name"
                x="0" :y="i * TL_ROW_H"
                :width="svgW" :height="TL_ROW_H"
                :fill="i % 2 === 0
                  ? ($q.dark.isActive ? 'rgba(255,255,255,0.04)' : 'rgba(0,0,0,0.025)')
                  : 'transparent'" />

          <!-- Vertical grid lines for time axis -->
          <line v-for="t in tlTicks" :key="'grid-' + t.label"
                :x1="tlColsW + t.x" y1="0"
                :x2="tlColsW + t.x" :y2="svgH - TL_AXIS_H"
                :stroke="$q.dark.isActive ? '#3a3a3a' : '#ebebeb'"
                stroke-width="1" />

          <!-- Axis tick labels -->
          <text v-for="t in tlTicks" :key="'tick-' + t.label"
                :x="tlColsW + t.x" :y="svgH - 5"
                font-size="10" text-anchor="middle"
                :fill="$q.dark.isActive ? '#777' : '#bbb'"
                style="font-family:sans-serif; user-select:none">
            {{ t.label }}
          </text>

          <!-- Client column: label centered over all rows for that client -->
          <g v-for="span in tlClientSpans" :key="'client-' + span.client">
            <!-- Separator line above each client group (except the first) -->
            <line v-if="span.startRow > 0"
                  x1="0" :y1="span.startRow * TL_ROW_H"
                  :x2="svgW" :y2="span.startRow * TL_ROW_H"
                  :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                  stroke-width="1" />
            <!-- Client name, vertically centered across its rows -->
            <text :x="TL_CLIENT_W / 2"
                  :y="span.startRow * TL_ROW_H + span.rowCount * TL_ROW_H / 2 + 4"
                  font-size="11" text-anchor="middle" font-weight="600"
                  :fill="$q.dark.isActive ? '#90caf9' : '#1565c0'"
                  style="font-family:sans-serif; user-select:none; cursor:pointer"
                  @click="router.push({ name: 'client-details', params: { name: span.client } })">
              {{ span.client.length > 14 ? span.client.slice(0, 13) + '\u2026' : span.client }}
            </text>
          </g>

          <!-- Vertical divider between client column and job name column -->
          <line :x1="TL_CLIENT_W" y1="0"
                :x2="TL_CLIENT_W" :y2="svgH - TL_AXIS_H"
                :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                stroke-width="1" />

          <!-- Vertical divider between job name column and chart area -->
          <line :x1="tlColsW" y1="0"
                :x2="tlColsW" :y2="svgH - TL_AXIS_H"
                :stroke="$q.dark.isActive ? '#444' : '#ccc'"
                stroke-width="1" />

          <!-- Job name labels -->
          <text v-for="(row, i) in tlRows" :key="'name-' + row.client + row.name"
                :x="tlColsW - 6" :y="i * TL_ROW_H + TL_ROW_H / 2 + 4"
                font-size="11" text-anchor="end"
                :fill="$q.dark.isActive ? '#ccc' : '#555'"
                style="font-family:sans-serif; user-select:none">
            {{ row.name.length > 18 ? row.name.slice(0, 17) + '\u2026' : row.name }}
          </text>

          <!-- Job bars — all runs for each job name in the same row -->
          <g v-for="(row, i) in tlRows" :key="'grp-' + row.client + row.name">
            <rect v-for="run in row.runs" :key="'bar-' + run.id"
                  :x="tlColsW + tlBarStart(run)"
                  :y="i * TL_ROW_H + TL_BAR_PAD"
                  :width="Math.max(2, tlBarWidth(run))"
                  :height="TL_ROW_H - TL_BAR_PAD * 2"
                  :fill="tlColorOf(run.status)"
                  rx="3" style="cursor:pointer"
                  @click="router.push({ name: 'job-details', params: { id: run.id } })"
                  @mouseenter="(e) => showTlTooltip(e, run)"
                  @mousemove="moveTlTooltip" />
          </g>
        </svg>
      </div>
      <div v-if="tlTooltip.visible" class="tl-tooltip"
           :style="`left:${tlTooltip.x}px; top:${tlTooltip.y}px`">
        <div class="text-weight-bold q-mb-xs">{{ tlTooltip.job.name }}</div>
        <div>Client: {{ tlTooltip.job.client }}</div>
        <div>ID: {{ tlTooltip.job.id }}</div>
        <div>Status: {{ jobStatusMap[tlTooltip.job.status]?.label ?? tlTooltip.job.status }}</div>
        <div>Start: {{ tlTooltip.job.starttime }}</div>
        <div v-if="tlTooltip.job.endtime">End: {{ tlTooltip.job.endtime }}</div>
        <div>Duration: {{ tlTooltip.job.duration || '—' }}</div>
        <div>Files: {{ (tlTooltip.job.files ?? 0).toLocaleString() }}</div>
        <div>Bytes: {{ fmtBytes(tlTooltip.job.bytes ?? 0) }}</div>
      </div>
    </q-card-section>
  </q-card>
</template>

<script setup>
import { ref, computed, reactive, watch, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { jobStatusMap, formatBytes } from '../mock/index.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'

const $q      = useQuasar()
const router  = useRouter()
const director = useDirectorStore()
const fmtBytes = formatBytes

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

// ── Computed geometry ─────────────────────────────────────────────────────────
const tlStart  = computed(() => Date.now() - tlDays.value * 24 * 3600 * 1000)
const tlRange  = computed(() => Date.now() - tlStart.value)
const svgW     = computed(() => Math.max(containerW.value, 600))
const tlColsW  = computed(() => TL_CLIENT_W + TL_LABEL_W)
const tlChartW = computed(() => Math.max(svgW.value - tlColsW.value, 1))
const svgH     = computed(() => tlRows.value.length * TL_ROW_H + TL_AXIS_H)

// ── Data helpers ──────────────────────────────────────────────────────────────
function tlParseTS(str) {
  if (!str || str.startsWith('0000')) return null
  return new Date(str.replace(' ', 'T')).getTime()
}

// Flat rows sorted by client → job name.
const tlRows = computed(() => {
  const now = Date.now()
  const clientGroups = new Map()
  for (const raw of (tlRawJobs.value ?? [])) {
    const j = normaliseJob(raw)
    const s = tlParseTS(j.starttime)
    const e = tlParseTS(j.endtime) ?? now
    if (s === null || e < tlStart.value || s > now) continue
    if (!clientGroups.has(j.client)) clientGroups.set(j.client, new Map())
    const jobMap = clientGroups.get(j.client)
    if (!jobMap.has(j.name)) jobMap.set(j.name, { name: j.name, client: j.client, runs: [] })
    jobMap.get(j.name).runs.push(j)
  }
  const rows = []
  for (const [, jobMap] of [...clientGroups.entries()].sort((a, b) => a[0].localeCompare(b[0]))) {
    for (const g of [...jobMap.values()].sort((a, b) => a.name.localeCompare(b.name))) {
      rows.push({ ...g, runs: g.runs.sort((a, b) => (tlParseTS(a.starttime) ?? 0) - (tlParseTS(b.starttime) ?? 0)) })
    }
  }
  return rows
})

// One span per client for the merged client-column label.
const tlClientSpans = computed(() => {
  const spans = []
  let i = 0
  while (i < tlRows.value.length) {
    const client = tlRows.value[i].client
    let count = 0
    while (i + count < tlRows.value.length && tlRows.value[i + count].client === client) count++
    spans.push({ client, startRow: i, rowCount: count })
    i += count
  }
  return spans
})

const tlTicks = computed(() => {
  const steps = 6
  const w     = tlChartW.value
  const start = tlStart.value
  const range = tlRange.value
  return Array.from({ length: steps + 1 }, (_, i) => {
    const ratio = i / steps
    const d     = new Date(start + ratio * range)
    let label
    if (tlDays.value === 1) {
      label = d.getHours().toString().padStart(2, '0') + ':' + d.getMinutes().toString().padStart(2, '0')
    } else if (tlDays.value === 7) {
      label = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'][d.getDay()] + '\u00a0' + d.getDate()
    } else {
      label = (d.getMonth() + 1) + '/' + d.getDate()
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
  if (!director.isConnected) return
  tlLoading.value = true
  tlError.value   = ''
  try {
    const res = await director.call(`llist jobs days=${tlDays.value}`)
    tlRawJobs.value = res?.jobs ?? []
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
