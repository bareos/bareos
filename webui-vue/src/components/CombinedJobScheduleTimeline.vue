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
  <div class="combined-timeline column full-height" data-testid="combined-job-schedule-timeline">
    <div class="row items-center no-wrap q-gutter-xs q-pb-xs combined-timeline__toolbar">
      <q-btn flat round dense icon="zoom_out" :disable="!canZoomOut" :title="t('Zoom out')" :aria-label="t('Zoom out')" @click="zoomOut" />
      <q-btn flat round dense icon="zoom_in" :disable="!canZoomIn" :title="t('Zoom in')" :aria-label="t('Zoom in')" @click="zoomIn" />
      <q-chip dense square outline color="primary" class="combined-timeline__zoom-label">
        {{ zoomLabel }}
      </q-chip>
      <q-space />
      <q-btn flat round dense icon="chevron_left" :title="t('Pan to the past')" :aria-label="t('Pan to the past')" @click="panPast" />
      <span class="text-caption text-weight-medium combined-timeline__period">{{ periodLabel }}</span>
      <q-btn flat round dense icon="chevron_right" :title="t('Pan to the future')" :aria-label="t('Pan to the future')" @click="panFuture" />
      <q-btn flat round dense icon="my_location" :title="t('Center on now')" :aria-label="t('Center on now')" @click="centerOnNow" />
      <q-btn flat round dense icon="refresh" :title="t('Refresh')" :aria-label="t('Refresh')" @click="$emit('refresh')" />
    </div>

    <q-banner v-if="beyondJobFetchWindow" dense class="bg-warning text-dark q-mb-xs">
      {{ t('Actual job history is limited to the last {days} days; scheduled trigger markers remain visible for this range.', { days: MAX_JOB_FETCH_DAYS }) }}
    </q-banner>

    <div class="row items-center q-gutter-md q-pb-xs text-caption">
      <div class="row items-center" style="gap:5px">
        <span class="combined-timeline__legend-bar" />
        <span>{{ t('Actual jobs') }}</span>
      </div>
      <div class="row items-center" style="gap:5px">
        <span class="combined-timeline__legend-tick" />
        <span>{{ t('Scheduled triggers') }}</span>
      </div>
      <div v-if="showNowLine" class="row items-center" style="gap:5px">
        <span class="combined-timeline__legend-now" />
        <span>{{ t('Now') }}</span>
      </div>
    </div>

    <div
      ref="bodyRef"
      class="combined-timeline__body col"
      :class="{ 'combined-timeline__body--panning': pointerPan !== null }"
      @wheel.prevent="onTimelineWheel"
    >
      <q-inner-loading :showing="loading" />
      <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>
      <div v-else-if="!hasContent" class="text-grey-7 text-center q-py-xl">
        {{ t('No actual or scheduled jobs in this period.') }}
      </div>
      <template v-else>
        <div class="combined-timeline__axis-header">
          <div />
          <div
            class="combined-timeline__axis combined-timeline__axis--header"
            @pointerdown="startPointerPan"
          >
            <div v-for="tick in axisTicks" :key="tick.key"
                 class="combined-timeline__gridline combined-timeline__gridline--header"
                 :style="{ left: `${tick.percent}%` }">
              <span v-if="tick.label" class="combined-timeline__tick-label">{{ tick.label }}</span>
            </div>
            <div v-if="showNowLine" class="combined-timeline__now"
                 :style="{ left: `${nowPercent}%` }"
                 tabindex="0"
                 :aria-label="nowLineLabel">
              <q-tooltip>{{ nowLineLabel }}</q-tooltip>
            </div>
          </div>
        </div>
        <div v-for="group in groups" :key="group.director ?? 'single'" class="q-mb-sm">
          <div v-if="multiDirectorTimeline" class="text-caption text-weight-bold text-primary q-mb-xs">
            {{ group.director }}
          </div>
          <div v-for="lane in group.lanes" :key="lane.key" class="combined-timeline__lane">
            <div class="combined-timeline__lane-label">
              <span class="text-weight-medium" :title="lane.name">{{ midEllipsis(lane.name || lane.schedule, 22) }}</span>
              <span v-if="lane.client" class="text-primary cursor-pointer"
                    :title="lane.client"
                    @click="openClient(lane)">
                {{ midEllipsis(lane.client, 16) }}
              </span>
              <span v-else-if="lane.schedule" class="text-grey-7" :title="lane.schedule">
                {{ midEllipsis(lane.schedule, 18) }}
              </span>
            </div>
            <div class="combined-timeline__axis" @pointerdown="startPointerPan">
              <div v-for="tick in axisTicks" :key="tick.key"
                   class="combined-timeline__gridline"
                   :style="{ left: `${tick.percent}%` }" />
              <div v-if="showNowLine" class="combined-timeline__now"
                   :style="{ left: `${nowPercent}%` }"
                   tabindex="0"
                   :aria-label="nowLineLabel">
                <q-tooltip>{{ nowLineLabel }}</q-tooltip>
              </div>
              <div v-for="run in lane.runs" :key="run.id ?? `${run.name}-${run.starttime}`"
                   class="combined-timeline__job-bar"
                   :style="jobBarStyle(run)"
                   data-testid="combined-timeline-job-bar"
                   tabindex="0"
                   role="img"
                   :aria-label="jobAriaLabel(run)"
                   @click="openJob(run)"
                   @keydown.enter="openJob(run)">
                <q-tooltip max-width="280px">
                  <div class="text-weight-bold q-mb-xs">{{ run.name }}</div>
                  <div v-if="multiDirectorTimeline">{{ t('Director') }}: {{ run.director }}</div>
                  <div v-if="run.client">{{ t('Client') }}: {{ run.client }}</div>
                  <div v-if="run.fileset">{{ t('Fileset') }}: {{ run.fileset }}</div>
                  <div v-if="run.id != null">{{ t('ID') }}: {{ run.id }}</div>
                  <div>{{ t('Status') }}: {{ displayJobStatus(run) }}</div>
                  <div>{{ t('Start') }}: {{ run.starttime }}</div>
                  <div v-if="run.endtime">{{ t('End') }}: {{ run.endtime }}</div>
                </q-tooltip>
              </div>
              <div v-for="marker in lane.markers" :key="marker.scopeKey ?? `${marker.name}-${marker.runtime}`"
                   class="combined-timeline__schedule-tick"
                   :style="markerStyle(marker)"
                   data-testid="combined-timeline-schedule-tick"
                   tabindex="0"
                   role="img"
                   :aria-label="markerAriaLabel(marker)">
                <q-tooltip max-width="280px">
                  <div class="text-weight-bold q-mb-xs">{{ t('Scheduled trigger') }}</div>
                  <div v-if="multiDirectorTimeline">{{ t('Director') }}: {{ marker.director }}</div>
                  <div>{{ t('Job') }}: {{ marker.name }}</div>
                  <div>{{ t('Schedule') }}: {{ marker.scheduleDisplay || marker.schedule }}</div>
                  <div>{{ t('Time') }}: {{ marker.datetime || formatMarkerTime(marker) }}</div>
                  <div v-if="marker.level">{{ t('Level') }}: {{ marker.level }}</div>
                  <div v-if="marker.pool">{{ t('Pool') }}: {{ marker.pool }}</div>
                  <div v-if="marker.storage">{{ t('Storage') }}: {{ marker.storage }}</div>
                  <div v-if="marker.priority">{{ t('Priority') }}: {{ marker.priority }}</div>
                </q-tooltip>
              </div>
            </div>
          </div>
        </div>
      </template>
    </div>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { displayJobStatus } from '../composables/useDirectorFetch.js'
