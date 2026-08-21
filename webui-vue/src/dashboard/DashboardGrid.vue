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
  <div>
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
      v-if="dashboard.widgets.length && isMobile"
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

    <!-- Desktop: full drag/resize grid -->
    <GridLayout
      v-else-if="dashboard.widgets.length"
      v-model:layout="layout"
      :col-num="12"
      :row-height="30"
      :margin="[8, 8]"
      :is-draggable="editMode"
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
import { ref, computed, provide, watch, reactive } from 'vue'
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

// Switch to simple vertical stack below Quasar's 'sm' breakpoint (600 px).
const isMobile = computed(() => $q.screen.lt.sm)

// ── snapshot data ──────────────────────────────────────────────────────────
const snapshots    = ref([])
const loading      = ref(false)
const refreshToken = ref(0)

const aggregate = computed(() => aggregateDirectorDashboardSnapshots(snapshots.value))

async function fetchData() {
  const credentials = auth.getCredentials()
  if (!credentials || props.activeDirectors.length === 0) {
    snapshots.value = []
    return
  }
  loading.value = true
  try {
    const results = await Promise.allSettled(
      props.activeDirectors.map(d =>
        fetchDirectorDashboardSnapshot({ ...credentials, director: d })
      )
    )
    snapshots.value = results
      .filter(r => r.status === 'fulfilled')
      .map(r => r.value)
  } finally {
    loading.value = false
    refreshToken.value += 1
  }
}

function refresh() { fetchData() }

watch(() => props.activeDirectors.join('\0'), () => fetchData(), { immediate: true })

provide(DASHBOARD_CONTEXT_KEY, {
  aggregate,
  loading,
  refresh,
  refreshToken,
  activeDirectors: computed(() => props.activeDirectors),
  directorOptions: computed(() => props.directorOptions),
})

// ── layout ─────────────────────────────────────────────────────────────────
// grid-layout-plus needs a flat array of { i, x, y, w, h } objects.
const layout = computed({
  get() {
    return props.dashboard.widgets.map(w => ({
      i: w.id,
      x: w.layout.x,
      y: w.layout.y,
      w: w.layout.w,
      h: w.layout.h,
    }))
  },
  set() {
    // mutations handled by onLayoutUpdated
  },
})

// Keep a reactive map so GridItem props stay in sync with drag/resize
const layoutMap = reactive({})
watch(
  () => props.dashboard.widgets,
  (widgets) => {
    widgets.forEach(w => {
      if (!layoutMap[w.id]) {
        layoutMap[w.id] = { x: w.layout.x, y: w.layout.y, w: w.layout.w, h: w.layout.h }
      }
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
  newLayout.forEach(item => {
    layoutMap[item.i] = { x: item.x, y: item.y, w: item.w, h: item.h }
  })
  dashboardStore.updateWidgetLayouts(
    props.dashboard.id,
    newLayout.map(item => ({ i: item.i, x: item.x, y: item.y, w: item.w, h: item.h }))
  )
}

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

defineExpose({ refresh, fetchData })
</script>
