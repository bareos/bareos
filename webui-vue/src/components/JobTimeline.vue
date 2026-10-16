<template>
  <q-card flat bordered class="bareos-panel">
    <q-card-section class="panel-header row items-center">
      <span>{{ t('Job Timeline') }}</span>
      <div v-if="showNowLineLegend" class="sched-now-legend q-ml-md">
        <span class="sched-now-legend-swatch" /> {{ t('Now') }}
      </div>
      <q-space />
      <template v-if="inDayView">
        <q-btn ref="backBtnRef" flat dense no-caps icon="arrow_back" color="white" :label="t('Back')"
               class="q-mr-sm" @click="closeDayView" />
        <div class="row items-center no-wrap sched-period-nav">
          <q-btn flat round dense icon="chevron_left" color="white" :title="t('Previous day')" :aria-label="t('Previous day')" @click="prevDay" />
          <span class="text-white sched-period-label sched-day-period-label">{{ dayViewLabel }}</span>
          <q-btn flat round dense icon="chevron_right" color="white" :title="t('Next day')" :aria-label="t('Next day')" @click="nextDay" />
        </div>
      </template>
      <template v-else>
        <div class="row items-center no-wrap sched-period-nav">
          <q-btn flat round dense icon="chevron_left" color="white" :title="t('Previous period')" :aria-label="t('Previous period')" @click="prevPeriod" />
          <span class="text-white sched-period-label">{{ periodLabel }}</span>
          <q-btn flat round dense icon="chevron_right" color="white" :title="t('Next period')" :aria-label="t('Next period')" @click="nextPeriod" />
        </div>
      </template>
      <q-btn-toggle v-model="activeMode" dense unelevated no-caps
                    :options="modeOptions"
                    text-color="grey-9"
                    color="grey-3"
                    toggle-color="primary"
                    toggle-text-color="white"
                    class="q-mx-sm sched-view-toggle" />
      <q-btn flat round dense icon="today" color="white" :title="t('Go to today')" :aria-label="t('Go to today')" @click="goToday" />
      <q-btn flat round dense icon="refresh" color="white" class="q-ml-sm" :title="t('Refresh')" :aria-label="t('Refresh')" @click="tlRefresh" />
    </q-card-section>

    <!-- Status legend -->
    <q-card-section class="q-py-sm row items-center q-gutter-md">
      <div v-for="(info, key) in jobStatusMap" :key="key"
           class="row items-center" style="gap:5px">
        <span class="tl-legend-dot" :style="{ background: tlColorOf(key) }" />
        <span class="text-caption">{{ info.label }}</span>
      </div>
    </q-card-section>

    <q-banner v-if="beyondFetchWindow" dense class="bg-warning text-dark">
      {{ t('Showing data for the last {days} days only; older data is not available.', { days: MAX_FETCH_DAYS }) }}
    </q-banner>

    <!-- "Show jobs" filter -->
    <q-card-section v-if="allJobOptions.length" class="q-pb-none">
      <div class="row items-center q-gutter-sm">
        <span class="text-caption text-grey-7">{{ t('Show jobs:') }}</span>
        <q-btn dense flat no-caps size="sm" color="primary" :label="t('All')"
               :disable="allJobsSelected" @click="selectAllJobs" />
        <q-btn dense flat no-caps size="sm" color="primary" :label="t('None')"
               :disable="noJobsSelected" @click="selectNoJobs" />
        <q-separator vertical inset />
        <q-checkbox
          v-for="option in allJobOptions"
          :key="option.key"
          v-model="visibleJobNames"
          :val="option.key"
          dense
        >
          <template #default>
            <span class="q-ml-xs text-caption">{{ option.label }}</span>
          </template>
        </q-checkbox>
      </div>
    </q-card-section>

    <q-card-section class="q-pa-sm" style="position:relative">
      <q-inner-loading :showing="tlLoading" />
      <div v-if="tlError" class="text-negative q-py-md">{{ tlError }}</div>
      <div v-else-if="!hasVisibleRuns || !periodHasVisibleRuns" class="text-grey-7 text-center q-py-xl">
        {{ t('No jobs in this period.') }}
      </div>
      <div v-else-if="inDayView" class="sched-day-view">
        <div v-for="group in dayViewGroups" :key="`group-${group.director ?? 'single'}`">
          <div v-if="multiDirectorTimeline" class="text-caption text-weight-bold text-primary q-mb-xs">
            {{ group.director }}
          </div>
          <div v-for="lane in group.lanes" :key="lane.key" class="sched-day-lane">
            <div class="sched-day-lane-label">
              <span :title="lane.fileset">{{ midEllipsis(lane.fileset, 20) }}</span>
              <span>@</span>
              <span class="text-primary cursor-pointer" style="text-decoration:underline"
                    :title="lane.client"
                    @click="router.push(clientDetailsRoute(lane))">{{ midEllipsis(lane.client, 14) }}</span>
            </div>
            <div class="sched-day-lane-axis">
              <div v-for="hm in DAY_VIEW_HOUR_MARKS" :key="hm.pct" class="sched-day-hour-mark"
                   :style="{ left: `${hm.pct}%` }">
                <span class="sched-day-hour-label">{{ hm.label }}</span>
              </div>
              <div v-if="dayViewDate === todayDateStr" class="sched-now-line sched-day-now-line"
                   :style="{ left: `${nowLinePercent}%` }"
                   tabindex="0"
                   :aria-label="nowLineLabel">
                <q-tooltip>{{ nowLineLabel }}</q-tooltip>
              </div>
              <div v-for="run in lane.runs" :key="run.id"
                   class="tl-day-bar"
                   :style="dayBarStyle(run)"
                   :class="{ 'tl-day-bar--running': isRunningJobStatus(run.status) }"
                   tabindex="0"
                   role="img"
                   :aria-label="runAriaLabel(run)"
                   @click="router.push(jobDetailsRoute(run))"
                   @keydown.enter="router.push(jobDetailsRoute(run))">
                <q-tooltip max-width="260px">
                  <div class="text-weight-bold q-mb-xs">{{ run.name }}</div>
                  <div v-if="multiDirectorTimeline">{{ t('Director') }}: {{ run.director }}</div>
                  <div>{{ t('Client') }}: {{ run.client }}</div>
                  <div>{{ t('Fileset') }}: {{ run.fileset }}</div>
                  <div>{{ t('ID') }}: {{ run.id }}</div>
                  <div>{{ t('Status') }}: {{ displayJobStatus(run) }}</div>
                  <div>{{ t('Start') }}: {{ run.starttime }}</div>
                  <div v-if="run.endtime">{{ t('End') }}: {{ run.endtime }}</div>
                  <div>{{ t('Duration') }}: {{ run.duration || '—' }}</div>
                  <div>{{ t('Files') }}: {{ formatNumber(run.files ?? 0, settings.locale) }}</div>
                  <div>{{ t('Bytes') }}: {{ fmtBytes(run.bytes ?? 0) }}</div>
                </q-tooltip>
              </div>
            </div>
          </div>
        </div>
      </div>
      <div v-else ref="calendarRef" class="sched-day-view sched-period-view">
        <div class="sched-period-ticks">
          <div class="sched-period-tick-axis">
            <div v-for="tick in periodDayTicks" :key="tick.dateStr"
                 class="sched-period-tick"
                 :class="{ 'sched-period-tick--today': tick.isToday }"
                 :style="{ left: `${tick.pct}%` }"
                 :data-date="tick.dateStr"
                 tabindex="0"
                 role="button"
                 :aria-label="t('View job runs for {date}', { date: tick.dateStr })"
                 @click="openDayView(tick.dateStr)"
                 @keydown.enter="openDayView(tick.dateStr)">
              <span v-if="tick.label" class="sched-period-tick-label">{{ tick.label }}</span>
            </div>
          </div>
        </div>
        <div v-for="group in periodViewGroups" :key="`pgroup-${group.director ?? 'single'}`">
          <div v-if="multiDirectorTimeline" class="text-caption text-weight-bold text-primary q-mb-xs">
            {{ group.director }}
          </div>
          <div v-for="lane in group.lanes" :key="lane.key" class="sched-day-lane">
            <div class="sched-day-lane-label">
              <span :title="lane.fileset">{{ midEllipsis(lane.fileset, 20) }}</span>
              <span>@</span>
              <span class="text-primary cursor-pointer" style="text-decoration:underline"
                    :title="lane.client"
                    @click="router.push(clientDetailsRoute(lane))">{{ midEllipsis(lane.client, 14) }}</span>
            </div>
            <div class="sched-day-lane-axis">
              <div v-for="tick in periodDayTicks" :key="tick.dateStr" class="sched-period-gridline"
                   :style="{ left: `${tick.pct}%` }" />
              <div v-if="periodNowPercent !== null" class="sched-now-line sched-day-now-line"
                   :style="{ left: `${periodNowPercent}%` }"
                   tabindex="0"
                   :aria-label="nowLineLabel">
                <q-tooltip>{{ nowLineLabel }}</q-tooltip>
              </div>
              <div v-for="run in lane.runs" :key="run.id"
                   class="tl-day-bar"
                   :style="periodBarStyle(run)"
                   :class="{ 'tl-day-bar--running': isRunningJobStatus(run.status) }"
                   tabindex="0"
                   role="img"
                   :aria-label="runAriaLabel(run)"
                   @click="router.push(jobDetailsRoute(run))"
                   @keydown.enter="router.push(jobDetailsRoute(run))">
                <q-tooltip max-width="260px">
                  <div class="text-weight-bold q-mb-xs">{{ run.name }}</div>
                  <div v-if="multiDirectorTimeline">{{ t('Director') }}: {{ run.director }}</div>
                  <div>{{ t('Client') }}: {{ run.client }}</div>
                  <div>{{ t('Fileset') }}: {{ run.fileset }}</div>
                  <div>{{ t('ID') }}: {{ run.id }}</div>
                  <div>{{ t('Status') }}: {{ displayJobStatus(run) }}</div>
                  <div>{{ t('Start') }}: {{ run.starttime }}</div>
                  <div v-if="run.endtime">{{ t('End') }}: {{ run.endtime }}</div>
                  <div>{{ t('Duration') }}: {{ run.duration || '—' }}</div>
                  <div>{{ t('Files') }}: {{ formatNumber(run.files ?? 0, settings.locale) }}</div>
                  <div>{{ t('Bytes') }}: {{ fmtBytes(run.bytes ?? 0) }}</div>
                </q-tooltip>
              </div>
            </div>
          </div>
        </div>
      </div>
    </q-card-section>
  </q-card>
