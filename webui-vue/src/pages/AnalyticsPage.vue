<template>
  <div class="q-pa-sm" :class="{ 'analytics-widget': isDashboardWidget }">
    <DirectorErrorsBanner v-if="section === 'summary'" :errors="directorErrors" />
    <q-banner v-if="section === 'summary' && error" dense class="bg-negative text-white q-mb-md">
      {{ error }}
    </q-banner>

    <div v-if="section === 'summary'" class="row q-gutter-md items-stretch wrap analytics-summary-stats">
      <div v-for="s in overallStats" :key="s.label" class="col-auto">
        <div class="text-caption text-grey-6" style="white-space:nowrap">{{ s.label }}</div>
        <router-link
          v-if="s.jobsQuery !== null"
          :to="{ name: 'jobs', query: s.jobsQuery }"
          style="color: inherit; text-decoration: none"
        >
          <div class="text-h6 text-weight-bold" :class="'text-' + s.color" style="line-height:1.2">{{ s.value }}</div>
        </router-link>
        <div v-else class="text-h6 text-weight-bold" :class="'text-' + s.color" style="line-height:1.2">{{ s.value }}</div>
      </div>
    </div>

    <div v-if="section === 'treemap'" class="analytics-fill">
      <div ref="treemapEl" style="position:relative;width:100%;height:100%;overflow:hidden">
        <q-btn-toggle
          v-model="treemapMode" flat no-caps dense size="sm"
          toggle-color="primary" class="analytics-treemap-toggle"
          :options="[{ label: t('Bytes'), value: 'bytes' }, { label: t('Files'), value: 'files' }]"
        />
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

    <div v-if="section === 'status'" class="column items-center justify-center analytics-fill" style="padding:8px; box-sizing:border-box">
      <div v-if="!statusChartData.labels.length" class="text-grey text-caption text-center">
        {{ t('No data') }}
      </div>
      <div v-else style="position:relative; width:100%; flex:1; min-height:0">
        <Doughnut :data="statusChartData" :options="statusChartOptions" />
      </div>
    </div>

    <div v-if="section === 'client-bytes'" class="analytics-fill">
      <div ref="clientTreemapEl" style="position:relative;width:100%;height:100%;overflow:hidden">
        <q-btn-toggle
          v-model="clientTreemapMode" flat no-caps dense size="sm"
          toggle-color="primary" class="analytics-treemap-toggle"
          :options="[{ label: t('Bytes'), value: 'bytes' }, { label: t('Files'), value: 'files' }]"
        />
        <div v-if="loading && !clientTreemapTiles.length" class="flex flex-center" style="height:100%">
          <q-spinner size="40px" color="primary" />
        </div>
        <template v-else>
          <router-link
            v-for="tile in clientTreemapTiles" :key="tile.name"
            :to="{ name: 'jobs', query: tile.jobsQuery }"
            :style="tile.style"
            style="position:absolute;overflow:hidden;box-sizing:border-box;border:2px solid white;border-radius:4px;transition:opacity .2s;color:inherit;text-decoration:none"
            :title="`${tile.name}\n${fmtBytes(tile.bytes)} · ${formatFileCount(tile.files)}`">
            <div style="padding:4px 6px;height:100%;display:flex;flex-direction:column;justify-content:center">
              <div class="text-white text-weight-bold" style="font-size:11px;line-height:1.2;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">
                {{ tile.name }}
              </div>
              <div v-if="tile.h > 36" class="text-white" style="font-size:10px;opacity:.85">
                {{ clientTreemapMode === 'bytes' ? fmtBytes(tile.bytes) : formatFileCount(tile.files) }}
              </div>
            </div>
          </router-link>
          <div v-if="!clientTreemapTiles.length" class="flex flex-center text-grey" style="height:100%">
            <span>{{ t('No data') }}</span>
          </div>
        </template>
        <div v-if="loading && clientTreemapTiles.length" class="analytics-refresh-indicator">
          <q-spinner size="14px" color="primary" />
        </div>
      </div>
    </div>

    <div v-if="section === 'level-distribution'" class="column items-center justify-center analytics-fill" style="padding:8px; box-sizing:border-box">
      <div v-if="!levelChartData.labels.length" class="text-grey text-caption text-center">
        {{ t('No data') }}
      </div>
      <div v-else style="position:relative; width:100%; flex:1; min-height:0">
        <Doughnut :data="levelChartData" :options="levelChartOptions" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, inject, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRouter } from 'vue-router'
