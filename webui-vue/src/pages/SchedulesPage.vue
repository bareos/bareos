<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status" :label="t('Status')"  no-caps />
      <q-tab name="show"   :label="t('Show')"    no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>

      <!-- SHOW: all configured schedules -->
      <q-tab-panel name="show" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Schedules') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshSchedules" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="schedError" dense class="bg-negative text-white">{{ schedError }}</q-banner>
            <q-table :rows="schedules" :columns="schedCols" row-key="name" dense flat
                     :loading="schedLoading" :pagination="{ rowsPerPage: 20 }">
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'"
                           :label="props.value ? t('Enabled') : t('Disabled')" />
                </q-td>
              </template>
              <template #body-cell-run="props">
                <q-td :props="props">
                  <div v-for="(r, i) in props.value" :key="i" class="text-caption text-mono">{{ r }}</div>
                  <span v-if="!props.value?.length" class="text-grey-5">—</span>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm"
                         :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                         :color="props.row.enabled ? 'warning' : 'positive'"
                          :title="props.row.enabled ? t('Disable') : t('Enable')"
                         :loading="togglingName === props.row.name"
                         @click="toggleSchedule(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- STATUS: schedule→jobs mapping + calendar preview -->
      <q-tab-panel name="status" class="q-pa-none">

        <!-- Scheduler Jobs: which schedules trigger which jobs -->
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">
            <span>{{ t('Scheduler Jobs') }}</span>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="scheduleJobRows" :columns="scheduleJobCols"
                     row-key="idx" dense flat :loading="statusLoading"
                     :pagination="{ rowsPerPage: 50 }">
              <template #body="props">
                <!-- Group header row: schedule name + toggle, shown once per schedule -->
                <q-tr v-if="props.row._firstInGroup" class="sched-group-header">
                  <q-td colspan="1" class="q-pl-sm">
                    <div class="row items-center no-wrap q-gutter-xs">
                      <q-btn flat round dense size="sm"
                             :icon="props.row.schedEnabled ? 'pause' : 'play_arrow'"
                             :color="props.row.schedEnabled ? 'warning' : 'positive'"
                             :title="props.row.schedEnabled ? t('Disable schedule') : t('Enable schedule')"
                             :loading="togglingName === props.row.schedule"
                             @click="toggleSchedule({ name: props.row.schedule, enabled: props.row.schedEnabled })" />
                      <span class="text-weight-bold">{{ props.row.schedule }}</span>
                      <q-badge :color="props.row.schedEnabled ? 'positive' : 'negative'"
                               :label="props.row.schedEnabled ? t('Enabled') : t('Disabled')" />
                    </div>
                  </q-td>
                </q-tr>
                <!-- Job row -->
                <q-tr :props="props">
                  <q-td key="job" style="padding-left: 48px">
                    <div v-if="props.row.job !== '—'" class="row items-center no-wrap q-gutter-xs">
                      <q-btn flat round dense size="sm"
                             :icon="props.row.jobEnabled ? 'pause' : 'play_arrow'"
                             :color="props.row.jobEnabled ? 'warning' : 'positive'"
                             :title="props.row.jobEnabled ? t('Disable job') : t('Enable job')"
                             :loading="togglingJob === props.row.job"
                             @click="toggleJob(props.row)" />
                      <span>{{ props.row.job }}</span>
                      <q-badge v-if="props.row.jobEnabled !== null"
                               :color="props.row.jobEnabled ? 'positive' : 'negative'"
                               :label="props.row.jobEnabled ? t('Enabled') : t('Disabled')" />
                    </div>
                    <span v-else class="text-grey-5">{{ t('no jobs configured') }}</span>
                  </q-td>
                </q-tr>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Scheduler Preview: Calendar (month or week view) -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Scheduler Preview') }}</span>
            <q-space />
            <q-btn-toggle v-model="viewMode" dense flat unelevated
                          :options="viewModeOptions"
                          color="white" text-color="white"
                          toggle-color="primary" toggle-text-color="white"
                          class="q-mr-sm" />
            <q-btn flat round dense icon="chevron_left"  color="white" @click="prevPeriod" />
            <span class="text-white q-mx-sm" style="min-width:160px;text-align:center">{{ periodLabel }}</span>
            <q-btn flat round dense icon="chevron_right" color="white" @click="nextPeriod" />
            <q-btn flat round dense icon="today" color="white" class="q-ml-sm" :title="t('Go to today')" @click="goToday" />
          </q-card-section>
          <q-card-section v-if="statusError" class="q-pa-none">
            <q-banner dense class="bg-negative text-white">{{ statusError }}</q-banner>
          </q-card-section>
          <!-- Schedule filter checkboxes -->
          <q-card-section v-if="allScheduleNames.length" class="q-pb-none">
            <div class="row items-center q-gutter-sm">
              <span class="text-caption text-grey-7">{{ t('Show schedules:') }}</span>
              <q-checkbox
                v-for="name in allScheduleNames" :key="name"
                v-model="visibleScheduleNames"
                :val="name" :label="name" dense
                :color="scheduleColor(name) ? undefined : 'primary'"
              >
                <template #default>
                  <span class="q-ml-xs text-caption"
                        :style="{ color: scheduleColor(name) }">{{ name }}</span>
                </template>
              </q-checkbox>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-sm" style="position:relative">
            <q-inner-loading :showing="statusLoading" />
            <div class="sched-calendar" :class="viewMode === 'week' ? 'sched-calendar--week' : ''">
              <div v-for="(h, hi) in calendarHeaders" :key="hi" class="sched-cal-header">{{ h }}</div>
              <div v-for="(cell, i) in calendarCells" :key="i"
                   :class="['sched-cal-cell',
                            cell.isToday && 'sched-cal-today',
                            !cell.day && 'sched-cal-empty',
                            viewMode === 'week' && 'sched-cal-cell--week']">
                <div v-if="cell.day" class="sched-cal-day-num">{{ cell.day }}</div>
                <div v-for="(run, j) in cell.runs" :key="j" class="sched-cal-run"
                     :style="{ background: scheduleColor(run.schedule) }">
                  <span class="sched-cal-run-time">{{ run.time }}</span>
                  <span class="sched-cal-run-name">{{ run.schedule }}</span>
                  <q-tooltip max-width="260px">
                    <div class="text-weight-bold q-mb-xs">{{ run.schedule }}</div>
                    <div>{{ run.datetime }}</div>
                      <div v-if="run.level">{{ t('Level') }}: {{ run.level }}</div>
                      <div v-if="run.pool">{{ t('Pool') }}: {{ run.pool }}</div>
                      <div v-if="run.storage">{{ t('Storage') }}: {{ run.storage }}</div>
                      <div v-if="run.priority">{{ t('Priority') }}: {{ run.priority }}</div>
                  </q-tooltip>
                </div>
              </div>
            </div>
          </q-card-section>
        </q-card>

      </q-tab-panel>

    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useDirectorStore } from '../stores/director.js'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'