</template>

<script setup>
import { computed, nextTick, onBeforeUnmount, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
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
import { useNowLine } from '../composables/useNowLine.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  buildTimelineLanes,
  distinctFilesetClientOptions,
  parseTimelineTimestamp,
} from '../utils/jobTimeline.js'
import {
  firstOfMonth,
  makeDateStr,
  mondayOf,
  parseDateStr,
  startOfToday,
} from '../utils/calendarGrid.js'
import { formatNumber } from '../utils/locales.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { midEllipsis } from '../utils/strings.js'

const {
  clientDetailsQuery = null,
  clientFilter = '',
  jobDetailsQuery = null,
  directors = null,
} = defineProps({
  clientDetailsQuery: {
    type: Object,
    default: null,
  },
  clientFilter: {
    type: String,
    default: '',
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

// ── State ─────────────────────────────────────────────────────────────────────
const tlRawJobs    = ref([])
const tlLoading    = ref(false)
const tlError      = ref('')
const backBtnRef   = ref(null)
const calendarRef  = ref(null)

const normalizedClientFilter = computed(() => String(clientFilter ?? '').trim())

// ── "Now" line (shared with the Scheduler Preview) ─────────────────────────────
const { nowLinePercent, nowLineLabel, todayDateStr } = useNowLine(t)

// ── Month/Week/Day navigation (mirrors SchedulesPage.vue's Scheduler Preview) ──
const calendarMode = ref(settings.jobsTimelineViewMode)
const viewAnchor = ref(calendarMode.value === 'month' ? firstOfMonth(new Date()) : mondayOf(new Date()))
// Job Timeline lands directly in the Day view for today by default (matching
// the previous "24h" default range), unlike the Scheduler Preview which
// starts in the Week/Month grid and only drills in on demand.
const dayViewDate = ref(todayDateStr.value)
const inDayView = computed(() => dayViewDate.value !== null)

const MONTH_NAMES = computed(() => [
  t('January'), t('February'), t('March'), t('April'), t('May'), t('June'),
  t('July'), t('August'), t('September'), t('October'), t('November'), t('December'),
])
const DAY_ABBR = computed(() => [
  t('Mon'), t('Tue'), t('Wed'), t('Thu'), t('Fri'), t('Sat'), t('Sun'),
])
const WEEKDAY_NAMES = computed(() => [
  t('Sunday'), t('Monday'), t('Tuesday'), t('Wednesday'), t('Thursday'), t('Friday'), t('Saturday'),
])

const modeOptions = computed(() => [
  { label: t('Day'), value: 'day' },
  { label: t('Week'), value: 'week' },
  { label: t('Month'), value: 'month' },
])

// Bridges the 3-way Day/Week/Month toggle onto the inDayView/calendarMode
// state pair, so both "click a calendar cell" and "pick Day in the toggle"
// drill into the same Day view.
// When leaving Day view via the toggle (as opposed to the "Back" button),
// anchor the Week/Month grid to the day being left instead of letting the
// calendarMode watcher reset it to today.
const skipModeAnchorReset = ref(false)
const activeMode = computed({
  get: () => (inDayView.value ? 'day' : calendarMode.value),
  set: (value) => {
    if (value === 'day') {
      openDayView(dayViewDate.value ?? todayDateStr.value)
    } else {
      if (dayViewDate.value) {
        const d = parseDateStr(dayViewDate.value)
        viewAnchor.value = value === 'month' ? firstOfMonth(d) : mondayOf(d)
        skipModeAnchorReset.value = true
      }
      calendarMode.value = value
      dayViewDate.value = null
    }
  },
})

const periodLabel = computed(() => {
  if (calendarMode.value === 'month') {
    const y = viewAnchor.value.getFullYear()
    const m = viewAnchor.value.getMonth()
    return `${MONTH_NAMES.value[m]} ${y}`
  }
  const mon = new Date(viewAnchor.value)
  const sun = new Date(mon)
  sun.setDate(sun.getDate() + 6)
  const fmt = d => `${String(d.getDate()).padStart(2, '0')} ${MONTH_NAMES.value[d.getMonth()].slice(0, 3)}`
  return `${fmt(mon)} – ${fmt(sun)} ${sun.getFullYear()}`
})

function prevPeriod() {
  const a = new Date(viewAnchor.value)
  if (calendarMode.value === 'month') {
    a.setMonth(a.getMonth() - 1)
    viewAnchor.value = firstOfMonth(a)
  } else {
    a.setDate(a.getDate() - 7)
    viewAnchor.value = a
  }
}
function nextPeriod() {
  const a = new Date(viewAnchor.value)
  if (calendarMode.value === 'month') {
    a.setMonth(a.getMonth() + 1)
    viewAnchor.value = firstOfMonth(a)
  } else {
    a.setDate(a.getDate() + 7)
    viewAnchor.value = a
  }
}
function goToday() {
  const todayDate = startOfToday()
  if (inDayView.value) {
    dayViewDate.value = makeDateStr(todayDate.getFullYear(), todayDate.getMonth(), todayDate.getDate())
    return
  }
  viewAnchor.value = calendarMode.value === 'month'
    ? firstOfMonth(new Date())
    : mondayOf(new Date())
}

const dayViewLabel = computed(() => {
  if (!dayViewDate.value) return ''
  const d = parseDateStr(dayViewDate.value)
  return `${WEEKDAY_NAMES.value[d.getDay()]}, ${d.getDate()} ${MONTH_NAMES.value[d.getMonth()]} ${d.getFullYear()}`
})

function openDayView(dateStr) {
  dayViewDate.value = dateStr
  nextTick(() => {
    const el = backBtnRef.value?.$el ?? backBtnRef.value
    el?.focus?.()
  })
}
function closeDayView() {
  const returningToDate = dayViewDate.value
  dayViewDate.value = null
  nextTick(() => {
    const selector = returningToDate ? `[data-date="${returningToDate}"]` : null
    const cell = selector ? calendarRef.value?.querySelector(selector) : null
    cell?.focus?.()
  })
}
function prevDay() {
  if (!dayViewDate.value) return
  const d = parseDateStr(dayViewDate.value)
  d.setDate(d.getDate() - 1)
  dayViewDate.value = makeDateStr(d.getFullYear(), d.getMonth(), d.getDate())
}
function nextDay() {
  if (!dayViewDate.value) return
  const d = parseDateStr(dayViewDate.value)
  d.setDate(d.getDate() + 1)
  dayViewDate.value = makeDateStr(d.getFullYear(), d.getMonth(), d.getDate())
}

watch(dayViewDate, (dateStr) => {
  if (!dateStr) return
  const d = parseDateStr(dateStr)
  viewAnchor.value = calendarMode.value === 'month' ? firstOfMonth(d) : mondayOf(d)
})

watch(calendarMode, (mode) => {
  if (skipModeAnchorReset.value) {
    skipModeAnchorReset.value = false
  } else {
    viewAnchor.value = mode === 'month' ? firstOfMonth(new Date()) : mondayOf(new Date())
  }
  settings.setJobsTimelineViewMode(mode)
})

// ── "Show jobs" filter (one checkbox per fileset@client lane) ────────────────
const allJobOptions = computed(() => distinctFilesetClientOptions(tlRawJobs.value))
const jobFilterInitialized = ref(false)
const visibleJobNames = ref([])

watch(allJobOptions, (options) => {
  const optionKeys = new Set(options.map(option => option.key))

  if (!jobFilterInitialized.value) {
    visibleJobNames.value = options.map(option => option.key)
    jobFilterInitialized.value = true
    return
  }

  visibleJobNames.value = visibleJobNames.value.filter(key => optionKeys.has(key))
})

const allJobsSelected = computed(
  () => allJobOptions.value.length > 0
    && visibleJobNames.value.length === allJobOptions.value.length
)
const noJobsSelected = computed(() => visibleJobNames.value.length === 0)
function selectAllJobs() { visibleJobNames.value = allJobOptions.value.map(option => option.key) }
function selectNoJobs() { visibleJobNames.value = [] }

const visibleJobNameSet = computed(() => new Set(visibleJobNames.value))
const visibleJobs = computed(() => (
  jobFilterInitialized.value
    ? tlRawJobs.value.filter(job => visibleJobNameSet.value.has(`${job.fileset ?? ''}\u0000${job.client ?? ''}`))
    : tlRawJobs.value
))

const hasVisibleRuns = computed(() => visibleJobs.value.length > 0)
// Whether the *currently displayed* period (not just the whole fetched
// window) contains a visible run, so an empty Day view or calendar page
// shows the "No jobs" message instead of a blank panel.
const periodHasVisibleRuns = computed(() => (
  inDayView.value
    ? dayViewGroups.value.some(group => group.lanes.length > 0)
    : periodViewGroups.value.some(group => group.lanes.length > 0)
))

// ── Day view lanes (one per director/fileset/client, duration bars) ──────────
const DAY_MS = 86_400_000
function dayBounds(dateStr) {
  const start = parseDateStr(dateStr).getTime()
  return { start, end: start + DAY_MS }
}

const dayViewGroups = computed(() => {
  if (!dayViewDate.value) return []
  const { start, end } = dayBounds(dayViewDate.value)
  const now = Math.min(Date.now(), end)
  return buildTimelineLanes(visibleJobs.value, { start, now, multiDirectorTimeline: multiDirectorTimeline.value })
})

function dayBarStyle(run) {
  const { start, end } = dayBounds(dayViewDate.value)
  const now = Math.min(Date.now(), end)
  const sRaw = parseTimelineTimestamp(run.starttime) ?? start
  const eRaw = parseTimelineTimestamp(run.endtime) ?? now
  const s = Math.max(sRaw, start)
  const e = Math.min(eRaw, now)
  const leftPct = ((s - start) / DAY_MS) * 100
  const widthPct = Math.max(0.4, ((e - s) / DAY_MS) * 100)
  return { left: `${leftPct}%`, width: `${widthPct}%`, background: tlColorOf(run.status) }
}

function runAriaLabel(run) {
  return `${run.name} — ${displayJobStatus(run)} — ${run.starttime}`
}

// ── Week/Month period view (continuous timeline spanning the full period,
// same lane/bar visual language as the Day view above, just scaled) ──────────
// Week always starts on the Monday held in viewAnchor; month always starts
// on the 1st (also held in viewAnchor) and runs up to (but excluding) the
// 1st of the following month.
const periodBounds = computed(() => {
  const start = viewAnchor.value.getTime()
  if (calendarMode.value === 'month') {
    const next = new Date(viewAnchor.value.getFullYear(), viewAnchor.value.getMonth() + 1, 1)
    return { start, end: next.getTime() }
  }
  return { start, end: start + 7 * DAY_MS }
})

// One tick per calendar day in the period, used both for the header row of
// clickable day labels and as gridlines inside each lane. Month view only
// labels the 1st and each Monday to avoid crowding ~30 ticks; week view
// labels every day.
const periodDayTicks = computed(() => {
  const { start, end } = periodBounds.value
  const total = end - start
  const todayStr = todayDateStr.value
  const ticks = []
  const cursor = new Date(start)
  while (cursor.getTime() < end) {
    const dateStr = makeDateStr(cursor.getFullYear(), cursor.getMonth(), cursor.getDate())
    const showLabel = calendarMode.value === 'week' || cursor.getDate() === 1 || cursor.getDay() === 1
    ticks.push({
      dateStr,
      pct: ((cursor.getTime() - start) / total) * 100,
      isToday: dateStr === todayStr,
      label: showLabel
        ? (calendarMode.value === 'week' ? `${DAY_ABBR.value[(cursor.getDay() + 6) % 7]} ${cursor.getDate()}` : String(cursor.getDate()))
        : '',
    })
    cursor.setDate(cursor.getDate() + 1)
  }
  return ticks
})

const periodNowPercent = computed(() => {
  const { start, end } = periodBounds.value
  const now = Date.now()
  if (now < start || now > end) return null
  return ((now - start) / (end - start)) * 100
})

const periodViewGroups = computed(() => {
  if (inDayView.value) return []
  const { start, end } = periodBounds.value
  const now = Math.min(Date.now(), end)
  return buildTimelineLanes(visibleJobs.value, { start, now, multiDirectorTimeline: multiDirectorTimeline.value })
})

function periodBarStyle(run) {
  const { start, end } = periodBounds.value
  const total = end - start
  const now = Math.min(Date.now(), end)
  const sRaw = parseTimelineTimestamp(run.starttime) ?? start
  const eRaw = parseTimelineTimestamp(run.endtime) ?? now
  const s = Math.max(sRaw, start)
  const e = Math.min(eRaw, now)
  const leftPct = ((s - start) / total) * 100
  const widthPct = Math.max(0.3, ((e - s) / total) * 100)
  return { left: `${leftPct}%`, width: `${widthPct}%`, background: tlColorOf(run.status) }
}

// ── Colors ──────────────────────────────────────────────────────────────────
const tlStatusColors = {
  T: '#21ba45', W: '#f2c037', f: '#c10015', E: '#c10015',
  A: '#9e9e9e', R: '#31ccec', C: '#bdbdbd',
}
function tlColorOf(status) { return tlStatusColors[status] ?? '#9e9e9e' }

// ── Routing helpers ──────────────────────────────────────────────────────────
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

// Labeled gridlines for the enlarged Day view axis (every 6 hours).
const DAY_VIEW_HOUR_MARKS = [0, 6, 12, 18].map(h => ({
  pct: (h / 24) * 100,
  label: `${String(h).padStart(2, '0')}:00`,
}))

const showNowLineLegend = computed(() => {
  if (inDayView.value) return dayViewDate.value === todayDateStr.value
  return periodNowPercent.value !== null
})

// ── Data fetching ─────────────────────────────────────────────────────────────
// `llist jobs` only supports a "days back from now" window (no absolute date
// range), so paging back to an older week/month means widening that window
// to cover the oldest date currently in view. Capped to avoid unbounded
// queries when a user pages far into the past.
const MAX_FETCH_DAYS = 366
const requestedFetchDays = computed(() => {
  const today = startOfToday()
  let earliest
  if (inDayView.value) {
    earliest = parseDateStr(dayViewDate.value)
  } else if (calendarMode.value === 'month') {
    earliest = firstOfMonth(viewAnchor.value)
  } else {
    earliest = new Date(viewAnchor.value)
  }
  const diffDays = Math.ceil((today - earliest) / DAY_MS)
  return Math.max(diffDays + 1, 1)
})
const fetchDays = computed(() => Math.min(requestedFetchDays.value, MAX_FETCH_DAYS))
// True once the user has navigated further back than the backend query can
// cover in one request, so the UI can flag that the displayed period may be
// showing incomplete/no data rather than silently looking empty.
const beyondFetchWindow = computed(() => requestedFetchDays.value > MAX_FETCH_DAYS)

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
        const clientClause = normalizedClientFilter.value
          ? ` client=${quoteDirectorString(normalizedClientFilter.value)}`
          : ''
        const [jobsResult, statusResult] = await Promise.all([
          client.call(`llist jobs days=${fetchDays.value}${clientClause}`),
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

watch(fetchDays, () => tlRefresh())

watch(normalizedClientFilter, () => tlRefresh())

watch(() => director.isConnected, (connected) => {
  if (connected) tlRefresh()
}, { immediate: true })
</script>

<style scoped>
.tl-day-bar {
  position: absolute;
  top: 50%;
  transform: translateY(-50%);
  height: 14px;
  border-radius: 3px;
  cursor: pointer;
}

.tl-day-bar:focus-visible {
  outline: 2px solid var(--q-primary);
  outline-offset: 1px;
}

.tl-day-bar--running {
  box-shadow: 0 0 0 2px rgba(49, 204, 236, 0.35);
}
</style>
