<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status" :label="t('Status')" no-caps />
      <q-tab name="show" :label="t('Show')" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated :swipeable="$q.platform.has.touch">
      <q-tab-panel name="status" class="q-pa-none">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Scheduler Jobs') }}</span>
            <q-space />
            <q-input v-model="jobSearch" dense outlined :placeholder="t('Search…')"
                     style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
          </q-card-section>
          <q-card-section class="q-py-sm schedules-list-stats">
            <div class="row items-center q-gutter-sm">
              <q-chip dense square outline color="grey-8" icon="event_note">
                {{ t('Schedules') }}: {{ schedulerStatusStats.totalSchedules }}
              </q-chip>
              <q-chip dense square outline color="positive" icon="check_circle">
                {{ t('Enabled') }}: {{ schedulerStatusStats.enabledSchedules }}
              </q-chip>
              <q-chip dense square outline color="grey-8" icon="work_outline">
                {{ t('Jobs') }}: {{ schedulerStatusStats.totalJobs }}
              </q-chip>
              <q-chip v-if="schedulerStatusStats.nextRun" dense square outline color="primary" icon="schedule">
                {{ t('Next run') }}: {{ schedulerStatusStats.nextRun.displayTime }}
                ({{ schedulerStatusStats.nextRun.schedule }})
                <q-tooltip>{{ schedulerStatusStats.nextRun.detailTime }}</q-tooltip>
              </q-chip>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="scheduleJobRows" :columns="scheduleJobCols"
                     row-key="idx" dense flat :loading="statusLoading"
                     :pagination="{ rowsPerPage: 50 }">
              <template #body="props">
                <q-tr v-if="props.row._isGroupHeader" class="sched-group-header cursor-pointer"
                      @click="toggleGroupCollapse(props.row.scheduleKey)">
                  <q-td key="job" class="q-pl-sm">
                    <div class="row items-center no-wrap q-gutter-xs">
                      <q-btn
                        flat
                        round
                        dense
                        size="sm"
                        :icon="props.row.collapsed ? 'chevron_right' : 'expand_more'"
                        @click.stop="toggleGroupCollapse(props.row.scheduleKey)"
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
                      <q-chip dense square outline color="grey-7" icon="work_outline">
                        {{ t('{n} jobs', { n: props.row.jobCount }) }}
                      </q-chip>
                      <q-chip v-if="props.row.nextRun" dense square outline color="primary" icon="schedule">
                        {{ t('Next') }}: {{ props.row.nextRun.displayTime }}
                        <q-tooltip>{{ props.row.nextRun.detailTime }}</q-tooltip>
                      </q-chip>
                    </div>
                  </q-td>
                  <q-td key="status" class="text-center" @click.stop>
                    <q-toggle
                      :model-value="props.row.schedEnabled"
                      :color="props.row.schedEnabled ? 'positive' : 'negative'"
                      dense
                      :label="props.row.schedEnabled ? t('Enabled') : t('Disabled')"
                      :loading="togglingName === props.row.scheduleKey"
                      @update:model-value="toggleSchedule({
                       name: props.row.schedule,
                       enabled: props.row.schedEnabled,
                       director: props.row.director,
                       scopeKey: props.row.scheduleKey,
                      })"
                    />
                  </q-td>
                </q-tr>
                <q-tr v-else :props="props">
                  <q-td key="job" style="padding-left: 48px">
                    <div v-if="props.row.job !== '—'" class="row items-center no-wrap q-gutter-xs">
                      <router-link
                        :to="{ name: 'jobs', query: props.row.jobsQuery }"
                        class="text-primary"
                        style="text-decoration:none"
                      >{{ props.row.job }}</router-link>
                    </div>
                    <span v-else class="text-grey-5">{{ t('no jobs configured') }}</span>
                  </q-td>
                  <q-td key="status" class="text-center">
                    <q-toggle
                      v-if="props.row.jobEnabled !== null"
                      :model-value="props.row.jobEnabled"
                      :color="jobStatusBadge(props.row).color"
                      dense
                      :label="jobStatusBadge(props.row).label"
                      :loading="togglingJob === props.row.jobScopeKey"
                      @update:model-value="toggleJob(props.row)"
                    >
                      <q-tooltip v-if="jobStatusBadge(props.row).detail">
                        {{ jobStatusBadge(props.row).detail }}
                      </q-tooltip>
                    </q-toggle>
                    <span v-else class="text-grey-5">—</span>
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
            <q-btn-toggle v-model="viewMode" dense unelevated no-caps
                          :options="viewModeOptions"
                          text-color="grey-9"
                          color="grey-3"
                          toggle-color="primary"
                          toggle-text-color="white"
                          class="q-mr-sm sched-view-toggle" />
            <div class="row items-center no-wrap sched-period-nav">
              <q-btn flat round dense icon="chevron_left" color="white" @click="prevPeriod" />
              <span class="text-white sched-period-label">{{ periodLabel }}</span>
              <q-btn flat round dense icon="chevron_right" color="white" @click="nextPeriod" />
            </div>
            <q-btn flat round dense icon="today" color="white" class="q-ml-sm" :title="t('Go to today')" @click="goToday" />
          </q-card-section>
          <q-card-section v-if="statusError" class="q-pa-none">
            <q-banner dense class="bg-negative text-white">{{ statusError }}</q-banner>
          </q-card-section>
          <q-card-section v-if="allScheduleOptions.length" class="q-pb-none">
            <div class="row items-center q-gutter-sm">
              <span class="text-caption text-grey-7">{{ t('Show schedules:') }}</span>
              <q-btn
                dense
                flat
                no-caps
                size="sm"
                color="primary"
                :label="t('All')"
                :disable="allSchedulesSelected"
                @click="selectAllSchedules"
              />
              <q-btn
                dense
                flat
                no-caps
                size="sm"
                color="primary"
                :label="t('None')"
                :disable="noSchedulesSelected"
                @click="selectNoSchedules"
              />
              <q-separator vertical inset />
              <q-checkbox
                v-for="option in allScheduleOptions"
                :key="option.key"
                v-model="visibleScheduleKeys"
                :val="option.key"
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
          <q-card-section v-if="nextUpcomingRuns.length" class="q-pb-none">
            <div class="row items-center q-gutter-sm sched-next-runs">
              <span class="text-caption text-grey-7">{{ t('Next runs:') }}</span>
              <q-chip
                v-for="(run, i) in nextUpcomingRuns"
                :key="i"
                dense
                square
                outline
                :style="{ color: scheduleColor(run.displaySchedule), borderColor: scheduleColor(run.displaySchedule) }"
              >
                <JobLevelBadge v-if="run.level" :level="run.level" class="q-mr-xs" />
                {{ run.displayTime }} — {{ run.displaySchedule }}
                <q-tooltip>{{ run.detailTime }}</q-tooltip>
              </q-chip>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-sm" style="position:relative">
            <q-inner-loading :showing="statusLoading" />
            <div v-if="hasVisibleScheduleSelection && !hasPreviewRuns"
                 class="text-grey-7 text-center q-py-xl">
              {{ t('No runs are scheduled in the selected time range.') }}
            </div>
            <div v-else class="sched-calendar" :class="viewMode === 'week' ? 'sched-calendar--week' : ''">
              <div v-for="(h, hi) in calendarHeaders" :key="hi" class="sched-cal-header">{{ h }}</div>
              <div v-for="(cell, i) in calendarCells" :key="i"
                   :class="['sched-cal-cell',
                            cell.isToday && 'sched-cal-today',
                            cell.isPast && 'sched-cal-past',
                            !cell.day && 'sched-cal-empty',
                            viewMode === 'week' && 'sched-cal-cell--week']">
                <div v-if="cell.day" class="sched-cal-day-num">{{ cell.day }}</div>
                <div v-for="(run, j) in cell.runs" :key="j" class="sched-cal-run"
                     :style="{ background: scheduleColor(run.displaySchedule) }">
                  <JobLevelBadge v-if="run.level" :level="run.level" class="sched-cal-run-level" />
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
                <div v-if="cell.overflowCount" class="sched-cal-run-more">
                  {{ t('+{n} more', { n: cell.overflowCount }) }}
                  <q-tooltip max-width="260px">
                    <div v-for="(run, k) in runsByDate[cell.dateStr]?.slice(MAX_CELL_RUNS[viewMode] ?? 0)" :key="k">
                      {{ run.time }} — {{ run.displaySchedule }}
                    </div>
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
            <q-input v-model="scheduleSearch" dense outlined :placeholder="t('Search…')"
                     style="width:200px" clearable class="q-mr-sm">
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" @click="refreshSchedules(true)" />
          </q-card-section>
          <q-card-section class="q-py-sm schedules-list-stats">
            <div class="row items-center q-gutter-sm">
              <q-chip dense square outline color="grey-8" icon="event_note">
                {{ t('Total') }}: {{ scheduleStats.total }}
              </q-chip>
              <q-chip dense square outline color="positive" icon="check_circle">
                {{ t('Enabled') }}: {{ scheduleStats.enabled }}
              </q-chip>
              <q-chip v-if="scheduleStats.disabled" dense square outline color="negative" icon="pause_circle">
                {{ t('Disabled') }}: {{ scheduleStats.disabled }}
              </q-chip>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="schedError" dense class="bg-negative text-white">{{ schedError }}</q-banner>
            <q-table
              :rows="schedules"
              :columns="schedCols"
              row-key="scopeKey"
              dense
              flat
              :loading="schedLoading"
              :filter="scheduleSearch"
              v-model:pagination="schedulesPagination"
            >
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-toggle
                    :model-value="props.value"
                    :color="props.value ? 'positive' : 'negative'"
                    dense
                    :label="props.value ? t('Enabled') : t('Disabled')"
                    :loading="togglingName === props.row.scopeKey"
                    @update:model-value="toggleSchedule(props.row)"
                  />
                </q-td>
              </template>
              <template #body-cell-run="props">
                <q-td :props="props">
                  <div v-for="(r, i) in props.value" :key="i" class="text-caption text-mono">{{ r }}</div>
                  <span v-if="!props.value?.length" class="text-grey-5">—</span>
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
import { computed, nextTick, onMounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { usePersistedTablePagination } from '../composables/usePersistedTablePagination.js'
import { usePersistedTableFilter } from '../composables/usePersistedTableFilter.js'
import {
  buildShownSchedules,
  buildStatusSchedules,
  fetchAggregatedSchedulesShow,
  fetchAggregatedSchedulesStatus,
  getEffectiveScheduleJobState,
} from '../composables/schedulesAggregate.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { formatRelativeDate } from '../utils/locales.js'
import {
  withJobsSearchQuery,
} from '../utils/jobs.js'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const { t } = useI18n()
const schedulesPagination = usePersistedTablePagination('schedules.list', {
  rowsPerPage: 20,
})

const validTabs = new Set(['status', 'show'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'status'
}

const route = useRoute()
const router = useRouter()
const tab = ref(normaliseTab(route.query.tab))
const directorErrors = ref([])

const {
  directorOptions,
  activeDirectors,
  isCommonScope: isCommonSchedules,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

const showDirectorColumn = computed(() => isCommonSchedules.value)

async function ensureScheduleActionDirector(targetDirector) {
  if (!targetDirector) {
    await ensureSingleScopeDirector()
    return
  }

  await ensureScopeDirector(targetDirector)
}

const schedLoading = ref(false)
const schedError = ref(null)
const shownSchedules = ref([])
const scheduleSearch = usePersistedTableFilter('schedules.show')
const togglingName = ref(null)

async function refreshSchedules(forceRefresh = false) {
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

      const result = await fetchAggregatedSchedulesShow(credentials, activeDirectors.value, { forceRefresh })
      shownSchedules.value = result.schedules
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const [showResponse, scheduleStateResponse] = await Promise.all([
      director.call('show schedules'),
      director.call('.schedule'),
    ])
    shownSchedules.value = buildShownSchedules(showResponse, scheduleStateResponse, currentDirector)
  } catch (reason) {
    schedError.value = reason?.message ?? String(reason)
  } finally {
    schedLoading.value = false
  }
}

const schedules = computed(() => shownSchedules.value)

const scheduleStats = computed(() => {
  const all = shownSchedules.value
  return {
    total: all.length,
    enabled: all.filter(sched => sched.enabled).length,
    disabled: all.filter(sched => !sched.enabled).length,
  }
})

const schedCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name', label: t('Name'), field: 'name', align: 'left', sortable: true },
  { name: 'enabled', label: t('Status'), field: 'enabled', align: 'center', sortable: true, style: 'width:140px' },
  { name: 'run', label: t('Run Directives'), field: 'run', align: 'left', sortable: true },
])

