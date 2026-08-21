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
  Renders the vue-grid-layout for one dashboard, provides the dashboard
  data context via Vue provide/inject to all child widgets, and handles
  widget configuration and removal.
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

    <GridLayout
      v-else
      :layout="gridLayout"
      :col-num="12"
      :row-height="30"
      :is-draggable="editMode"
      :is-resizable="editMode"
      :margin="[8, 8]"
      :use-css-transforms="true"
      @layout-updated="onLayoutUpdated"
    >
      <GridItem
        v-for="widget in dashboard.widgets"
        :key="widget.id"
        :i="widget.id"
        :x="widget.layout.x"
        :y="widget.layout.y"
        :w="widget.layout.w"
        :h="widget.layout.h"
        :min-w="widget.layout.minW ?? 2"
        :min-h="widget.layout.minH ?? 3"
        drag-allow-from=".widget-drag-handle"
        drag-ignore-from="a, button, input, select, .no-drag"
      >
        <WidgetShell
          :title="widget.title || defaultTitle(widget.type)"
          :edit-mode="editMode"
          @configure="openConfig(widget)"
          @remove="removeWidget(widget)"
          style="height:100%"
        >
          <component
            :is="resolveComponent(widget.type)"
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
import { ref, computed, provide, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { GridLayout, GridItem } from 'vue-grid-layout'
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

// ── snapshot data ─────────────────────────────────────────────────────────────
const snapshots = ref([])
const loading   = ref(false)
const refreshToken = ref(0)   // incremented to signal widgets to re-fetch

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
      props.activeDirectors.map(d => fetchDirectorDashboardSnapshot({ ...credentials, director: d }))
    )
    snapshots.value = results
      .filter(r => r.status === 'fulfilled')
      .map(r => r.value)
  } finally {
    loading.value = false
    refreshToken.value += 1
  }
}

// Expose as a callable refresh so RunningJobsWidget can trigger it.
function refresh() { fetchData() }

watch(() => props.activeDirectors.join('\0'), () => { fetchData() }, { immediate: true })

// Provide context to all child widgets.
provide(DASHBOARD_CONTEXT_KEY, {
  aggregate,
  loading,
  refresh,
  refreshToken,
  activeDirectors: computed(() => props.activeDirectors),
  directorOptions: computed(() => props.directorOptions),
})

// ── grid layout ───────────────────────────────────────────────────────────────

/** vue-grid-layout needs a flat layout array. */
const gridLayout = computed(() =>
  props.dashboard.widgets.map(w => ({
    i: w.id,
    x: w.layout.x,
    y: w.layout.y,
    w: w.layout.w,
    h: w.layout.h,
    minW: w.layout.minW ?? 2,
    minH: w.layout.minH ?? 3,
  }))
)

function onLayoutUpdated(newLayout) {
  if (!props.editMode) return
  dashboardStore.updateWidgetLayouts(props.dashboard.id, newLayout)
}

// ── widget actions ─────────────────────────────────────────────────────────────

function defaultTitle(type) {
  return getWidgetDefinition(type)?.defaultTitle ?? type
}

function resolveComponent(type) {
  return getWidgetDefinition(type)?.component ?? null
}

const configuringWidget = ref(null)
const showConfigDialog   = ref(false)

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
  }).onOk(() => {
    dashboardStore.removeWidget(props.dashboard.id, widget.id)
  })
}

// Expose refresh so the parent page can call it from the toolbar.
defineExpose({ refresh, fetchData })
</script>