const director = useDirectorStore()
const $q = useQuasar()
const { t } = useI18n()

const tab = ref('status')

// ── Show tab ──────────────────────────────────────────────────────────────────
const schedLoading  = ref(false)
const schedError    = ref(null)
const rawSchedules  = ref({})
const togglingName  = ref(null)

async function refreshSchedules() {
  schedLoading.value = true
  schedError.value   = null
  try {
    const res = await director.call('show schedules')
    // show schedules returns {"schedules": {"Name": {name, enabled, run:[...]}}}
    const raw = res?.schedules ?? {}
    rawSchedules.value = Array.isArray(raw) ? Object.fromEntries(raw.map(s => [s.name, s])) : raw
  } catch (e) {
    schedError.value = e.message
  } finally {
    schedLoading.value = false
  }
}

const schedules = computed(() =>
  Object.values(rawSchedules.value).map(s => ({
    name:    s.name ?? '',
    enabled: s.enabled !== false && s.enabled !== 0,
    run:     Array.isArray(s.run) ? s.run : (s.run ? [s.run] : []),
  }))
)

const schedCols = computed(() => [
  { name: 'name',    label: t('Name'),           field: 'name',    align: 'left', sortable: true },
  { name: 'enabled', label: t('Status'),         field: 'enabled', align: 'center' },
  { name: 'run',     label: t('Run Directives'), field: 'run',     align: 'left' },
  { name: 'actions', label: '',                  field: 'actions', align: 'center', style: 'width:60px' },
])

async function toggleSchedule(row) {
  const action = row.enabled ? 'disable' : 'enable'
  togglingName.value = row.name
  try {
    await director.call(`${action} schedule=${row.name}`)
    $q.notify({ type: 'positive', message: t('Schedule "{name}" {action}d', { name: row.name, action }) })
    await Promise.all([refreshSchedules(), refreshStatus()])
  } catch (e) {
    $q.notify({ type: 'negative', message: e.message })
  } finally {
    togglingName.value = null
  }
}

const togglingJob = ref(null)

async function toggleJob(row) {
  const action = row.jobEnabled ? 'disable' : 'enable'
  togglingJob.value = row.job
  try {
    await director.call(`${action} job=${row.job}`)
    $q.notify({ type: 'positive', message: t('Job "{name}" {action}d', { name: row.job, action }) })
    await refreshStatus()
  } catch (e) {
    $q.notify({ type: 'negative', message: e.message })
  } finally {
    togglingJob.value = null
  }
}

// ── Status tab ────────────────────────────────────────────────────────────────
const statusLoading  = ref(false)
const statusError    = ref(null)
const schedulesData  = ref([])   // from "schedules" key
const previewData    = ref([])   // from "preview" key

