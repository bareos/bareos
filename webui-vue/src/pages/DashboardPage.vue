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
  <q-page class="q-pa-md">

    <!-- ── Top toolbar ───────────────────────────────────────────────────── -->
    <div class="row items-center q-mb-sm q-gutter-sm no-wrap">

      <!-- Dashboard tabs -->
      <q-tabs
        v-model="activeDashboardId"
        dense no-caps
        class="col"
        style="min-width:0"
        @update:model-value="dashStore.setActiveDashboard($event)"
      >
        <q-tab
          v-for="db in dashStore.dashboards"
          :key="db.id"
          :name="db.id"
          :label="db.name"
        >
          <!-- Rename button shown in edit mode -->
          <q-btn
            v-if="editMode && dashStore.dashboards.length > 1"
            flat round dense
            icon="edit"
            size="xs"
            class="q-ml-xs"
            @click.stop="startRename(db)"
          />
        </q-tab>

        <!-- Add dashboard tab -->
        <q-btn
          flat dense
          icon="add"
          size="sm"
          :title="t('Add dashboard')"
          @click="addDashboard"
        />
      </q-tabs>

      <!-- Right side controls -->
      <div class="row items-center q-gutter-xs no-wrap" style="flex-shrink:0">
        <!-- refresh countdown -->
        <span class="text-caption text-grey-6 panel-refresh-countdown">
          <span aria-hidden="true">↻</span>
          <span class="panel-refresh-countdown__value">{{ countdown }}s</span>
        </span>
        <q-btn flat round dense icon="refresh" size="sm" :loading="gridRef?.loading"
               :title="t('Refresh')" @click="manualRefresh" />

        <template v-if="editMode">
          <q-btn flat round dense icon="add_circle_outline" color="primary" size="sm"
                 :title="t('Add widget')" @click="showPicker = true" />
          <q-btn
            v-if="dashStore.dashboards.length > 1"
            flat round dense icon="delete_outline" color="negative" size="sm"
            :title="t('Delete dashboard')"
            @click="deleteDashboard"
          />
          <q-btn color="primary" dense no-caps size="sm" :label="t('Done')"
                 @click="editMode = false" />
        </template>
        <q-btn v-else flat round dense icon="edit" size="sm"
               :title="t('Edit layout')" @click="editMode = true" />
      </div>
    </div>

    <!-- Edit mode banner -->
    <q-banner v-if="editMode" class="bg-blue-1 text-blue-9 q-mb-sm" rounded dense>
      <template #avatar><q-icon name="info" color="blue-9" /></template>
      <span v-if="!isMobile">
        {{ t('Drag widgets to reposition. Resize from the bottom-right corner. Click the gear icon to configure a widget.') }}
      </span>
      <span v-else>
        Resize from the bottom-right corner. Tap the gear icon to configure a widget.
      </span>
    </q-banner>

    <!-- ── Dashboard grid ─────────────────────────────────────────────────── -->
    <DashboardGrid
      v-if="currentDashboard"
      ref="gridRef"
      :dashboard="currentDashboard"
      :edit-mode="editMode"
      :active-directors="activeDirectors"
      :director-options="directorOptions"
    />

    <!-- ── Widget picker ──────────────────────────────────────────────────── -->
    <WidgetPickerDialog
      v-model="showPicker"
      @pick="onWidgetPicked"
    />

    <!-- ── Widget initial config (required props) ─────────────────────────── -->
    <WidgetConfigDialog
      v-if="pendingWidget"
      v-model="showInitialConfig"
      :widget="pendingWidget"
      :required-only="true"
      :active-directors="activeDirectors"
      @save="onInitialConfigSave"
      @update:model-value="onInitialConfigClose"
    />

    <!-- ── Rename dashboard dialog ────────────────────────────────────────── -->
    <q-dialog v-model="showRenameDialog">
      <q-card style="min-width:320px">
        <q-card-section class="panel-header row items-center">
          <span>{{ t('Rename Dashboard') }}</span>
          <q-space />
          <q-btn flat round dense icon="close" color="white" @click="showRenameDialog = false" />
        </q-card-section>
        <q-card-section>
          <q-input
            v-model="renameValue"
            dense outlined autofocus
            :label="t('Dashboard name')"
            @keyup.enter="confirmRename"
          />
        </q-card-section>
        <q-card-actions align="right" class="q-pb-md q-pr-md">
          <q-btn flat :label="t('Cancel')" @click="showRenameDialog = false" />
          <q-btn color="primary" :label="t('Rename')" :disable="!renameValue.trim()" @click="confirmRename" />
        </q-card-actions>
      </q-card>
    </q-dialog>

  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useDashboardStore } from '../stores/dashboards.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { useDirectorStore } from '../stores/director.js'
import { getWidgetDefinition } from '../dashboard/widgetRegistry.js'
import { useSettingsStore } from '../stores/settings.js'
import DashboardGrid from '../dashboard/DashboardGrid.vue'
import WidgetPickerDialog from '../dashboard/WidgetPickerDialog.vue'
import WidgetConfigDialog from '../dashboard/WidgetConfigDialog.vue'