async function toggleSchedule(row) {
  const action = row.enabled ? 'disable' : 'enable'
  togglingName.value = row.scopeKey ?? row.name
  try {
    await ensureScheduleActionDirector(row.director)
    await director.call(`${action} schedule=${quoteDirectorString(row.name)}`)
    await Promise.all([refreshSchedules(true), refreshStatus()])
    $q.notify({
      type: 'positive',
      message: row.enabled
        ? t('Schedule {name} disabled.', { name: row.name })
        : t('Schedule {name} enabled.', { name: row.name }),
    })
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
    await director.call(`${action} job=${quoteDirectorString(row.job)}`)
    await Promise.all([refreshSchedules(true), refreshStatus()])
    await nextTick()

    const updatedRow = scheduleJobRows.value.find(candidate => candidate.jobScopeKey === row.jobScopeKey)
    const state = updatedRow
      ? getEffectiveScheduleJobState(updatedRow.schedEnabled, updatedRow.jobEnabled)
      : null

    let message = row.jobEnabled
      ? t('Job {name} disabled.', { name: row.job })
      : t('Job {name} enabled.', { name: row.job })

    if (updatedRow?.jobEnabled === true) {
      message = state?.code === 'disabled-schedule'
        ? t('Job {name} enabled, but the schedule is disabled.', { name: row.job })
        : t('Job {name} enabled.', { name: row.job })
    } else if (updatedRow?.jobEnabled === false) {
      message = state?.code === 'disabled-job-and-schedule'
        ? t('Job {name} disabled. The schedule is also disabled.', { name: row.job })
        : t('Job {name} disabled.', { name: row.job })
    }

    $q.notify({ type: 'positive', message })
  } catch (e) {
    $q.notify({ type: 'negative', message: e.message })
  } finally {
    togglingJob.value = null
  }
}

