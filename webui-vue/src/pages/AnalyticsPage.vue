<template>
  <div class="q-pa-sm" :class="{ 'analytics-widget': isDashboardWidget }">
    <DirectorErrorsBanner v-if="section === 'summary'" :errors="directorErrors" />
    <q-banner v-if="section === 'summary' && error" dense class="bg-negative text-white q-mb-md">
      {{ error }}
    </q-banner>

    <div v-if="section === 'summary'" class="row q-col-gutter-md">
      <div class="col-6 col-sm-3 col-md-2" v-for="s in overallStats" :key="s.label">
        <q-card flat bordered class="bareos-panel text-center">
          <q-card-section class="q-py-sm">
            <div class="text-caption text-grey-6">{{ s.label }}</div>
            <router-link
              v-if="s.jobsQuery !== null"
              :to="{ name: 'jobs', query: s.jobsQuery }"
              style="color: inherit; text-decoration: none"
            >
              <div class="text-h6 text-weight-bold" :class="'text-' + s.color">{{ s.value }}</div>
            </router-link>
            <div v-else class="text-h6 text-weight-bold" :class="'text-' + s.color">{{ s.value }}</div>
          </q-card-section>
        </q-card>
      </div>
    </div>

    <div v-if="section === 'treemap'" class="analytics-fill">
      <div ref="treemapEl" style="position:relative;width:100%;height:100%;overflow:hidden">
        <div v-if="loading && !treemapTiles.length" class="flex flex-center" style="height:100%">
          <q-spinner size="40px" color="primary" />
        </div>
        <template v-else>
          <component
            :is="tile.jobsQuery !== null ? 'router-link' : 'div'"
            v-for="tile in treemapTiles" :key="tile.name"
            :to="tile.jobsQuery !== null ? { name: 'jobs', query: tile.jobsQuery } : undefined"
            :style="tile.style"
            style="position:absolute;overflow:hidden;box-sizing:border-box;border:2px solid white;border-radius:4px;transition:opacity .2s;color:inherit;text-decoration:none"
            :title="`${tile.name}\n${fmtBytes(tile.bytes)} · ${formatFileCount(tile.files)}`">
            <div style="padding:4px 6px;height:100%;display:flex;flex-direction:column;justify-content:center">
              <div class="text-white text-weight-bold" style="font-size:11px;line-height:1.2;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">
                {{ tile.name }}
              </div>
              <div v-if="tile.h > 36" class="text-white" style="font-size:10px;opacity:.85">
                {{ treemapMode === 'bytes' ? fmtBytes(tile.bytes) : formatFileCount(tile.files) }}
              </div>
            </div>
          </component>
          <div v-if="!treemapTiles.length" class="flex flex-center text-grey" style="height:100%">
            <span>{{ t('No data') }}</span>
          </div>
        </template>
        <div v-if="loading && treemapTiles.length" class="analytics-refresh-indicator">
          <q-spinner size="14px" color="primary" />
        </div>
      </div>
    </div>

    <q-table
      v-if="section === 'status'"
      :rows="statusRows"
      :columns="statusCols"
      row-key="label"
      dense
      flat
      hide-bottom
      :pagination="{ rowsPerPage: 0 }"
      :loading="loading"
    >
      <template #body-cell-bar="tableProps">
        <q-td :props="tableProps" style="width:200px">
          <q-linear-progress :value="tableProps.row.count / maxStatusCount || 0"
            :color="tableProps.row.color" track-color="grey-2" size="12px" rounded />
        </q-td>
      </template>
      <template #body-cell-label="tableProps">
        <q-td :props="tableProps">
          <router-link
            v-if="tableProps.row.jobsQuery !== null"
            :to="{ name: 'jobs', query: tableProps.row.jobsQuery }"
            style="text-decoration: none"
          >
            <q-badge :color="tableProps.row.color" :label="tableProps.row.label" />
          </router-link>
          <q-badge v-else :color="tableProps.row.color" :label="tableProps.row.label" />
        </q-td>
      </template>
    </q-table>

    <div v-if="section === 'client-bytes'" class="q-pa-sm q-gutter-xs analytics-fill" style="position:relative">
            <div v-if="loading && !clientBytes.length" class="text-center q-py-md">
              <q-spinner size="32px" color="primary" />
            </div>
            <template v-else>
              <div v-for="c in clientBytes" :key="c.name" class="q-mb-xs">
                <div class="row items-center q-mb-xs" style="gap:4px">
                  <router-link
                    :to="{ name: 'jobs', query: c.jobsQuery }"
                    class="text-caption ellipsis"
                    :style="{ width: '110px', minWidth: 0, textDecoration: 'none', color: c.color }"
                    :title="c.name"
                  >{{ c.name }}</router-link>
                  <q-linear-progress :value="bytesGauge(c.bytes)" track-color="grey-3"
                                     :style="{ flex: 1, color: c.color }" size="10px" rounded />
                  <span class="text-caption text-grey-6" style="width:60px;text-align:right">{{ fmtBytes(c.bytes) }}</span>
                </div>
              </div>
              <div v-if="!clientBytes.length" class="text-grey text-caption text-center q-py-md">{{ t('No data') }}</div>
            </template>
            <div v-if="loading && clientBytes.length" class="analytics-refresh-indicator">
              <q-spinner size="14px" color="primary" />
            </div>
    </div>

    <div v-if="section === 'level-distribution'" class="q-pa-sm analytics-fill">
            <div v-for="l in levelDist" :key="l.label" class="q-mb-sm">
              <div class="row items-center q-mb-xs" style="gap:6px">
                <span class="text-caption" style="width:90px">{{ l.label }}</span>
                <q-linear-progress :value="l.count / totalJobs || 0"
                  :color="l.color" track-color="grey-3" size="10px" rounded style="flex:1" />
                <span class="text-caption text-grey-6" style="width:30px;text-align:right">{{ l.count }}</span>
              </div>
            </div>
    </div>
  </div>
