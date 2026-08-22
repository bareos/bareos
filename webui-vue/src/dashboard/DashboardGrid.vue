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

<!--
  DashboardGrid
  Renders a grid-layout-plus drag/resize grid for one dashboard.
  Provides the dashboard data context via Vue provide/inject to all child widgets.
-->
<template>
  <div
    ref="gridRoot"
    class="dashboard-grid-root"
    :class="{ 'grid-interacting': interactionActive }"
  >
    <!-- Empty state -->
    <div
      v-if="!dashboard.widgets.length"
      class="column items-center justify-center text-grey q-py-xl"
      style="min-height:200px"
    >
      <q-icon name="mdi-view-dashboard-outline" size="48px" class="q-mb-md" />
      <div class="text-body1">{{ t('This dashboard has no widgets yet.') }}</div>
      <div v-if="editMode" class="text-caption q-mt-xs">
        {{ t('Click "Add Widget" to get started.') }}
      </div>
    </div>

    <!-- Mobile: simple vertical stack (no drag/resize) -->
    <div
      v-if="dashboard.widgets.length && isMobile && !editMode"
      class="column q-gutter-sm q-px-sm"
    >
      <WidgetShell
        v-for="widget in dashboard.widgets"
        :key="widget.id"
        :title="widget.title || defaultTitle(widget.type)"
        :edit-mode="editMode"
        style="min-height:200px"
        @configure="openConfig(widget)"
        @remove="removeWidget(widget)"
      >
        <component
          :is="resolveWidgetComponent(widget.type)"
          :widget-props="widget.props"
        />
      </WidgetShell>
    </div>

    <!-- Full drag/resize grid (desktop + mobile edit mode) -->
    <GridLayout
      v-else-if="dashboard.widgets.length"
      :layout="layout"
      :col-num="12"
      :row-height="30"
      :margin="[8, 8]"
      :is-draggable="editMode && !isMobile"
      :is-resizable="editMode"
      :use-css-transforms="true"
      :vertical-compact="true"
      drag-allow-from=".widget-drag-handle"
      @layout-updated="onLayoutUpdated"
    >
      <GridItem
        v-for="widget in dashboard.widgets"
        :key="widget.id"
        :i="widget.id"
        :x="layoutMap[widget.id]?.x ?? widget.layout.x"
        :y="layoutMap[widget.id]?.y ?? widget.layout.y"
        :w="layoutMap[widget.id]?.w ?? widget.layout.w"
        :h="layoutMap[widget.id]?.h ?? widget.layout.h"
        :min-w="widget.layout.minW ?? 2"
        :min-h="widget.layout.minH ?? 3"
        @resize="onItemResize"
        @resized="onItemResized"
        @move="onItemMove"
        @moved="onItemMoved"
      >
        <WidgetShell
          :title="widget.title || defaultTitle(widget.type)"
          :edit-mode="editMode"
          style="height:100%"
          @configure="openConfig(widget)"
          @remove="removeWidget(widget)"
        >
          <component
            :is="resolveWidgetComponent(widget.type)"
            :widget-props="widget.props"
          />
        </WidgetShell>
      </GridItem>
    </GridLayout>

    <!-- Configure dialog -->
    <WidgetConfigDialog
      v-if="configuringWidget"
      v-model="showConfigDialog"
      :widget="configuringWidget"
      :required-only="false"
      :active-directors="activeDirectors"
      @save="onConfigSave"
    />
  </div>
</template>

<script setup>
import {
  ref, computed, provide, watch, reactive, nextTick, onMounted, onUnmounted,
} from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { GridLayout, GridItem } from 'grid-layout-plus'
import { useDashboardStore } from '../stores/dashboards.js'
import { useAuthStore } from '../stores/auth.js'
import { getWidgetDefinition } from './widgetRegistry.js'
import { DASHBOARD_CONTEXT_KEY } from './dashboardContext.js'
import {
  aggregateDirectorDashboardSnapshots,
  fetchDirectorDashboardSnapshot,
} from '../composables/directorAggregate.js'
import WidgetShell from './WidgetShell.vue'
import WidgetConfigDialog from './WidgetConfigDialog.vue'

const props = defineProps({
  dashboard:       { type: Object,  required: true },
  editMode:        { type: Boolean, default: false },
  activeDirectors: { type: Array,   default: () => [] },
  directorOptions: { type: Array,   default: () => [] },
})

const { t } = useI18n()
const $q = useQuasar()
const dashboardStore = useDashboardStore()
const auth = useAuthStore()
const gridRoot = ref(null)
const interactionActive = ref(false)

// Switch to simple vertical stack below Quasar's 'sm' breakpoint (600 px).
const isMobile = computed(() => $q.screen.lt.sm)