function jobStatusBadge(row) {
  const state = getEffectiveScheduleJobState(row.schedEnabled, row.jobEnabled)

  if (state.code === 'unknown') {
    return {
      color: 'grey',
      label: '',
      detail: '',
    }
  }

  switch (state.code) {
    case 'enabled':
      return {
        color: 'positive',
        label: t('Enabled'),
        detail: '',
      }
    case 'disabled-job':
      return {
        color: 'negative',
        label: t('Disabled'),
        detail: t('The job definition is disabled.'),
      }
    case 'disabled-schedule':
      return {
        color: 'warning',
        label: t('Enabled, schedule disabled'),
        detail: t('The schedule is disabled, but the job definition is enabled.'),
      }
    case 'disabled-job-and-schedule':
      return {
        color: 'negative',
        label: t('Disabled, schedule disabled'),
        detail: t('Both the schedule and the job definition are disabled.'),
      }
    default:
      return {
        color: 'grey',
        label: '',
        detail: '',
      }
  }
}

const statusLoading = ref(false)
const statusError = ref(null)
const schedulesData = ref([])
const previewData = ref([])
// Independent of the navigated calendar range, so "next run" info stays
// anchored to "now" even while the user browses the calendar into the
// future or past.
const upcomingPreviewData = ref([])
const UPCOMING_RANGE = { from: 0, to: 60 }