// ── View state ────────────────────────────────────────────────────────────────
const viewMode  = ref('month')  // 'month' | 'week'

// viewAnchor: a Date set to the first day of the visible month or week (Mon).
// We always store it as midnight local time.
function startOfToday() {
  const d = new Date()
  d.setHours(0, 0, 0, 0)
  return d
}
function mondayOf(date) {
  const d = new Date(date)
  d.setHours(0, 0, 0, 0)
  const dow = d.getDay()                    // 0=Sun
  d.setDate(d.getDate() - ((dow + 6) % 7)) // back to Monday
  return d
}
function firstOfMonth(date) {
  return new Date(date.getFullYear(), date.getMonth(), 1)
}

const viewAnchor = ref(firstOfMonth(new Date()))

// Compute the days-from-today offsets for the API call.
const apiDaysRange = computed(() => {
  const today  = startOfToday()
  const MS_DAY = 86400_000
  let from, to
  if (viewMode.value === 'month') {
    const y = viewAnchor.value.getFullYear()
    const m = viewAnchor.value.getMonth()
    const first = new Date(y, m, 1)
    const last  = new Date(y, m + 1, 0)          // last day of month
    from = Math.floor((first - today) / MS_DAY)
    to   = Math.floor((last  - today) / MS_DAY) + 1
  } else {
    // week: anchor is the Monday
    const mon = new Date(viewAnchor.value)
    const sun = new Date(mon)
    sun.setDate(sun.getDate() + 6)
    from = Math.floor((mon - today) / MS_DAY)
    to   = Math.floor((sun - today) / MS_DAY) + 1
  }
  return { from, to }
})

const MONTH_NAMES = computed(() => [
  t('January'), t('February'), t('March'), t('April'), t('May'), t('June'),
  t('July'), t('August'), t('September'), t('October'), t('November'), t('December'),
])
const DAY_ABBR = computed(() => [
  t('Mon'), t('Tue'), t('Wed'), t('Thu'), t('Fri'), t('Sat'), t('Sun'),
])

const periodLabel = computed(() => {
  if (viewMode.value === 'month') {
    const y = viewAnchor.value.getFullYear()
    const m = viewAnchor.value.getMonth()
    return `${MONTH_NAMES.value[m]} ${y}`
  } else {
    const mon = new Date(viewAnchor.value)
    const sun = new Date(mon)
    sun.setDate(sun.getDate() + 6)
    const fmt = d => `${String(d.getDate()).padStart(2,'0')} ${MONTH_NAMES.value[d.getMonth()].slice(0,3)}`
    return `${fmt(mon)} – ${fmt(sun)} ${sun.getFullYear()}`
  }
})

// Column headers: for month view just Mon–Sun; for week view include the date.
const calendarHeaders = computed(() => {
  if (viewMode.value === 'month') return DAY_ABBR.value
  return DAY_ABBR.value.map((d, i) => {
    const day = new Date(viewAnchor.value)
    day.setDate(day.getDate() + i)
    return `${d} ${day.getDate()}`
  })
})

function prevPeriod() {
  const a = new Date(viewAnchor.value)
  if (viewMode.value === 'month') {
    a.setMonth(a.getMonth() - 1)
    viewAnchor.value = firstOfMonth(a)
  } else {
    a.setDate(a.getDate() - 7)
    viewAnchor.value = a
  }
}
function nextPeriod() {
  const a = new Date(viewAnchor.value)
  if (viewMode.value === 'month') {
    a.setMonth(a.getMonth() + 1)
    viewAnchor.value = firstOfMonth(a)
  } else {
    a.setDate(a.getDate() + 7)
    viewAnchor.value = a
  }
}
function goToday() {
  viewAnchor.value = viewMode.value === 'month'
    ? firstOfMonth(new Date())
    : mondayOf(new Date())
}

// When view mode changes, reset anchor to current period.
watch(viewMode, m => {
  viewAnchor.value = m === 'month' ? firstOfMonth(new Date()) : mondayOf(new Date())
})

