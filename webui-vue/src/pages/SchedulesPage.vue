<template>
  <q-page class="q-pa-md">
    <q-card v-if="directorOptions.length > 1" flat bordered class="q-mb-md bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Schedules Scope') }}</span>
        <q-space />
        <q-chip dense square color="white" text-color="primary" :label="schedulesScopeLabel" />
      </q-card-section>
      <q-card-section>
        <q-select
          v-model="selectedDirectorsModel"
          data-testid="schedules-directors"
          :options="directorOptions"
          option-label="label"
          option-value="value"
          emit-value
          map-options
          multiple
          use-chips
          outlined
          dense
          :label="t('Directors')"
        />
        <div class="text-caption text-grey-6 q-mt-sm">
          {{ t('Select the directors that contribute to the schedules views.') }}
        </div>
        <q-banner
          v-if="directorErrors.length"
          rounded
          dense
          class="bg-warning text-black q-mt-md"
        >
          <template #avatar>
            <q-icon name="warning" />
          </template>
          <div v-for="item in directorErrors" :key="item.director">
            <strong>{{ item.director }}</strong>: {{ item.message }}
          </div>
        </q-banner>
      </q-card-section>
    </q-card>

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status" :label="t('Status')" no-caps />
      <q-tab name="show" :label="t('Show')" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>
      <q-tab-panel name="status" class="q-pa-none">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">
            <span>{{ t('Scheduler Jobs') }}</span>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="isCommonSchedules" dense rounded class="bg-info text-white q-mb-sm">
              {{ t('Schedule and job toggles act on the director shown in each schedule group when multiple directors are selected.') }}
            </q-banner>
            <q-table :rows="scheduleJobRows" :columns="scheduleJobCols"
                     row-key="idx" dense flat :loading="statusLoading"
                     :pagination="{ rowsPerPage: 50 }">
              <template #body="props">
                <q-tr v-if="props.row._firstInGroup" class="sched-group-header">
                  <q-td colspan="1" class="q-pl-sm">
                    <div class="row items-center no-wrap q-gutter-xs">
                      <q-btn
                        flat
                        round
                        dense
                        size="sm"
                        :icon="props.row.schedEnabled ? 'pause' : 'play_arrow'"
                        :color="props.row.schedEnabled ? 'warning' : 'positive'"
                        :title="props.row.schedEnabled ? t('Disable schedule') : t('Enable schedule')"
                        :loading="togglingName === props.row.scheduleKey"
                        @click="toggleSchedule({
                          name: props.row.schedule,
                          enabled: props.row.schedEnabled,
                          director: props.row.director,
                          scopeKey: props.row.scheduleKey,
                        })"
                      />
                      <span class="text-weight-bold">{{ props.row.schedule }}</span>
                      <q-chip
                        v-if="isCommonSchedules"
                        dense
                        square
                        color="primary"
                        text-color="white"
                        :label="props.row.director"
                      />
                      <q-badge :color="props.row.schedEnabled ? 'positive' : 'negative'"
                               :label="props.row.schedEnabled ? t('Enabled') : t('Disabled')" />
                    </div>
                  </q-td>
                </q-tr>
                <q-tr :props="props">
                  <q-td key="job" style="padding-left: 48px">
                    <div v-if="props.row.job !== '—'" class="row items-center no-wrap q-gutter-xs">
                      <q-btn
                        flat
                        round
                        dense
                        size="sm"
                        :icon="props.row.jobEnabled ? 'pause' : 'play_arrow'"
                        :color="props.row.jobEnabled ? 'warning' : 'positive'"
                        :title="props.row.jobEnabled ? t('Disable job') : t('Enable job')"
                        :loading="togglingJob === props.row.jobScopeKey"
                        @click="toggleJob(props.row)"
                      />
                      <router-link
                        :to="{ name: 'jobs', query: props.row.jobsQuery }"
                        class="text-primary"
                        style="text-decoration:none"
                      >{{ props.row.job }}</router-link>
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

        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Scheduler Preview') }}</span>
            <q-space />
            <q-btn-toggle v-model="viewMode" dense flat unelevated
                          :options="viewModeOptions"
                          color="white" text-color="white"
                          toggle-color="primary" toggle-text-color="white"
                          class="q-mr-sm" />
            <q-btn flat round dense icon="chevron_left" color="white" @click="prevPeriod" />
            <span class="text-white q-mx-sm" style="min-width:160px;text-align:center">{{ periodLabel }}</span>
            <q-btn flat round dense icon="chevron_right" color="white" @click="nextPeriod" />
            <q-btn flat round dense icon="today" color="white" class="q-ml-sm" :title="t('Go to today')" @click="goToday" />
          </q-card-section>
          <q-card-section v-if="statusError" class="q-pa-none">
            <q-banner dense class="bg-negative text-white">{{ statusError }}</q-banner>
          </q-card-section>
          <q-card-section v-if="allScheduleOptions.length" class="q-pb-none">
            <div class="row items-center q-gutter-sm">
              <span class="text-caption text-grey-7">{{ t('Show schedules:') }}</span>
              <q-checkbox
                v-for="option in allScheduleOptions"
                :key="option.key"
                v-model="visibleScheduleKeys"
                :val="option.key"
                :label="option.label"
                dense
                :color="scheduleColor(option.label) ? undefined : 'primary'"
              >
                <template #default>
                  <span class="q-ml-xs text-caption"
                        :style="{ color: scheduleColor(option.label) }">{{ option.label }}</span>
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
                     :style="{ background: scheduleColor(run.displaySchedule) }">
                  <span class="sched-cal-run-time">{{ run.time }}</span>
                  <span class="sched-cal-run-name">{{ run.displaySchedule }}</span>
                  <q-tooltip max-width="260px">
                    <div class="text-weight-bold q-mb-xs">{{ run.displaySchedule }}</div>
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

      <q-tab-panel name="show" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Schedules') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshSchedules" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="isCommonSchedules" dense rounded class="bg-info text-white q-mb-sm">
              {{ t('Schedule toggles act on the director shown in each row when multiple directors are selected.') }}
            </q-banner>
            <q-banner v-if="schedError" dense class="bg-negative text-white">{{ schedError }}</q-banner>
            <q-table
              :rows="schedules"
              :columns="schedCols"
              row-key="scopeKey"
              dense
              flat
              :loading="schedLoading"
              :pagination="{ rowsPerPage: 20 }"
            >
              <template #body-cell-director="props">
                <q-td :props="props">
                  <q-chip dense square color="primary" text-color="white" :label="props.value" />
                </q-td>
              </template>
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
                  <q-btn
                    flat
                    round
                    dense
                    size="sm"
                    :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                    :color="props.row.enabled ? 'warning' : 'positive'"
                    :title="props.row.enabled ? t('Disable') : t('Enable')"
                    :loading="togglingName === props.row.scopeKey"
                    @click="toggleSchedule(props.row)"
                  />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { computed, onMounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import {
  fetchAggregatedSchedulesShow,
  fetchAggregatedSchedulesStatus,
} from '../composables/schedulesAggregate.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  withJobsSearchQuery,
  withJobsScopeDirectorQuery,
} from '../utils/jobs.js'

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const { t } = useI18n()