async function refreshUpcomingPreview() {
  try {
    if (activeDirectors.value.length === 0) {
      upcomingPreviewData.value = []
      return
    }

    if (isCommonSchedules.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        return
      }

      const result = await fetchAggregatedSchedulesStatus(
        credentials,
        activeDirectors.value,
        UPCOMING_RANGE
      )
      upcomingPreviewData.value = result.previewData
      return
    }

    const currentDirector = activeDirectors.value[0]
    const statusResponse = await director.call(
      `status scheduler days=${UPCOMING_RANGE.from},${UPCOMING_RANGE.to}`
    )
    upcomingPreviewData.value = (Array.isArray(statusResponse?.preview) ? statusResponse.preview : [])
      .map(item => ({
        ...item,
        director: currentDirector,
        scheduleKey: `${currentDirector}:${item.schedule ?? ''}`,
        scheduleDisplay: item.schedule ?? '',
      }))
  } catch {
    // "Next run" info is a convenience add-on; a failure here should not
    // surface as a page-level error (the calendar preview already reports
    // fetch failures via statusError).
  }
}

const viewMode = ref(settings.schedulesViewMode)

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

const viewAnchor = ref(viewMode.value === 'month' ? firstOfMonth(new Date()) : mondayOf(new Date()))

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
  settings.setSchedulesViewMode(mode)
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
    const [statusResponse, showResponse, showAllResponse, scheduleStateResponse] = await Promise.all([
      director.call(`status scheduler days=${apiDaysRange.value.from},${apiDaysRange.value.to}`),
      director.call('show schedules'),
      director.call('show schedules all'),
      director.call('.schedule'),
    ])
    schedulesData.value = buildStatusSchedules(
      statusResponse,
      showResponse,
      showAllResponse,
      scheduleStateResponse,
      currentDirector
    )
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