import { useNowLine } from '../composables/useNowLine.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  buildCenteredTimelineDayBounds,
  buildCombinedTimelineGroups,
  parseTimelineTimestamp,
} from '../utils/jobTimeline.js'
import { startOfToday } from '../utils/calendarGrid.js'
import { formatLocalDateTime } from '../utils/locales.js'
import { midEllipsis } from '../utils/strings.js'

const props = defineProps({
  actualJobs: {
    type: Array,
    default: () => [],
  },
  scheduledRuns: {
    type: Array,
    default: () => [],
  },
  loading: {
    type: Boolean,
    default: false,
  },
  error: {
    type: String,
    default: '',
  },
  directors: {
    type: Array,
    default: () => [],
  },
})

const emit = defineEmits(['range-change', 'refresh'])

const { t } = useI18n()
const router = useRouter()
const settings = useSettingsStore()
const { nowTick, nowLineLabel } = useNowLine(t)

const DAY_MS = 86_400_000
const HOUR_MS = 3_600_000
const MAX_JOB_FETCH_DAYS = 366
const PAN_FRACTION = 0.25
const RANGE_CHANGE_DEBOUNCE_MS = 250
const windowOptions = [
  { labelKey: '6 h', value: 6 * HOUR_MS },
  { labelKey: '12 h', value: 12 * HOUR_MS },
  { labelKey: '24 h', value: DAY_MS },
  { labelKey: '3 d', value: 3 * DAY_MS },
  { labelKey: '7 d', value: 7 * DAY_MS },
  { labelKey: '30 d', value: 30 * DAY_MS },
]
const windowIndex = ref(2)
const viewCenter = ref(nowTick.value)
const followNow = ref(true)