// ── snapshot data ──────────────────────────────────────────────────────────
const snapshots    = ref([])
const loading      = ref(false)
const refreshToken = ref(0)
const pools        = ref([])
const poolLoading  = ref(false)
const poolRefreshToken = ref(0)
let _latestFetchRequestId = 0
let _dataRequestsInFlight = 0
let _poolRequestsInFlight = 0

// Pool data changes infrequently — only re-fetch every POOL_REFRESH_EVERY
// normal refresh cycles (≈ every 10 minutes at the default 60 s interval).
const POOL_REFRESH_EVERY = 10
let _refreshCount = 0

const aggregate = computed(() => aggregateDirectorDashboardSnapshots(snapshots.value))

async function fetchData({ forcePools = false } = {}) {
  const requestId = ++_latestFetchRequestId
  const credentials = auth.getCredentials()
  if (!credentials || props.activeDirectors.length === 0) {
    if (requestId === _latestFetchRequestId) {
      snapshots.value = []
      pools.value = []
    }
    return
  }

  _refreshCount += 1
  const includePools = forcePools || _refreshCount % POOL_REFRESH_EVERY === 1

  _dataRequestsInFlight += 1
  loading.value = true
  if (includePools) {
    _poolRequestsInFlight += 1
    poolLoading.value = true
  }
  try {
    const results = await Promise.allSettled(
      props.activeDirectors.map(d =>
        fetchDirectorDashboardSnapshot({ ...credentials, director: d }, { includePools })
      )
    )
    const fresh = results
      .filter(r => r.status === 'fulfilled')
      .map(r => r.value)

    // Ignore stale completions from older requests.
    if (requestId !== _latestFetchRequestId) return

    if (includePools) {
      snapshots.value = fresh
      pools.value = aggregateDirectorDashboardSnapshots(fresh).pools
      poolRefreshToken.value += 1
    } else {
      // Carry forward pool data from the previous snapshot for each director.
      const prevPoolsByDir = Object.fromEntries(
        snapshots.value.map(s => [s.director, s.pools ?? []])
      )
      snapshots.value = fresh.map(s => ({
        ...s,
        pools: prevPoolsByDir[s.director] ?? [],
      }))
    }
    refreshToken.value += 1
  } finally {
    _dataRequestsInFlight = Math.max(0, _dataRequestsInFlight - 1)
    loading.value = _dataRequestsInFlight > 0
    if (includePools) {
      _poolRequestsInFlight = Math.max(0, _poolRequestsInFlight - 1)
      poolLoading.value = _poolRequestsInFlight > 0
    }
  }
}

function refresh() { fetchData() }

watch(() => props.activeDirectors.join('\0'), () => fetchData({ forcePools: true }), { immediate: true })

provide(DASHBOARD_CONTEXT_KEY, {
  aggregate,
  pools,
  loading,
  poolLoading,
  refresh,
  refreshToken,
  poolRefreshToken,
  activeDirectors: computed(() => props.activeDirectors),
  directorOptions: computed(() => props.directorOptions),
})

// ── layout ─────────────────────────────────────────────────────────────────
// grid-layout-plus needs a flat array of { i, x, y, w, h } objects.
const layoutModel = ref([])

function toLayoutItems(widgets) {
  return widgets.map(w => ({
    i: String(w.id),
    x: w.layout.x,
    y: w.layout.y,
    w: w.layout.w,
    h: w.layout.h,
  }))
}

function isSameLayout(a, b) {
  if (a.length !== b.length) return false
  return a.every((item, idx) => {
    const other = b[idx]
    return other
      && String(item.i) === String(other.i)
      && item.x === other.x
      && item.y === other.y
      && item.w === other.w
      && item.h === other.h
  })
}

const layout = computed(() => layoutModel.value)

// Keep a reactive map so GridItem props stay in sync with drag/resize
const layoutMap = reactive({})

watch(
  () => props.dashboard.widgets,
  (widgets) => {
    layoutModel.value = toLayoutItems(widgets)
    widgets.forEach(w => {
      layoutMap[w.id] = { x: w.layout.x, y: w.layout.y, w: w.layout.w, h: w.layout.h }
    })
    // Remove stale entries
    const ids = new Set(widgets.map(w => w.id))
    for (const id of Object.keys(layoutMap)) {
      if (!ids.has(id)) delete layoutMap[id]
    }
  },
  { immediate: true, deep: true }
)

function onLayoutUpdated(newLayout) {
  if (!props.editMode) return
  const normalized = newLayout.map(item => ({
    i: String(item.i),
    x: item.x,
    y: item.y,
    w: item.w,
    h: item.h,
  }))
  if (isSameLayout(layoutModel.value, normalized)) return
  layoutModel.value = normalized
  normalized.forEach(item => {
    layoutMap[item.i] = { x: item.x, y: item.y, w: item.w, h: item.h }
  })
}