</template>

<script setup>
import { computed, inject, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { fetchAggregatedAnalytics } from '../composables/analyticsAggregate.js'
import { directorCollection, normaliseJob } from '../composables/useDirectorFetch.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { formatBytes } from '../mock/index.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  withJobsSearchQuery,
  withJobsStatusFilterQuery,
} from '../utils/jobs.js'
import { formatNumber } from '../utils/locales.js'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import { DASHBOARD_CONTEXT_KEY } from '../dashboard/dashboardContext.js'

const props = defineProps({
  widgetProps: { type: Object, default: () => ({}) },
})
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const fmtBytes = formatBytes
const { t } = useI18n()
const treemapMode = ref('bytes')
const treemapEl = ref(null)
const treemapW = ref(600)
const treemapH = ref(280)
const rawJobs = ref([])
const loadingLocal = ref(false)
const error = ref(null)
const directorErrors = ref([])
const dashboardContext = inject(DASHBOARD_CONTEXT_KEY, null)
const isDashboardWidget = computed(() => dashboardContext !== null)
const section = computed(() => props.widgetProps.section ?? ({
  'analytics-summary': 'summary',
  'analytics-treemap': 'treemap',
  'analytics-status-breakdown': 'status',
  'analytics-client-bytes': 'client-bytes',
  'analytics-level-distribution': 'level-distribution',
}[props.widgetProps.type] ?? 'summary'))

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonAnalytics,
  scopeLabel: analyticsScopeLabel,
  syncSelectedDirectors,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

function formatFileCount(count) {
  return `${formatNumber(count ?? 0, settings.locale)} ${t('files')}`
}

async function refresh() {
  loadingLocal.value = true
  error.value = null
  directorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      rawJobs.value = []
      return
    }

    if (isCommonAnalytics.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedAnalytics(credentials, activeDirectors.value)
      rawJobs.value = result.jobs
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const result = await director.call('list jobs')
    rawJobs.value = directorCollection(result?.jobs).map((job) => ({
      ...normaliseJob(job),
      director: currentDirector,
      scopeKey: `${currentDirector}:${normaliseJob(job).id}`,
    }))
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  } finally {
    loadingLocal.value = false
  }
}

const jobs = computed(() => dashboardContext
  ? directorCollection(dashboardContext.analyticsJobs.value)
  : directorCollection(rawJobs.value))
const loading = computed(() => dashboardContext
  ? dashboardContext.analyticsLoading.value
  : loadingLocal.value)
const totalJobs = computed(() => jobs.value.length || 1)