const validTabs = new Set(['status', 'show'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'status'
}

const route = useRoute()
const router = useRouter()
const tab = ref(normaliseTab(route.query.tab))
const directorErrors = ref([])

const directorOptions = computed(() => {
  const values = new Set([
    ...director.availableDirectors,
    ...settings.selectedDirectors,
    auth.user?.director,
    settings.directorName,
  ].filter(Boolean))
  return [...values].map(value => ({ label: value, value }))
})

function syncSelectedDirectors() {
  const validDirectors = directorOptions.value.map(option => option.value)
  const selected = settings.selectedDirectors.filter(value => validDirectors.includes(value))

  if (selected.length > 0) {
    if (selected.length !== settings.selectedDirectors.length) {
      settings.setSelectedDirectors(selected)
    }
    return
  }

  const fallbackDirector = auth.user?.director || settings.directorName
  if (fallbackDirector) {
    settings.setSelectedDirectors([fallbackDirector])
  }
}

const selectedDirectorsModel = computed({
  get: () => settings.selectedDirectors,
  set: (value) => {
    const selected = Array.isArray(value) ? value : []
    if (selected.length > 0) {
      settings.setSelectedDirectors(selected)
      return
    }

    const fallbackDirector = auth.user?.director || settings.directorName
    settings.setSelectedDirectors(fallbackDirector ? [fallbackDirector] : [])
  },
})

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    directorOptions.value.some(option => option.value === value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isCommonSchedules = computed(() => activeDirectors.value.length > 1)
const showDirectorColumn = computed(() => isCommonSchedules.value)
const schedulesScopeLabel = computed(() => (
  isCommonSchedules.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))

async function ensureSingleScopeDirector() {
  if (activeDirectors.value.length !== 1) {
    return
  }

  const scopeDirector = activeDirectors.value[0]
  if (!scopeDirector) {
    return
  }

  if (auth.user?.director === scopeDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(scopeDirector)
}

async function ensureScheduleActionDirector(targetDirector) {
  if (!targetDirector) {
    await ensureSingleScopeDirector()
    return
  }

  if (auth.user?.director === targetDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(targetDirector)
}

const schedLoading = ref(false)
const schedError = ref(null)
const shownSchedules = ref([])
const togglingName = ref(null)

async function refreshSchedules() {
  schedLoading.value = true
  schedError.value = null
  directorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      shownSchedules.value = []
      return
    }

    if (isCommonSchedules.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedSchedulesShow(credentials, activeDirectors.value)
      shownSchedules.value = result.schedules
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const response = await director.call('show schedules')
    const shown = Object.values(response?.schedules ?? {})
    shownSchedules.value = shown.map(item => ({
      name: item?.name ?? '',
      enabled: item?.enabled !== false && item?.enabled !== 0,
      run: Array.isArray(item?.run) ? item.run : (item?.run ? [item.run] : []),
      director: currentDirector,
      scopeKey: `${currentDirector}:${item?.name ?? ''}`,
      displayName: item?.name ?? '',
    }))
  } catch (reason) {
    schedError.value = reason?.message ?? String(reason)
  } finally {
    schedLoading.value = false
  }
}

const schedules = computed(() => shownSchedules.value)

const schedCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name', label: t('Name'), field: 'name', align: 'left', sortable: true },
  { name: 'enabled', label: t('Status'), field: 'enabled', align: 'center' },
  { name: 'run', label: t('Run Directives'), field: 'run', align: 'left' },
  { name: 'actions', label: '', field: 'actions', align: 'center', style: 'width:60px' },
])

async function toggleSchedule(row) {
  const action = row.enabled ? 'disable' : 'enable'
  togglingName.value = row.scopeKey ?? row.name
  try {
    await ensureScheduleActionDirector(row.director)
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
  togglingJob.value = row.jobScopeKey ?? row.job
  try {
    await ensureScheduleActionDirector(row.director)
    await director.call(`${action} job=${row.job}`)
    $q.notify({ type: 'positive', message: t('Job "{name}" {action}d', { name: row.job, action }) })
    await refreshStatus()
  } catch (e) {
    $q.notify({ type: 'negative', message: e.message })
  } finally {
    togglingJob.value = null
  }
}

const statusLoading = ref(false)
const statusError = ref(null)
const schedulesData = ref([])
const previewData = ref([])

const viewMode = ref('month')

function startOfToday() {
  const d = new Date()
  d.setHours(0, 0, 0, 0)
  return d
}
function mondayOf(date) {
  const d = new Date(date)
  d.setHours(0, 0, 0, 0)
  const dow = d.getDay()
  d.setDate(d.getDate() - ((dow + 6) % 7))
  return d
}
function firstOfMonth(date) {
  return new Date(date.getFullYear(), date.getMonth(), 1)
}

const viewAnchor = ref(firstOfMonth(new Date()))

const apiDaysRange = computed(() => {
  const today = startOfToday()
  const MS_DAY = 86400_000
  let from
  let to
  if (viewMode.value === 'month') {
    const y = viewAnchor.value.getFullYear()
    const m = viewAnchor.value.getMonth()
    const first = new Date(y, m, 1)
    const last = new Date(y, m + 1, 0)
    from = Math.floor((first - today) / MS_DAY)
    to = Math.floor((last - today) / MS_DAY) + 1
  } else {
    const mon = new Date(viewAnchor.value)
    const sun = new Date(mon)
    sun.setDate(sun.getDate() + 6)
    from = Math.floor((mon - today) / MS_DAY)
    to = Math.floor((sun - today) / MS_DAY) + 1
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
  }
  const mon = new Date(viewAnchor.value)
  const sun = new Date(mon)
  sun.setDate(sun.getDate() + 6)
  const fmt = d => `${String(d.getDate()).padStart(2, '0')} ${MONTH_NAMES.value[d.getMonth()].slice(0, 3)}`
  return `${fmt(mon)} – ${fmt(sun)} ${sun.getFullYear()}`
})

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

watch(viewMode, (mode) => {
  viewAnchor.value = mode === 'month' ? firstOfMonth(new Date()) : mondayOf(new Date())
})

async function refreshStatus() {
  statusLoading.value = true
  statusError.value = null
  directorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      schedulesData.value = []
      previewData.value = []
      return
    }

    if (isCommonSchedules.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedSchedulesStatus(
        credentials,
        activeDirectors.value,
        apiDaysRange.value
      )
      schedulesData.value = result.schedulesData
      previewData.value = result.previewData
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const [statusResponse, showResponse] = await Promise.all([
      director.call(`status scheduler days=${apiDaysRange.value.from},${apiDaysRange.value.to}`),
      director.call('show schedules'),
    ])
    const active = Array.isArray(statusResponse?.schedules) ? statusResponse.schedules : []
    const activeNames = new Set(active.map(item => item.name))
    const allShown = Object.values(showResponse?.schedules ?? {})
    const disabled = allShown
      .filter(item => !activeNames.has(item.name))
      .map(item => ({ name: item.name, enabled: false, jobs: [] }))

    schedulesData.value = [...active, ...disabled].map((item) => {
      const name = item?.name ?? ''
      return {
        ...item,
        name,
        enabled: item?.enabled !== false && item?.enabled !== 0,
        director: currentDirector,
        scopeKey: `${currentDirector}:${name}`,
        displayName: name,
        jobs: Array.isArray(item?.jobs)
          ? item.jobs.map(job => ({
            ...job,
            director: currentDirector,
            schedule: name,
            scheduleKey: `${currentDirector}:${name}`,
          }))
          : [],
      }
    })
    previewData.value = (Array.isArray(statusResponse?.preview) ? statusResponse.preview : [])
      .map(item => ({
        ...item,
        director: currentDirector,
        scheduleKey: `${currentDirector}:${item.schedule ?? ''}`,
        scheduleDisplay: item.schedule ?? '',
      }))
  } catch (reason) {
    statusError.value = reason?.message ?? String(reason)
  } finally {
    statusLoading.value = false
  }
}

watch(apiDaysRange, refreshStatus, { deep: true, immediate: true })

const scheduleJobRows = computed(() => {
  const rows = []
  for (const sched of schedulesData.value) {
    const jobs = Array.isArray(sched.jobs) ? sched.jobs : []
    if (!jobs.length) {
      rows.push({
        idx: rows.length,
        director: sched.director,
        schedule: sched.name,
        scheduleKey: sched.scopeKey,
          schedEnabled: sched.enabled,
          job: '—',
          jobEnabled: null,
          jobScopeKey: `${sched.scopeKey}:—`,
          jobsQuery: null,
          _firstInGroup: true,
        })
    } else {
      jobs.forEach((job, index) => {
        rows.push({
          idx: rows.length,
          director: sched.director,
          schedule: sched.name,
          scheduleKey: sched.scopeKey,
            schedEnabled: sched.enabled,
            job: job.name,
            jobEnabled: job.enabled,
            jobScopeKey: `${sched.scopeKey}:${job.name}`,
            jobsQuery: withJobsScopeDirectorQuery(
              withJobsSearchQuery({}, job.name),
              sched.director,
            ),
            _firstInGroup: index === 0,
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

const allScheduleOptions = computed(() =>
  schedulesData.value.map(schedule => ({
    key: schedule.scopeKey,
    label: isCommonSchedules.value ? schedule.displayName : schedule.name,
  }))
)

const visibleScheduleKeys = ref([])

watch(allScheduleOptions, (options) => {
  for (const option of options) {
    if (!visibleScheduleKeys.value.includes(option.key)) {
      visibleScheduleKeys.value.push(option.key)
    }
  }
})

const runsByDate = computed(() => {
  const visible = visibleScheduleKeys.value
  const map = {}
  for (const run of previewData.value) {
    if (visible.length && !visible.includes(run.scheduleKey)) continue
    const ts = run.runtime
    if (!ts) continue
    const d = new Date(ts * 1000)
    const date = `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`
    const time = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`
    if (!map[date]) map[date] = []
    map[date].push({
      ...run,
      time,
      displaySchedule: isCommonSchedules.value ? run.scheduleDisplay : run.schedule,
    })
  }
  return map
})

function makeDateStr(y, m, d) {
  return `${y}-${String(m + 1).padStart(2, '0')}-${String(d).padStart(2, '0')}`
}

const calendarCells = computed(() => {
  const today = startOfToday()
  const todayStr = makeDateStr(today.getFullYear(), today.getMonth(), today.getDate())

  if (viewMode.value === 'week') {
    return DAY_ABBR.value.map((_, i) => {
      const day = new Date(viewAnchor.value)
      day.setDate(day.getDate() + i)
      const dateStr = makeDateStr(day.getFullYear(), day.getMonth(), day.getDate())
      return { day: day.getDate(), dateStr, isToday: dateStr === todayStr, runs: runsByDate.value[dateStr] ?? [] }
    })
  }

  const y = viewAnchor.value.getFullYear()
  const m = viewAnchor.value.getMonth()
  const firstWeekday = new Date(y, m, 1).getDay()
  const daysInMonth = new Date(y, m + 1, 0).getDate()
  const startOffset = (firstWeekday + 6) % 7

  const cells = []
  for (let i = 0; i < startOffset; i++) cells.push({ day: 0, runs: [] })
  for (let d = 1; d <= daysInMonth; d++) {
    const dateStr = makeDateStr(y, m, d)
    cells.push({ day: d, dateStr, isToday: dateStr === todayStr, runs: runsByDate.value[dateStr] ?? [] })
  }
  while (cells.length % 7 !== 0) cells.push({ day: 0, runs: [] })
  return cells
})

const SCHED_COLORS = ['#1565c0', '#2e7d32', '#c62828', '#6a1b9a', '#e65100', '#00695c', '#283593', '#558b2f', '#4e342e', '#00838f']
function scheduleColor(name) {
  let h = 0
  for (let i = 0; i < name.length; i++) h = Math.imul(31, h) + name.charCodeAt(i) | 0
  return SCHED_COLORS[Math.abs(h) % SCHED_COLORS.length]
}

watch(tab, (value) => {
  const current = normaliseTab(route.query.tab)
  if (current !== value) {
    const query = { ...route.query }
    delete query.tab
    if (value !== 'status') {
      query.tab = value
    }
    router.replace({ path: '/schedules', query })
  }

  if (value === 'show') {
    refreshSchedules()
  }
})

watch(() => route.query.tab, (value) => {
  const next = normaliseTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  if (tab.value === 'show') {
    refreshSchedules()
  }
  refreshStatus()
})

onMounted(() => {
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
})
</script>

<style scoped>
.sched-group-header td {
  background: rgba(0, 0, 0, 0.04);
  border-top: 2px solid rgba(0, 0, 0, 0.15) !important;
  padding-top: 4px !important;
  padding-bottom: 4px !important;
}
</style>