const multiDirectorTimeline = computed(() => props.directors.length > 1)
const selectedWindow = computed(() => windowOptions[windowIndex.value])
const visibleWindowMs = computed(() => selectedWindow.value.value)
const zoomLabel = computed(() => t(selectedWindow.value.labelKey))
const centerTime = computed(() => followNow.value ? nowTick.value : viewCenter.value)
const bodyRef = ref(null)
const pointerPan = ref(null)
let rangeChangeTimer = null
let emittedInitialRange = false

const bounds = computed(() => {
  return buildCenteredTimelineDayBounds(centerTime.value, visibleWindowMs.value)
})

const now = computed(() => nowTick.value)

const groups = computed(() => buildCombinedTimelineGroups(
  props.actualJobs,
  props.scheduledRuns,
  {
    start: bounds.value.start,
    end: bounds.value.end,
    now: now.value,
    multiDirectorTimeline: multiDirectorTimeline.value,
  }
))

const hasContent = computed(() => groups.value.some(group => group.lanes.length > 0))

const periodLabel = computed(() => {
  const start = new Date(bounds.value.start)
  const end = new Date(bounds.value.end)
  return `${formatLocalDateTime(start, settings.locale, { dateStyle: 'short', timeStyle: 'short' })} – ${formatLocalDateTime(end, settings.locale, { dateStyle: 'short', timeStyle: 'short' })}`
})

const canZoomIn = computed(() => windowIndex.value > 0)
const canZoomOut = computed(() => windowIndex.value < windowOptions.length - 1)

function panBy(offsetMs) {
  followNow.value = false
  viewCenter.value = centerTime.value + offsetMs
}

function panPast() {
  panBy(-visibleWindowMs.value * PAN_FRACTION)
}

function panFuture() {
  panBy(visibleWindowMs.value * PAN_FRACTION)
}

function zoomIn() {
  if (!canZoomIn.value) return
  viewCenter.value = centerTime.value
  windowIndex.value -= 1
}

function zoomOut() {
  if (!canZoomOut.value) return
  viewCenter.value = centerTime.value
  windowIndex.value += 1
}

function centerOnNow() {
  followNow.value = true
  viewCenter.value = nowTick.value
}

function zoomToIndex(nextIndex, anchorRatio = 0.5) {
  if (nextIndex < 0 || nextIndex >= windowOptions.length) return
  const ratio = Math.max(0, Math.min(1, anchorRatio))
  const oldWindowMs = visibleWindowMs.value
  const oldStart = bounds.value.start
  const anchorTime = oldStart + oldWindowMs * ratio
  const nextWindowMs = windowOptions[nextIndex].value

  viewCenter.value = anchorTime + nextWindowMs * (0.5 - ratio)
  followNow.value = false
  windowIndex.value = nextIndex
}

function axisRatioFromEvent(event) {
  const axis = event.target?.closest?.('.combined-timeline__axis')
  const rect = axis?.getBoundingClientRect?.()
    ?? bodyRef.value?.getBoundingClientRect?.()
  if (!rect || rect.width <= 0) return 0.5
  return (event.clientX - rect.left) / rect.width
}