import { Doughnut } from 'vue-chartjs'
import {
  Chart as ChartJS,
  ArcElement,
  Tooltip,
  Legend,
} from 'chart.js'
import ChartDataLabels from 'chartjs-plugin-datalabels'
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
import { getContrastTextColor } from '../dashboard/piePalette.js'
import { CenterTextPlugin } from '../dashboard/centerTextPlugin.js'

ChartJS.register(ArcElement, Tooltip, Legend, CenterTextPlugin, ChartDataLabels)

const props = defineProps({
  widgetProps: { type: Object, default: () => ({}) },
})
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const fmtBytes = formatBytes
const { t } = useI18n()
const router = useRouter()
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
  const clientCount = dashboardContext?.aggregate?.value?.clientCount
  const storageCount = dashboardContext?.aggregate?.value?.storageCount
  const stats = [
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
  if (clientCount !== undefined) {
    stats.push({ label: t('Clients'), value: clientCount, color: 'purple-7', jobsQuery: null })
  }
  if (storageCount !== undefined) {
    stats.push({ label: t('Storages'), value: storageCount, color: 'indigo-7', jobsQuery: null })
  }
  return stats
})

const statusRows = computed(() => {
  const j = jobs.value
  const count = code => j.filter(x => x.status === code).length
  return [
    {
      label: t('Successful'),
      color: '#2eb87a',
      count: count('T'),
      jobsQuery: withJobsStatusFilterQuery({}, 'T'),
    },
    {
      label: t('Warning'),
      color: '#b8b82e',
      count: count('W'),
      jobsQuery: withJobsStatusFilterQuery({}, 'W'),
    },
    {
      label: t('Failed'),
      color: '#b82e2e',
      count: count('f') + count('E'),
      jobsQuery: withJobsStatusFilterQuery({}, ['f', 'E']),
    },
    {
      label: t('Canceled'),
      color: '#888888',
      count: count('A'),
      jobsQuery: withJobsStatusFilterQuery({}, 'A'),
    },
    {
      label: t('Running'),
      color: '#0075be',
      count: count('R'),
      jobsQuery: withJobsStatusFilterQuery({}, 'R'),
    },
  ]
})

const statusRowsNonEmpty = computed(() => statusRows.value.filter(r => r.count > 0))
const totalStatusJobs = computed(() => statusRows.value.reduce((s, r) => s + r.count, 0))

const statusChartData = computed(() => ({
  labels: statusRowsNonEmpty.value.map(r => r.label),
  datasets: [{
    data: statusRowsNonEmpty.value.map(r => r.count),
    backgroundColor: statusRowsNonEmpty.value.map(r => r.color),
    borderWidth: 1,
  }],
}))

function handleStatusNavigation(index) {
  const row = statusRowsNonEmpty.value[index]
  if (!row) return
  void router.push({ name: 'jobs', query: row.jobsQuery })
}

const statusChartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  cutout: '65%',
  onClick(_event, elements) {
    handleStatusNavigation(elements?.[0]?.index)
  },
  plugins: {
    legend: {
      position: 'bottom',
      labels: { font: { size: 11 }, boxWidth: 12 },
      onClick(_event, legendItem) {
        handleStatusNavigation(legendItem.index)
      },
    },
    tooltip: {
      callbacks: {
        label(context) {
          return ` ${context.label}: ${context.raw}`
        },
      },
    },
    centerText: {
      lines: [String(totalStatusJobs.value), t('Total')],
      fonts: ['600 15px sans-serif', '11px sans-serif'],
      colors: ['#333', '#888'],
    },
    datalabels: {
      color: (context) => getContrastTextColor(
        statusRowsNonEmpty.value[context.dataIndex]?.color ?? '#0075be'
      ),
      font: { weight: 'bold', size: 11 },
      formatter: (value, context) => {
        const total = context.dataset.data.reduce((a, b) => a + b, 0)
        const percent = total > 0 ? (value / total) * 100 : 0
        return percent >= 5 ? `${percent.toFixed(0)}%` : ''
      },
    },
  },
}))

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
        files: 0,
        jobsQuery: withJobsSearchQuery({}, j.client),
      }
    }
    map[label].bytes += j.bytes
    map[label].files += j.files
  }
  return Object.values(map)
    .sort((a, b) => b.bytes - a.bytes)
    .slice(0, 12)
    .map((c, i) => ({ ...c, color: PALETTE[i % PALETTE.length] }))
})