async function refreshStatus() {
  statusLoading.value = true
  statusError.value   = null
  try {
    const { from, to } = apiDaysRange.value
    const [res, showRes] = await Promise.all([
      director.call(`status scheduler days=${from},${to}`),
      director.call('show schedules'),
    ])
    const active = Array.isArray(res?.schedules) ? res.schedules : []
    previewData.value = Array.isArray(res?.preview) ? res.preview : []

    // Merge in disabled schedules from "show schedules" so they remain visible
    const activeNames = new Set(active.map(s => s.name))
    const allShown = Object.values(showRes?.schedules ?? {})
    const disabled = allShown
      .filter(s => !activeNames.has(s.name))
      .map(s => ({ name: s.name, enabled: false, jobs: [] }))

    schedulesData.value = [...active, ...disabled]
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

// Auto-refresh whenever the visible window changes (immediate: true loads on mount).
watch(apiDaysRange, refreshStatus, { deep: true, immediate: true })

// Flatten schedules→jobs into table rows with group markers
const scheduleJobRows = computed(() => {
  const rows = []
  for (const sched of schedulesData.value) {
    const jobs = Array.isArray(sched.jobs) ? sched.jobs : []
    if (!jobs.length) {
      rows.push({
        idx: rows.length,
        schedule: sched.name,
        schedEnabled: sched.enabled,
        job: '—',
        jobEnabled: null,
        _firstInGroup: true,
      })
    } else {
      jobs.forEach((j, i) => {
        rows.push({
          idx: rows.length,
          schedule: sched.name,
          schedEnabled: sched.enabled,
          job: j.name,
          jobEnabled: j.enabled,
          _firstInGroup: i === 0,
        })
      })
    }
  }
  return rows
})

const scheduleJobCols = computed(() => [
  { name: 'job', label: t('Job'), field: 'job', align: 'left', headerStyle: 'padding-left: 48px' },
])
const viewModeOptions = computed(() => [
  { label: t('Month'), value: 'month' },
  { label: t('Week'), value: 'week' },
])

// ── Schedule visibility filter for calendar ────────────────────────────────
const allScheduleNames = computed(() =>
  [...new Set(schedulesData.value.map(s => s.name))]
)

// visibleScheduleNames: array of schedule names to show in the calendar.
// Initialised to all schedules when data loads.
const visibleScheduleNames = ref([])

watch(allScheduleNames, names => {
  // Add newly discovered schedule names to the visible set
  for (const n of names) {
    if (!visibleScheduleNames.value.includes(n)) {
      visibleScheduleNames.value.push(n)
    }
  }
})

// Group previewData entries by ISO date "YYYY-MM-DD" using the runtime timestamp.
// Only include entries for currently visible schedules.
const runsByDate = computed(() => {
  const visible = visibleScheduleNames.value
  const map = {}
  for (const r of previewData.value) {
    if (visible.length && !visible.includes(r.schedule)) continue
    const ts = r.runtime
    if (!ts) continue
    const d    = new Date(ts * 1000)
    const date = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`
    const time = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`
    if (!map[date]) map[date] = []
    map[date].push({ ...r, time })
  }
  return map
})

function makeDateStr(y, m, d) {
  return `${y}-${String(m + 1).padStart(2,'0')}-${String(d).padStart(2,'0')}`
}

const calendarCells = computed(() => {
  const today     = startOfToday()
  const todayStr  = makeDateStr(today.getFullYear(), today.getMonth(), today.getDate())

  if (viewMode.value === 'week') {
    return DAY_ABBR.value.map((_, i) => {
      const day = new Date(viewAnchor.value)
      day.setDate(day.getDate() + i)
      const dateStr = makeDateStr(day.getFullYear(), day.getMonth(), day.getDate())
      return { day: day.getDate(), dateStr, isToday: dateStr === todayStr, runs: runsByDate.value[dateStr] ?? [] }
    })
  }

  // Month view
  const y = viewAnchor.value.getFullYear()
  const m = viewAnchor.value.getMonth()
  const firstWeekday = new Date(y, m, 1).getDay()
  const daysInMonth  = new Date(y, m + 1, 0).getDate()
  const startOffset  = (firstWeekday + 6) % 7  // Monday-first

  const cells = []
  for (let i = 0; i < startOffset; i++) cells.push({ day: 0, runs: [] })
  for (let d = 1; d <= daysInMonth; d++) {
    const dateStr = makeDateStr(y, m, d)
    cells.push({ day: d, dateStr, isToday: dateStr === todayStr, runs: runsByDate.value[dateStr] ?? [] })
  }
  while (cells.length % 7 !== 0) cells.push({ day: 0, runs: [] })
  return cells
})

// Assign a stable color to each schedule name via string hash.
const SCHED_COLORS = ['#1565c0','#2e7d32','#c62828','#6a1b9a','#e65100','#00695c','#283593','#558b2f','#4e342e','#00838f']
function scheduleColor(name) {
  let h = 0
  for (let i = 0; i < name.length; i++) h = Math.imul(31, h) + name.charCodeAt(i) | 0
  return SCHED_COLORS[Math.abs(h) % SCHED_COLORS.length]
}

// Status tab loads immediately (default); Show tab loads lazily on first visit.
watch(tab, t => { if (t === 'show') refreshSchedules() })
</script>

<style scoped>
.sched-group-header td {
  background: rgba(0, 0, 0, 0.04);
  border-top: 2px solid rgba(0, 0, 0, 0.15) !important;
  padding-top: 4px !important;
  padding-bottom: 4px !important;
}
</style>