function onTimelineWheel(event) {
  const direction = Math.sign(event.deltaY)
  if (direction === 0) return
  const nextIndex = windowIndex.value + (direction > 0 ? 1 : -1)
  zoomToIndex(nextIndex, axisRatioFromEvent(event))
}

function isTimelineInteractiveTarget(target) {
  return Boolean(target?.closest?.(
    [
      'button',
      'a',
      '.combined-timeline__job-bar',
      '.combined-timeline__schedule-tick',
      '.combined-timeline__now',
      '.combined-timeline__lane-label',
    ].join(',')
  ))
}

function startPointerPan(event) {
  if (!event.isPrimary || event.button !== 0) return
  if (isTimelineInteractiveTarget(event.target)) return

  const axis = event.currentTarget
  const rect = axis.getBoundingClientRect()
  if (rect.width <= 0) return

  followNow.value = false
  viewCenter.value = centerTime.value
  pointerPan.value = {
    pointerId: event.pointerId,
    startX: event.clientX,
    startCenter: viewCenter.value,
    axisWidth: rect.width,
  }

  axis.setPointerCapture?.(event.pointerId)
  window.addEventListener('pointermove', onPointerPanMove)
  window.addEventListener('pointerup', stopPointerPan)
  window.addEventListener('pointercancel', stopPointerPan)
  event.preventDefault()
}

function onPointerPanMove(event) {
  const pan = pointerPan.value
  if (!pan || event.pointerId !== pan.pointerId) return

  const deltaX = event.clientX - pan.startX
  viewCenter.value = pan.startCenter
    - (deltaX / pan.axisWidth) * visibleWindowMs.value
  event.preventDefault()
}

function stopPointerPan(event) {
  if (event && pointerPan.value && event.pointerId !== pointerPan.value.pointerId) {
    return
  }
  const wasPanning = pointerPan.value !== null
  pointerPan.value = null
  window.removeEventListener('pointermove', onPointerPanMove)
  window.removeEventListener('pointerup', stopPointerPan)
  window.removeEventListener('pointercancel', stopPointerPan)
  if (wasPanning) {
    emitRangeChangeNow()
  }
}

function cleanupPointerPan() {
  pointerPan.value = null
  window.removeEventListener('pointermove', onPointerPanMove)
  window.removeEventListener('pointerup', stopPointerPan)
  window.removeEventListener('pointercancel', stopPointerPan)
}

onBeforeUnmount(() => {
  if (rangeChangeTimer !== null) {
    clearTimeout(rangeChangeTimer)
    rangeChangeTimer = null
  }
  cleanupPointerPan()
})

const schedulerRange = computed(() => {
  const today = startOfToday().getTime()
  return {
    from: Math.floor((bounds.value.start - today) / DAY_MS),
    to: Math.ceil((bounds.value.end - today) / DAY_MS),
  }
})

const jobDays = computed(() => {
  const today = startOfToday().getTime()
  const requestedDays = Math.ceil((today - bounds.value.start) / DAY_MS) + 1
  return Math.max(1, Math.min(requestedDays, MAX_JOB_FETCH_DAYS))
})

const beyondJobFetchWindow = computed(() => {
  const today = startOfToday().getTime()
  return Math.ceil((today - bounds.value.start) / DAY_MS) + 1 > MAX_JOB_FETCH_DAYS
})

function currentRangePayload() {
  return {
    start: bounds.value.start,
    end: bounds.value.end,
    jobDays: jobDays.value,
    schedulerRange: schedulerRange.value,
  }
}

function emitRangeChangeNow() {
  if (rangeChangeTimer !== null) {
    clearTimeout(rangeChangeTimer)
    rangeChangeTimer = null
  }
  emit('range-change', currentRangePayload())
}

function scheduleRangeChange() {
  if (pointerPan.value !== null) return
  if (rangeChangeTimer !== null) {
    clearTimeout(rangeChangeTimer)
  }
  rangeChangeTimer = setTimeout(() => {
    rangeChangeTimer = null
    emit('range-change', currentRangePayload())
  }, RANGE_CHANGE_DEBOUNCE_MS)
}