const jobSearch = usePersistedTableFilter('schedules.status')
const expandedSchedules = ref(new Set())

function toggleGroupCollapse(key) {
  const next = new Set(expandedSchedules.value)
  if (next.has(key)) {
    next.delete(key)
  } else {
    next.add(key)
  }
  expandedSchedules.value = next
}

const nextRunByScheduleKey = computed(() => {
  const nowSeconds = Date.now() / 1000
  const map = {}
  for (const run of upcomingPreviewData.value) {
    if (typeof run.runtime !== 'number' || run.runtime < nowSeconds) continue
    const existing = map[run.scheduleKey]
    if (!existing || run.runtime < existing.runtime) {
      map[run.scheduleKey] = run
    }
  }
  return map
})

function describeRunTime(run) {
  if (!run) return null
  const relative = typeof run.runtime === 'number'
    ? formatRelativeDate(new Date(run.runtime * 1000), settings.locale)
    : run.datetime
  return {
    displayTime: settings.relativeTime ? relative : run.datetime,
    detailTime: settings.relativeTime ? run.datetime : relative,
  }
}

const scheduleJobRows = computed(() => {
  const search = jobSearch.value.trim().toLowerCase()
  const rows = []
  for (const sched of schedulesData.value) {
    const allJobs = Array.isArray(sched.jobs) ? sched.jobs : []
    const scheduleMatches = sched.name.toLowerCase().includes(search)
    const jobs = search
      ? allJobs.filter(job => scheduleMatches || job.name.toLowerCase().includes(search))
      : allJobs

    if (search && !scheduleMatches && !jobs.length) {
      continue
    }

    const collapsed = !search && !expandedSchedules.value.has(sched.scopeKey)
    const nextRun = describeRunTime(nextRunByScheduleKey.value[sched.scopeKey])

    rows.push({
      idx: rows.length,
      director: sched.director,
      schedule: sched.name,
      scheduleKey: sched.scopeKey,
      schedEnabled: sched.enabled,
      jobCount: jobs.length,
      nextRun,
      collapsed,
      _isGroupHeader: true,
    })

    if (collapsed) {
      continue
    }

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
        _isGroupHeader: false,
      })
    } else {
      jobs.forEach((job) => {
        rows.push({
          idx: rows.length,
          director: sched.director,
          schedule: sched.name,
          scheduleKey: sched.scopeKey,
          schedEnabled: sched.enabled,
          job: job.name,
          jobEnabled: job.enabled,
          jobScopeKey: `${sched.scopeKey}:${job.name}`,
          jobsQuery: withJobsSearchQuery({}, job.name),
          _isGroupHeader: false,
        })
      })
    }
  }
  return rows
})