const overallStats = computed(() => {
  const j = jobs.value
  return [
    { label: t('Total Jobs'), value: j.length, color: 'primary', jobsQuery: {} },
    {
      label: t('Successful'),
      value: j.filter(x => x.status === 'T').length,
      color: 'positive',
      jobsQuery: withJobsStatusFilterQuery({}, 'T'),
    },
    {
      label: t('Warning'),
      value: j.filter(x => x.status === 'W').length,
      color: 'warning',
      jobsQuery: withJobsStatusFilterQuery({}, 'W'),
    },
    {
      label: t('Failed'),
      value: j.filter(x => x.status === 'f' || x.status === 'E').length,
      color: 'negative',
      jobsQuery: withJobsStatusFilterQuery({}, ['f', 'E']),
    },
    {
      label: t('Total Bytes'),
      value: fmtBytes(j.reduce((a, x) => a + x.bytes, 0)),
      color: 'blue-7',
      jobsQuery: null,
    },
    {
      label: t('Total Files'),
      value: formatNumber(j.reduce((a, x) => a + x.files, 0), settings.locale),
      color: 'teal-7',
      jobsQuery: null,
    },
  ]
})

const statusRows = computed(() => {
  const j = jobs.value
  const count = code => j.filter(x => x.status === code).length
  return [
    {
      label: t('Successful'),
      color: 'positive',
      count: count('T'),
      jobsQuery: withJobsStatusFilterQuery({}, 'T'),
    },
    {
      label: t('Warning'),
      color: 'warning',
      count: count('W'),
      jobsQuery: withJobsStatusFilterQuery({}, 'W'),
    },
    {
      label: t('Failed'),
      color: 'negative',
      count: count('f') + count('E'),
      jobsQuery: withJobsStatusFilterQuery({}, ['f', 'E']),
    },
    {
      label: t('Canceled'),
      color: 'grey',
      count: count('A'),
      jobsQuery: withJobsStatusFilterQuery({}, 'A'),
    },
    {
      label: t('Running'),
      color: 'info',
      count: count('R'),
      jobsQuery: withJobsStatusFilterQuery({}, 'R'),
    },
  ]
})
const maxStatusCount = computed(() => Math.max(1, ...statusRows.value.map(r => r.count)))
const statusCols = [
  { name: 'label', label: 'Status', field: 'label', align: 'left', style: 'width:100px', sortable: true },
  { name: 'bar', label: '', field: 'bar', align: 'left' },
  { name: 'count', label: '#', field: 'count', align: 'right', style: 'width:50px', sortable: true },
].map(col => ({ ...col, label: col.label ? t(col.label) : col.label }))

function prefixedLabel(directorName, baseName) {
  return isCommonAnalytics.value ? `${directorName} / ${baseName}` : baseName
}

const PALETTE = ['#1976D2', '#388E3C', '#F57C00', '#7B1FA2', '#C62828',
  '#00838F', '#558B2F', '#6D4C41', '#455A64', '#E91E63',
  '#0277BD', '#2E7D32', '#EF6C00', '#6A1B9A', '#AD1457']

const clientBytes = computed(() => {
  const map = {}
  for (const j of jobs.value) {
    if (!j.client) continue
    const label = prefixedLabel(j.director, j.client)
    if (!map[label]) {
      map[label] = {
        name: label,
        bytes: 0,
        jobsQuery: withJobsSearchQuery({}, j.client),
      }
    }
    map[label].bytes += j.bytes
  }
  return Object.values(map)
    .sort((a, b) => b.bytes - a.bytes)
    .slice(0, 12)
    .map((c, i) => ({ ...c, color: PALETTE[i % PALETTE.length] }))
})

const maxBytes = computed(() => Math.max(1, ...clientBytes.value.map(c => c.bytes)))
function bytesGauge(val) { return val / maxBytes.value }

const levelDist = computed(() => {
  const j = jobs.value
  const count = code => j.filter(x => x.level === code).length
  return [
    { label: t('Full'), color: 'primary', count: count('F') },
    { label: t('Incremental'), color: 'teal', count: count('I') },
    { label: t('Differential'), color: 'purple', count: count('D') },
  ]
})