watch(
  () => [bounds.value.start, bounds.value.end],
  () => {
    if (!emittedInitialRange) {
      emittedInitialRange = true
      emitRangeChangeNow()
      return
    }
    scheduleRangeChange()
  },
  { immediate: true }
)

const totalMs = computed(() => Math.max(bounds.value.end - bounds.value.start, 1))

const showNowLine = computed(() => (
  now.value >= bounds.value.start && now.value <= bounds.value.end
))
const nowPercent = computed(() => (
  ((now.value - bounds.value.start) / totalMs.value) * 100
))

const axisTicks = computed(() => {
  if (visibleWindowMs.value <= DAY_MS) {
    const tickCount = visibleWindowMs.value <= 12 * HOUR_MS ? 6 : 8
    return Array.from({ length: tickCount + 1 }, (_, index) => {
      const tickTime = bounds.value.start
        + (visibleWindowMs.value / tickCount) * index
      const tickDate = new Date(tickTime)
      return {
        key: `tick-${tickTime}`,
        percent: (index / tickCount) * 100,
        label: `${String(tickDate.getHours()).padStart(2, '0')}:${String(tickDate.getMinutes()).padStart(2, '0')}`,
      }
    })
  }

  const ticks = []
  const cursor = new Date(bounds.value.start)
  cursor.setHours(0, 0, 0, 0)
  if (cursor.getTime() < bounds.value.start) {
    cursor.setDate(cursor.getDate() + 1)
  }
  while (cursor.getTime() < bounds.value.end) {
    const current = cursor.getTime()
    const label = visibleWindowMs.value <= 7 * DAY_MS || cursor.getDate() === 1 || cursor.getDay() === 1
      ? String(cursor.getDate())
      : ''
    ticks.push({
      key: `day-${current}`,
      percent: ((current - bounds.value.start) / totalMs.value) * 100,
      label,
    })
    cursor.setDate(cursor.getDate() + 1)
  }
  return ticks
})

const statusColors = {
  T: '#21ba45',
  W: '#f2c037',
  f: '#c10015',
  E: '#c10015',
  A: '#9e9e9e',
  R: '#31ccec',
  C: '#bdbdbd',
}

function jobColor(status) {
  return statusColors[status] ?? '#9e9e9e'
}

function scheduleColor(marker) {
  const text = `${marker.director ?? ''}:${marker.scheduleDisplay ?? marker.schedule ?? ''}`
  let hash = 0
  for (let i = 0; i < text.length; i++) {
    hash = Math.imul(31, hash) + text.charCodeAt(i) | 0
  }
  const unsigned = hash >>> 0
  return `hsl(${unsigned % 360} ${60 + ((unsigned >>> 9) % 20)}% ${35 + ((unsigned >>> 17) % 20)}%)`
}

function percentForTimestamp(timestamp) {
  return ((timestamp - bounds.value.start) / totalMs.value) * 100
}

function jobBarStyle(run) {
  const startedAt = parseTimelineTimestamp(run.starttime) ?? bounds.value.start
  const endedAt = parseTimelineTimestamp(run.endtime) ?? Math.min(now.value, bounds.value.end)
  const start = Math.max(startedAt, bounds.value.start)
  const end = Math.min(endedAt, now.value, bounds.value.end)
  return {
    left: `${percentForTimestamp(start)}%`,
    width: `${Math.max(0.4, percentForTimestamp(end) - percentForTimestamp(start))}%`,
    background: jobColor(run.status),
  }
}

function markerStyle(marker) {
  return {
    left: `${percentForTimestamp(marker.runtime)}%`,
    background: scheduleColor(marker),
  }
}

function formatMarkerTime(marker) {
  return formatLocalDateTime(new Date(marker.runtime), settings.locale, {
    dateStyle: 'short',
    timeStyle: 'short',
  })
}

function jobAriaLabel(run) {
  return `${run.name} — ${displayJobStatus(run)} — ${run.starttime}`
}

function markerAriaLabel(marker) {
  return `${marker.name} — ${t('scheduled')} — ${formatMarkerTime(marker)}`
}