const schedulerStatusStats = computed(() => {
  const groups = schedulesData.value
  const totalSchedules = groups.length
  const enabledSchedules = groups.filter(sched => sched.enabled).length
  const totalJobs = groups.reduce(
    (sum, sched) => sum + (Array.isArray(sched.jobs) ? sched.jobs.length : 0),
    0
  )
  const nowSeconds = Date.now() / 1000
  const rawNextRun = upcomingPreviewData.value
    .filter(run => typeof run.runtime === 'number' && run.runtime >= nowSeconds)
    .sort((a, b) => a.runtime - b.runtime)[0] ?? null
  const nextRun = rawNextRun ? { ...rawNextRun, ...describeRunTime(rawNextRun) } : null

  return { totalSchedules, enabledSchedules, totalJobs, nextRun }
})

const scheduleJobCols = computed(() => [
  { name: 'job', label: t('Job'), field: 'job', align: 'left', headerStyle: 'padding-left: 48px', sortable: true },
  { name: 'status', label: t('Status'), field: 'status', align: 'center', style: 'width:160px' },
])
const viewModeOptions = computed(() => [
  { label: t('Month'), value: 'month', icon: 'calendar_month' },
  { label: t('Week'), value: 'week', icon: 'view_week' },
])

const allScheduleOptions = computed(() =>
  schedulesData.value.map(schedule => ({
    key: schedule.scopeKey,
    label: isCommonSchedules.value ? schedule.displayName : schedule.name,
  }))
)

const visibleScheduleKeys = ref([])
const scheduleSelectionInitialized = ref(false)

watch(allScheduleOptions, (options) => {
  const optionKeys = new Set(options.map(option => option.key))

  if (!scheduleSelectionInitialized.value) {
    visibleScheduleKeys.value = options.map(option => option.key)
    scheduleSelectionInitialized.value = true
    return
  }

  visibleScheduleKeys.value = visibleScheduleKeys.value.filter(key => optionKeys.has(key))
})

const allSchedulesSelected = computed(
  () => allScheduleOptions.value.length > 0
    && visibleScheduleKeys.value.length === allScheduleOptions.value.length
)
const noSchedulesSelected = computed(() => visibleScheduleKeys.value.length === 0)

function selectAllSchedules() {
  visibleScheduleKeys.value = allScheduleOptions.value.map(option => option.key)
}
function selectNoSchedules() {
  visibleScheduleKeys.value = []
}