const clientTreemapMode = ref('bytes')
const clientTreemapEl = ref(null)
const clientTreemapW = ref(600)
const clientTreemapH = ref(280)

const clientTreemapTiles = computed(() => {
  const W = clientTreemapW.value
  const H = clientTreemapH.value
  if (!W || !H) return []
  const groups = clientBytes.value.filter(c => c[clientTreemapMode.value] > 0)
  if (!groups.length) return []
  const items = groups.map(g => ({ ...g, value: g[clientTreemapMode.value] }))
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

const levelDist = computed(() => {
  const j = jobs.value
  const count = code => j.filter(x => x.level === code).length
  return [
    { label: t('Full'), color: '#0075be', count: count('F') },
    { label: t('Incremental'), color: '#2eb8b8', count: count('I') },
    { label: t('Differential'), color: '#6c2eb8', count: count('D') },
  ]
})

const levelDistNonEmpty = computed(() => levelDist.value.filter(l => l.count > 0))
const totalLevelJobs = computed(() => levelDist.value.reduce((s, l) => s + l.count, 0))

const levelChartData = computed(() => ({
  labels: levelDistNonEmpty.value.map(l => l.label),
  datasets: [{
    data: levelDistNonEmpty.value.map(l => l.count),
    backgroundColor: levelDistNonEmpty.value.map(l => l.color),
    borderWidth: 1,
  }],
}))

const levelChartOptions = computed(() => ({
  responsive: true,
  maintainAspectRatio: false,
  cutout: '65%',
  plugins: {
    legend: {
      position: 'bottom',
      labels: { font: { size: 11 }, boxWidth: 12 },
    },
    tooltip: {
      callbacks: {
        label(context) {
          return ` ${context.label}: ${context.raw}`
        },
      },
    },
    centerText: {
      lines: [String(totalLevelJobs.value), t('Total')],
      fonts: ['600 15px sans-serif', '11px sans-serif'],
      colors: ['#333', '#888'],
    },
    datalabels: {
      color: (context) => getContrastTextColor(
        levelDistNonEmpty.value[context.dataIndex]?.color ?? '#0075be'
      ),
      font: { weight: 'bold', size: 11 },
      formatter: (value, context) => {
        const total = context.dataset.data.reduce((a, b) => a + b, 0)
        const percent = total > 0 ? (value / total) * 100 : 0
        return percent >= 5 ? `${percent.toFixed(0)}%` : ''
      },
    },
  },
}))

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
  if (treemapEl.value) {
    const ro = new ResizeObserver(entries => {
      for (const e of entries) {
        treemapW.value = e.contentRect.width
        treemapH.value = e.contentRect.height
      }
    })
    ro.observe(treemapEl.value)
    onUnmounted(() => ro.disconnect())
  }

  if (clientTreemapEl.value) {
    const ro = new ResizeObserver(entries => {
      for (const e of entries) {
        clientTreemapW.value = e.contentRect.width
        clientTreemapH.value = e.contentRect.height
      }
    })
    ro.observe(clientTreemapEl.value)
    onUnmounted(() => ro.disconnect())
  }
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

.analytics-treemap-toggle {
  position: absolute;
  top: 4px;
  left: 4px;
  z-index: 2;
  background: rgba(255, 255, 255, 0.88);
  border-radius: 4px;
}
</style>