const jobGroups = computed(() => {
  const map = {}
  for (const j of jobs.value) {
    if (j.type === 'R') continue
    const label = prefixedLabel(j.director, j.name)
    if (!map[label]) {
      map[label] = {
        name: label,
        bytes: 0,
        files: 0,
        jobsQuery: withJobsSearchQuery({}, j.name),
      }
    }
    map[label].bytes += j.bytes
    map[label].files += j.files
  }
  return Object.values(map).sort((a, b) => b.bytes - a.bytes)
})

function squarify(items, x0, y0, x1, y1) {
  if (!items.length) return []
  if (items.length === 1) {
    return [{ ...items[0], x: x0, y: y0, w: x1 - x0, h: y1 - y0 }]
  }
  const W = x1 - x0
  const H = y1 - y0
  const total = items.reduce((s, i) => s + i.value, 0)
  let bestAspect = Infinity
  let bestSplit = 1
  let cumFrac = 0
  for (let i = 0; i < items.length - 1; i++) {
    cumFrac += items[i].value / total
    const aW = W > H ? cumFrac * W : W
    const aH = W > H ? H : cumFrac * H
    const tileW = W > H ? aW : aW * (items[i].value / (cumFrac * total))
    const tileH = W > H ? aH * (items[i].value / (cumFrac * total)) : aH
    const aspect = Math.max(tileW / Math.max(tileH, 0.001), Math.max(tileH, 0.001) / tileW)
    if (aspect < bestAspect) {
      bestAspect = aspect
      bestSplit = i + 1
    }
  }
  const leftItems = items.slice(0, bestSplit)
  const rightItems = items.slice(bestSplit)
  const leftFrac = leftItems.reduce((s, i) => s + i.value, 0) / total
  if (W >= H) {
    return [
      ...squarify(leftItems, x0, y0, x0 + W * leftFrac, y1),
      ...squarify(rightItems, x0 + W * leftFrac, y0, x1, y1),
    ]
  }
  return [
    ...squarify(leftItems, x0, y0, x1, y0 + H * leftFrac),
    ...squarify(rightItems, x0, y0 + H * leftFrac, x1, y1),
  ]
}

const treemapTiles = computed(() => {
  const W = treemapW.value
  const H = treemapH.value
  if (!W || !H) return []
  const groups = jobGroups.value.filter(g => g[treemapMode.value] > 0)
  if (!groups.length) return []
  const items = groups.map((g, i) => ({
    ...g,
    value: g[treemapMode.value],
    color: PALETTE[i % PALETTE.length],
  }))
  const tiles = squarify(items, 0, 0, W, H)
  return tiles.map(t => ({
    name: t.name,
    bytes: t.bytes,
    files: t.files,
    h: t.h,
    jobsQuery: t.jobsQuery,
    style: {
      left: `${t.x}px`,
      top: `${t.y}px`,
      width: `${t.w}px`,
      height: `${t.h}px`,
      backgroundColor: t.color,
    },
  }))
})

onMounted(() => {
  if (!dashboardContext) {
    director.fetchAvailableDirectors().catch(() => {})
    syncSelectedDirectors()
    refresh()
  }

  // The ResizeObserver must run in both standalone and dashboard-widget
  // mode: it sizes the treemap tiles to the widget's actual rendered
  // dimensions instead of the hardcoded fallback size.
  if (!treemapEl.value) return
  const ro = new ResizeObserver(entries => {
    for (const e of entries) {
      treemapW.value = e.contentRect.width
      treemapH.value = e.contentRect.height
    }
  })
  ro.observe(treemapEl.value)
  onUnmounted(() => ro.disconnect())
})

watch(() => directorOptions.value, () => {
  if (dashboardContext) return
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  if (dashboardContext) return
  refresh()
})
</script>

<style scoped>
.analytics-widget {
  height: 100%;
  display: flex;
  flex-direction: column;
}

.analytics-fill {
  height: 100%;
}

/* Inside a dashboard widget the outer element is a flex column with a
   definite height (see WidgetShell); make the section fill it via flex
   instead of relying on a percentage-height chain, so absolutely
   positioned content (the treemap tiles) isn't clipped to zero height. */
.analytics-widget .analytics-fill {
  flex: 1;
  min-height: 0;
}

.analytics-refresh-indicator {
  position: absolute;
  top: 6px;
  right: 6px;
  z-index: 2;
  line-height: 0;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.82);
  padding: 3px;
}
</style>