function openJob(run) {
  if (run.id == null) return
  router.push({
    name: 'job-details',
    params: { id: run.id },
    query: run.director ? { director: run.director, dashboardOrigin: true } : { dashboardOrigin: true },
  })
}

function openClient(lane) {
  if (!lane.client) return
  router.push({
    name: 'client-details',
    params: { name: lane.client },
    query: lane.director ? { director: lane.director, dashboardOrigin: true } : { dashboardOrigin: true },
  })
}
</script>

<style scoped>
.combined-timeline {
  min-height: 100%;
  font-size: 13px;
}

.combined-timeline__toolbar {
  min-width: 0;
}

.combined-timeline__period {
  min-width: 9rem;
  text-align: center;
  white-space: nowrap;
}

.combined-timeline__legend-bar {
  width: 18px;
  height: 8px;
  border-radius: 3px;
  background: #21ba45;
}

.combined-timeline__legend-tick,
.combined-timeline__legend-now {
  width: 3px;
  height: 14px;
  border-radius: 2px;
}

.combined-timeline__legend-tick {
  background: var(--q-primary);
}

.combined-timeline__legend-now {
  background: #c10015;
}

.combined-timeline__body {
  position: relative;
  min-height: 0;
  overflow: auto;
}

.combined-timeline__body--panning {
  cursor: grabbing;
  user-select: none;
}

.combined-timeline__lane {
  display: grid;
  grid-template-columns: minmax(8rem, 13rem) minmax(16rem, 1fr);
  gap: 0.5rem;
  align-items: center;
  min-height: 32px;
}

.combined-timeline__axis-header {
  position: sticky;
  top: 0;
  z-index: 5;
  display: grid;
  grid-template-columns: minmax(8rem, 13rem) minmax(16rem, 1fr);
  gap: 0.5rem;
  min-height: 24px;
  margin-bottom: 0.25rem;
  background: white;
}

.combined-timeline__lane-label {
  display: flex;
  flex-direction: column;
  min-width: 0;
  line-height: 1.15;
}

.combined-timeline__axis {
  position: relative;
  min-height: 28px;
  border-left: 1px solid rgba(0, 0, 0, 0.12);
  border-bottom: 1px solid rgba(0, 0, 0, 0.08);
  cursor: grab;
  touch-action: none;
}

.combined-timeline__body--panning .combined-timeline__axis {
  cursor: grabbing;
}

.combined-timeline__axis--header {
  min-height: 24px;
  border-bottom-color: rgba(0, 0, 0, 0.18);
}

.combined-timeline__gridline,
.combined-timeline__now {
  position: absolute;
  top: 0;
  bottom: 0;
  width: 1px;
}

.combined-timeline__gridline {
  background: rgba(0, 0, 0, 0.08);
}

.combined-timeline__gridline--header {
  background: rgba(0, 0, 0, 0.14);
}

.combined-timeline__now {
  z-index: 3;
  background: #c10015;
}

.combined-timeline__tick-label {
  position: absolute;
  top: -2px;
  left: 3px;
  font-size: 10px;
  color: #757575;
}

.combined-timeline__job-bar {
  position: absolute;
  top: 8px;
  height: 10px;
  border-radius: 3px;
  cursor: pointer;
  z-index: 2;
}

.combined-timeline__schedule-tick {
  position: absolute;
  top: 5px;
  width: 4px;
  height: 18px;
  border-radius: 2px;
  cursor: help;
  transform: translateX(-50%);
  z-index: 4;
  box-shadow: 0 0 0 1px rgba(255, 255, 255, 0.9);
}

.combined-timeline__job-bar:focus-visible,
.combined-timeline__schedule-tick:focus-visible,
.combined-timeline__now:focus-visible {
  outline: 2px solid var(--q-primary);
  outline-offset: 1px;
}

@media (max-width: 700px) {
  .combined-timeline__axis-header,
  .combined-timeline__lane {
    grid-template-columns: 1fr;
    gap: 0.125rem;
    margin-bottom: 0.5rem;
  }

  .combined-timeline__axis-header > div:first-child {
    display: none;
  }
}
</style>