function updateDraftLayoutItem(id, patch) {
  if (!props.editMode) return
  const key = String(id)
  const current = layoutMap[key]
  if (!current) return
  layoutMap[key] = { ...current, ...patch }
  layoutModel.value = layoutModel.value.map(item =>
    String(item.i) === key ? { ...item, ...patch } : item
  )
}

function onItemResize(id, h, w) {
  interactionActive.value = true
  updateDraftLayoutItem(id, { h, w })
}

function onItemMove(id, x, y) {
  interactionActive.value = true
  updateDraftLayoutItem(id, { x, y })
}

function commitDraftLayout() {
  if (!props.editMode) return
  const normalized = layoutModel.value.map(item => ({
    i: String(item.i),
    x: item.x,
    y: item.y,
    w: item.w,
    h: item.h,
  }))
  const current = new Map(
    props.dashboard.widgets.map(w => [
      String(w.id),
      { x: w.layout.x, y: w.layout.y, w: w.layout.w, h: w.layout.h },
    ])
  )
  const unchanged =
    normalized.length === current.size
    && normalized.every(item => {
      const c = current.get(item.i)
      return c
        && c.x === item.x
        && c.y === item.y
        && c.w === item.w
        && c.h === item.h
    })
  if (unchanged) return
  dashboardStore.updateWidgetLayouts(props.dashboard.id, normalized)
}

function onItemResized(id, h, w) {
  updateDraftLayoutItem(id, { h, w })
  commitDraftLayout()
  interactionActive.value = false
  scheduleInteractionClassReset()
}

function onItemMoved(id, x, y) {
  updateDraftLayoutItem(id, { x, y })
  commitDraftLayout()
  interactionActive.value = false
  scheduleInteractionClassReset()
}

function resetInteractionClasses() {
  const root = gridRoot.value
  if (!root) return
  root.querySelectorAll('.vgl-item--resizing, .vgl-item--dragging').forEach(el => {
    el.classList.remove('vgl-item--resizing', 'vgl-item--dragging')
  })
}

function scheduleInteractionClassReset() {
  nextTick(() => {
    resetInteractionClasses()
  })
}

watch(
  () => props.editMode,
  (isEditMode) => {
    if (isEditMode) return
    interactionActive.value = false
    commitDraftLayout()
    scheduleInteractionClassReset()
  }
)

function onPointerRelease() {
  if (!props.editMode) return
  interactionActive.value = false
  scheduleInteractionClassReset()
}

onMounted(() => {
  window.addEventListener('mouseup', onPointerRelease, true)
  window.addEventListener('pointerup', onPointerRelease, true)
  window.addEventListener('touchend', onPointerRelease, true)
})

onUnmounted(() => {
  window.removeEventListener('mouseup', onPointerRelease, true)
  window.removeEventListener('pointerup', onPointerRelease, true)
  window.removeEventListener('touchend', onPointerRelease, true)
})

// ── widget helpers ─────────────────────────────────────────────────────────
function defaultTitle(type) {
  return getWidgetDefinition(type)?.defaultTitle ?? type
}

function resolveWidgetComponent(type) {
  return getWidgetDefinition(type)?.component ?? null
}

const configuringWidget = ref(null)
const showConfigDialog  = ref(false)

function openConfig(widget) {
  configuringWidget.value = widget
  showConfigDialog.value  = true
}

function onConfigSave({ title, props: newProps }) {
  if (!configuringWidget.value) return
  dashboardStore.updateWidgetTitle(props.dashboard.id, configuringWidget.value.id, title)
  dashboardStore.updateWidgetProps(props.dashboard.id, configuringWidget.value.id, newProps)
}

function removeWidget(widget) {
  $q.dialog({
    title: t('Remove Widget'),
    message: t('Remove "{title}" from this dashboard?', {
      title: widget.title || defaultTitle(widget.type),
    }),
    ok:     { label: t('Remove'), color: 'negative', flat: true },
    cancel: { label: t('Cancel'), flat: true },
  }).onOk(() => dashboardStore.removeWidget(props.dashboard.id, widget.id))
}

defineExpose({ refresh, fetchData, loading })
</script>

<style scoped>
.dashboard-grid-root :deep(.vgl-item--placeholder) {
  display: none !important;
}

.dashboard-grid-root.grid-interacting :deep(.vgl-item--placeholder) {
  display: block !important;
}

@media (max-width: 599px) {
  :deep(.vgl-layout) {
    --vgl-resizer-size: 18px;
    --vgl-resizer-border-width: 3px;
  }

  /* Keep page scrolling usable in mobile edit mode. */
  :deep(.vgl-item--no-touch) {
    touch-action: pan-y !important;
  }
}
</style>