const runsByDate = computed(() => {
  const visible = visibleScheduleKeys.value
  const filterActive = scheduleSelectionInitialized.value
  const map = {}
  for (const run of previewData.value) {
    if (filterActive && !visible.includes(run.scheduleKey)) continue
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

const hasVisibleScheduleSelection = computed(() => {
  if (allScheduleOptions.value.length === 0) {
    return false
  }

  return visibleScheduleKeys.value.some(key => allScheduleOptions.value.some(option => option.key === key))
})

const hasPreviewRuns = computed(() => Object.keys(runsByDate.value).length > 0)

const nextUpcomingRuns = computed(() => {
  const nowSeconds = Date.now() / 1000
  const visible = visibleScheduleKeys.value
  const filterActive = scheduleSelectionInitialized.value
  return upcomingPreviewData.value
    .filter(run => typeof run.runtime === 'number' && run.runtime >= nowSeconds)
    .filter(run => !filterActive || visible.includes(run.scheduleKey))
    .sort((a, b) => a.runtime - b.runtime)
    .slice(0, 5)
    .map(run => {
      const relative = typeof run.runtime === 'number'
        ? formatRelativeDate(new Date(run.runtime * 1000), settings.locale)
        : run.datetime
      return {
        ...run,
        displaySchedule: isCommonSchedules.value ? run.scheduleDisplay : run.schedule,
        displayTime: settings.relativeTime ? relative : run.datetime,
        detailTime: settings.relativeTime ? run.datetime : relative,
      }
    })
})

const MAX_CELL_RUNS = { month: 3, week: 6 }

function makeDateStr(y, m, d) {
  return `${y}-${String(m + 1).padStart(2, '0')}-${String(d).padStart(2, '0')}`
}

function buildCellRuns(dateStr) {
  const all = runsByDate.value[dateStr] ?? []
  const max = MAX_CELL_RUNS[viewMode.value] ?? all.length
  return {
    runs: all.slice(0, max),
    overflowCount: Math.max(0, all.length - max),
  }
}

const calendarCells = computed(() => {
  const today = startOfToday()
  const todayStr = makeDateStr(today.getFullYear(), today.getMonth(), today.getDate())

  if (viewMode.value === 'week') {
    return DAY_ABBR.value.map((_, i) => {
      const day = new Date(viewAnchor.value)
      day.setDate(day.getDate() + i)
      const dateStr = makeDateStr(day.getFullYear(), day.getMonth(), day.getDate())
      const { runs, overflowCount } = buildCellRuns(dateStr)
      return {
        day: day.getDate(),
        dateStr,
        isToday: dateStr === todayStr,
        isPast: dateStr < todayStr,
        runs,
        overflowCount,
      }
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
    const { runs, overflowCount } = buildCellRuns(dateStr)
    cells.push({
      day: d,
      dateStr,
      isToday: dateStr === todayStr,
      isPast: dateStr < todayStr,
      runs,
      overflowCount,
    })
  }
  while (cells.length % 7 !== 0) cells.push({ day: 0, runs: [] })
  return cells
})

function scheduleColor(name) {
  const text = String(name ?? '')
  let h = 0
  for (let i = 0; i < text.length; i++) h = Math.imul(31, h) + text.charCodeAt(i) | 0
  const unsigned = h >>> 0
  const hue = unsigned % 360
  const saturation = 60 + ((unsigned >>> 9) % 20)
  const lightness = 35 + ((unsigned >>> 17) % 20)
  return `hsl(${hue} ${saturation}% ${lightness}%)`
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
  refreshUpcomingPreview()
})

onMounted(() => {
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  refreshUpcomingPreview()
})
</script>

<style scoped>
.schedules-list-stats {
  flex-wrap: wrap;
}

.schedules-list-stats :deep(.q-chip) {
  font-weight: 600;
}

.sched-next-runs {
  flex-wrap: wrap;
}

.sched-group-header td {
  background: rgba(0, 0, 0, 0.04);
  border-top: 2px solid rgba(0, 0, 0, 0.15) !important;
  padding-top: 4px !important;
  padding-bottom: 4px !important;
}

.sched-view-toggle {
  border: 1px solid rgba(255, 255, 255, 0.55);
  border-radius: 6px;
  overflow: hidden;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.12);
}

.sched-view-toggle :deep(.q-btn) {
  min-width: 5.25rem;
  padding-inline: 0.75rem;
  font-weight: 600;
}

.sched-view-toggle :deep(.q-btn + .q-btn) {
  border-left: 1px solid rgba(21, 101, 192, 0.16);
}

:deep(.sched-view-toggle .q-btn:focus-visible) {
  outline: 2px solid rgba(0, 0, 0, 0.55);
  outline-offset: 2px;
}

.sched-period-nav {
  gap: 0.125rem;
}

.sched-period-label {
  display: inline-block;
  min-width: 9rem;
  margin: 0;
  padding: 0 0.125rem;
  text-align: center;
  white-space: nowrap;
}
</style>