const { t } = useI18n()
const $q = useQuasar()
const dashStore = useDashboardStore()
const director = useDirectorStore()
const settings = useSettingsStore()

// ── director scope ─────────────────────────────────────────────────────────
const {
  directorOptions,
  activeDirectors,
  syncSelectedDirectors,
} = useDirectorScope({ t, syncEmptySelection: 'all' })

async function loadAvailableDirectors() {
  try {
    await director.fetchAvailableDirectors()
  } catch { /* keep going */ }
}

// ── active dashboard ───────────────────────────────────────────────────────
const activeDashboardId = computed({
  get: () => dashStore.activeDashboardId ?? dashStore.dashboards[0]?.id,
  set: (id) => dashStore.setActiveDashboard(id),
})
const currentDashboard = computed(() => dashStore.activeDashboard())
const isMobile = computed(() => $q.screen.lt.sm)

// ── edit mode ──────────────────────────────────────────────────────────────
const editMode = ref(false)

// ── grid ref (for refresh) ─────────────────────────────────────────────────
const gridRef = ref(null)

function manualRefresh() {
  gridRef.value?.refresh()
  countdown.value = settings.refreshInterval
}

// ── auto-refresh ───────────────────────────────────────────────────────────
const countdown = ref(settings.refreshInterval)
let _timer = null

function startAutoRefresh() {
  stopAutoRefresh()
  countdown.value = settings.refreshInterval
  _timer = setInterval(() => {
    countdown.value -= 1
    if (countdown.value <= 0) {
      gridRef.value?.refresh()
      countdown.value = settings.refreshInterval
    }
  }, 1000)
}

function stopAutoRefresh() {
  clearInterval(_timer)
  _timer = null
}

onMounted(async () => {
  await loadAvailableDirectors()
  syncSelectedDirectors()
  startAutoRefresh()
})
onUnmounted(stopAutoRefresh)

watch(() => directorOptions.value, () => { syncSelectedDirectors() }, { deep: true })

// ── add widget flow ────────────────────────────────────────────────────────
const showPicker = ref(false)

// Pending widget: a partially built widget instance awaiting initial config.
const pendingWidget = ref(null)
const showInitialConfig = ref(false)

function onWidgetPicked(def) {
  const hasRequired = def.requiredProps?.length > 0
  const nextY = currentDashboard.value?.widgets?.reduce(
    (maxY, w) => Math.max(maxY, Number(w.layout?.y ?? 0) + Number(w.layout?.h ?? 0)),
    0
  ) ?? 0
  const newWidget = {
    id: `tmp-${Date.now()}`,
    type: def.type,
    title: def.defaultTitle,
    props: {},
    layout: { ...def.defaultLayout, x: 0, y: nextY },
  }

  if (hasRequired) {
    // Show the initial config dialog before placing the widget.
    pendingWidget.value = newWidget
    showInitialConfig.value = true
  } else {
    placeWidget(newWidget)
  }
}

function onInitialConfigSave({ title, props }) {
  if (!pendingWidget.value) return
  placeWidget({
    ...pendingWidget.value,
    title,
    props,
  })
  pendingWidget.value = null
}

function onInitialConfigClose(open) {
  if (!open) {
    // User cancelled — discard pending widget.
    pendingWidget.value = null
  }
}

function placeWidget(widget) {
  dashStore.addWidget(activeDashboardId.value, {
    type:   widget.type,
    title:  widget.title,
    props:  widget.props,
    layout: widget.layout,
  })
}

// ── dashboard management ───────────────────────────────────────────────────

function addDashboard() {
  $q.dialog({
    title: t('New Dashboard'),
    prompt: {
      model: t('My Dashboard'),
      label: t('Dashboard name'),
      type: 'text',
    },
    ok:     { label: t('Create'), color: 'primary' },
    cancel: true,
  }).onOk(name => {
    if (name?.trim()) dashStore.addDashboard(name.trim())
  })
}

function deleteDashboard() {
  $q.dialog({
    title:   t('Delete Dashboard'),
    message: t('Delete dashboard "{name}"? This cannot be undone.', {
      name: currentDashboard.value?.name,
    }),
    ok:     { label: t('Delete'), color: 'negative', flat: true },
    cancel: { label: t('Cancel'), flat: true },
  }).onOk(() => {
    dashStore.removeDashboard(activeDashboardId.value)
    editMode.value = false
  })
}

// ── rename dashboard ───────────────────────────────────────────────────────
const showRenameDialog = ref(false)
const renameValue      = ref('')
const renamingId       = ref(null)

function startRename(db) {
  renamingId.value    = db.id
  renameValue.value   = db.name
  showRenameDialog.value = true
}

function confirmRename() {
  if (!renameValue.value.trim()) return
  dashStore.renameDashboard(renamingId.value, renameValue.value.trim())
  showRenameDialog.value = false
}
</script>
