<!--
  bareos-webui - Bareos Web-Frontend

  @link      https://github.com/bareos/bareos
  @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG
  @license   GNU Affero General Public License (http://www.gnu.org/licenses/)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->

<template>
  <component :is="embedded ? 'div' : 'q-page'" class="q-pa-md">
    <q-tabs v-if="!embedded" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-route-tab :label="t('Devices')"      no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'storages') }" />
      <q-route-tab :label="t('Pools')"        no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'pools') }" />
      <q-route-tab :label="t('Volumes')"      no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'volumes') }" />
      <q-route-tab :label="t('Autochangers')" no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'autochangers') }" />
    </q-tabs>

    <!-- ── Toolbar ─────────────────────────────────────────── -->
    <div class="row items-center q-gutter-sm q-mb-md">
      <q-select
        v-model="selectedStorage"
        :options="storageOptions"
        option-label="label"
        option-value="value"
        emit-value
        map-options
        :label="t('Autochanger')"
        outlined
        dense
        style="min-width:200px"
        :loading="storagesLoading"
      />

      <q-btn flat round dense icon="refresh" :title="t('Refresh')"
             :loading="slotsLoading" @click="manualRefresh" />

      <q-chip
        v-if="isCommonAutochangerScope && currentStorage"
        dense
        square
        color="primary"
        text-color="white"
      >
        {{ currentStorage.director }}
      </q-chip>

      <q-space />

      <q-btn outline color="primary" icon="sync_alt" :label="t('Update Slots')"
             :disable="!selectedStorageName || commandRunning" @click="doUpdateSlots" />

      <q-btn outline color="primary" icon="label" :label="t('Label barcodes')"
             :disable="!selectedStorageName || commandRunning" @click="labelDialog = true" />

      <q-btn outline color="primary" icon="monitor_heart" :label="t('Status')"
             :disable="!selectedStorageName || commandRunning" @click="showStatus" />
    </div>

    <q-banner v-if="loadError" class="bg-negative text-white q-mb-md" rounded>
      {{ loadError }}
    </q-banner>

    <q-banner
      v-if="directorErrors.length"
      class="bg-warning text-black q-mb-md"
      rounded
    >
      <template #avatar>
        <q-icon name="warning" />
      </template>
      <div v-for="item in directorErrors" :key="item.director">
        <strong>{{ item.director }}</strong>: {{ item.message }}
      </div>
    </q-banner>

    <!-- No autochanger storages -->
    <q-banner v-if="!storagesLoading && storageOptions.length === 0"
              class="bg-warning text-white q-mb-md" rounded>
      {{ t('No autochanger storages configured.') }}
    </q-banner>

    <!-- ── Slot Tables ────────────────────────────────────── -->
    <div v-if="selectedStorageName" class="row q-col-gutter-md">

      <!-- Storage Slots (left, wide) -->
      <div class="col-12 col-md-8">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">
            {{ formatCountLabel(storageSlots.length, t('Slots')) }}
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="storageSlots"
              :columns="slotCols"
              row-key="slotnr"
              dense flat
              :loading="slotsLoading"
              :pagination="{ rowsPerPage: 0 }"
              hide-pagination
              virtual-scroll
              style="max-height: 70vh"
            >
              <template #body-cell-slotnr="props">
                <q-td :props="props">
                  <q-icon
                    :name="props.row.content === 'full' ? 'inventory_2' : 'inbox'"
                    :color="props.row.content === 'full' ? 'brown-6' : 'grey-5'"
                    size="18px" class="q-mr-xs" />
                  {{ props.value }}
                </q-td>
              </template>
              <template #body-cell-drag="props">
                <q-td :props="props"
                      v-if="props.row.content === 'full' && slotInDriveMap[props.row.slotnr] == null"
                      draggable="true"
                      style="cursor:grab"
                      @dragstart="onSlotDragStart($event, props.row)">
                  <q-icon name="drag_indicator" color="grey-6" />
                  <q-tooltip>{{ t('Drag to a drive to mount') }}</q-tooltip>
                </q-td>
                <q-td v-else :props="props" />
              </template>
              <template #body-cell-content="props">
                <q-td :props="props"
                      @dragover="onDragOverSlot($event, props.row)"
                      @dragleave="onDragLeaveSlot"
                      @drop="onDropToSlot($event, props.row)"
                      :class="{
                        'dnd-drop-target': dragOverSlot === props.row.slotnr,
                        'dnd-drop-container': props.row.content === 'empty',
                      }">
                  <q-badge
                    v-if="slotInDriveMap[props.row.slotnr] != null"
                    color="blue"
                    :label="formatInDriveLabel(t, slotInDriveMap[props.row.slotnr])" />
                  <q-badge v-else
                           :color="props.value === 'full' ? 'green' : 'grey'"
                           :label="props.value" />
                  <span
                    v-if="dragOverSlot === props.row.slotnr"
                    class="dnd-drop-hint text-caption text-grey-6"
                  >{{ t('drop to move') }}</span>
                </q-td>
              </template>
              <template #body-cell-mr_volname="props">
                <q-td :props="props">
                  <div v-if="props.value && props.value !== '?'" class="row items-center no-wrap q-gutter-xs">
                    <VolumeNameLink
                      :name="props.value"
                      :volume="volumeDetailsByName[props.value]"
                      :query="buildAutochangerVolumeDetailsQuery(currentStorage)"
                    />
                    <q-btn flat round dense size="xs" icon="content_copy"
                           :title="t('Copy volume name')"
                           @click.stop="copyName(props.value)" />
                  </div>
                </q-td>
              </template>
              <template #body-cell-mr_volstatus="props">
                <q-td :props="props">
                  <span v-if="props.row.content === 'full'">
                    <q-badge v-if="props.value && props.value !== '?'"
                             :color="volStatusColor(props.value)"
                             :label="props.value" />
                    <q-badge v-else color="grey" :label="t('not in catalog')" />
                  </span>
                </q-td>
              </template>
              <template #body-cell-pr_name="props">
                <q-td :props="props">
                    <router-link
                      v-if="props.value && props.value !== '?'"
                      :to="{
                        name: 'pool-details',
                        params: { name: props.value },
                        query: {
                          ...buildAutochangerSelectionQuery(route.query, currentStorage),
                          ...(currentStorage?.director ? { director: currentStorage.director } : {}),
                        },
                      }"
                      class="text-primary"
                    >{{ props.value }}</router-link>
                  <q-btn v-else-if="props.row.content === 'full'"
                         flat dense size="sm" icon="label" :label="t('Label')"
                         color="primary" no-caps
                         :title="t('Label this volume into a pool')"
                         @click="openSlotLabelDialog(props.row.slotnr)" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                  <template v-if="props.row.content === 'full' && slotInDriveMap[props.row.slotnr] == null">
                    <q-btn flat round dense size="sm" icon="play_circle"
                           :title="t('Mount to drive')"
                           @click="openSlotMountDialog(props.row.slotnr)" />
                    <q-btn flat round dense size="sm" icon="swap_horiz"
                           :title="t('Transfer to slot')"
                           @click="openTransferDialog(props.row.slotnr)" />
                    <q-btn flat round dense size="sm" icon="upload"
                           :title="t('Export')"
                           @click="doExport(props.row.slotnr)" />
                  </template>
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </div>

      <!-- Right column: Drives (top) + Import/Export Slots (bottom) -->
      <div class="col-12 col-md-4 column q-gutter-md">

        <!-- Drives -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">
            {{ formatCountLabel(drives.length, t('Drives')) }}
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="drives"
              :columns="driveCols"
              row-key="slotnr"
              dense flat
              :loading="slotsLoading"
              :pagination="{ rowsPerPage: 0 }"
              hide-pagination
            >
              <template #body-cell-drag="props">
                <q-td :props="props"
                      v-if="props.row.content === 'full'"
                      draggable="true"
                      style="cursor:grab"
                      @dragstart="onSlotDragStart($event, props.row)">
                  <q-icon name="drag_indicator" color="grey-6" />
                  <q-tooltip>{{ t('Drag to a slot to import') }}</q-tooltip>
                </q-td>
                <q-td v-else :props="props" />
              </template>
              <template #body-cell-slotnr="props">
                <q-td :props="props">
                  <q-icon name="dns"
                          :color="props.row.content === 'full' ? 'primary' : 'grey-5'"
                          size="18px" class="q-mr-xs" />
                  {{ props.value }}
                </q-td>
              </template>
              <template #body-cell-content="props">
                <q-td :props="props"
                      @dragover="onDragOverDrive($event, props.row)"
                      @dragleave="onDragLeaveDrive"
                      @drop="onDropToDrive($event, props.row)"
                      :class="{
                        'dnd-drop-target': dragOverDrive === props.row.slotnr && props.row.content === 'empty',
                        'dnd-drop-container': props.row.content === 'empty',
                      }">
                   <q-badge :color="props.value === 'full' ? 'green' : 'grey'"
                            :label="props.value" />
                   <span
                     v-if="dragOverDrive === props.row.slotnr && props.row.content === 'empty'"
                      class="dnd-drop-hint text-caption text-grey-6"
                    >{{ t('drop to mount') }}</span>
                 </q-td>
               </template>
              <template #body-cell-volname="props">
                <q-td :props="props">
                  <div v-if="props.value" class="row items-center no-wrap q-gutter-xs">
                    <VolumeNameLink
                      :name="props.value"
                      :volume="volumeDetailsByName[props.value]"
                      :query="buildAutochangerVolumeDetailsQuery(currentStorage)"
                    />
                    <q-btn flat round dense size="xs" icon="content_copy"
                           :title="t('Copy volume name')"
                           @click.stop="copyName(props.value)" />
                  </div>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                  <q-btn v-if="props.row.content === 'full'"
                         flat round dense size="sm" icon="eject"
                         :title="t('Release')"
                         @click="doRelease(props.row.slotnr)" />
                  <q-btn v-else
                         flat round dense size="sm" icon="play_circle"
                         :title="t('Mount')"
                         @click="openMountDialog(props.row.slotnr)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Import/Export Slots -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center no-wrap">
            <span class="col">
              {{ formatCountLabel(importSlots.length, t('Import/Export Slots')) }}
            </span>
             <q-btn flat round dense size="sm" icon="download"
                   :title="t('Import all')" :disable="commandRunning" @click="doImportAll" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="importSlots"
              :columns="ieSlotCols"
              row-key="slotnr"
              dense flat
              :loading="slotsLoading"
              :pagination="{ rowsPerPage: 0 }"
              hide-pagination
            >
              <template #body-cell-slotnr="props">
                <q-td :props="props">
                  <q-icon name="move_to_inbox"
                          :color="props.row.content === 'full' ? 'teal-6' : 'grey-5'"
                          size="18px" class="q-mr-xs" />
                  {{ props.value }}
                </q-td>
              </template>
              <template #body-cell-content="props">
                <q-td :props="props"
                      @dragover="onDragOverImportSlot($event, props.row)"
                      @dragleave="onDragLeaveImportSlot"
                      @drop="onDropToImportSlot($event, props.row)"
                      :class="{ 'dnd-drop-target': dragOverImportSlot === props.row.slotnr }">
                  <q-badge :color="props.value === 'full' ? 'green' : 'grey'"
                           :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-volname="props">
                <q-td :props="props">
                  <div v-if="props.value" class="row items-center no-wrap q-gutter-xs">
                    <VolumeNameLink
                      :name="props.value"
                      :volume="volumeDetailsByName[props.value]"
                      :query="buildAutochangerVolumeDetailsQuery(currentStorage)"
                    />
                    <q-btn flat round dense size="xs" icon="content_copy"
                           :title="t('Copy volume name')"
                           @click.stop="copyName(props.value)" />
                  </div>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                   <q-btn v-if="props.row.content === 'full'"
                          flat round dense size="sm" icon="download"
                          :title="t('Import')"
                          :disable="commandRunning"
                          @click="doImport(props.row.slotnr)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

      </div>

    </div>

    <!-- ── Label Barcodes Dialog ──────────────────────────── -->
    <q-dialog v-model="labelDialog">
      <q-card style="min-width:360px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">{{ t('Label barcodes') }}</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-select v-model="labelForm.pool"
                    :options="poolNames"
                    :label="t('Pool')" outlined dense />
          <q-input v-model="labelForm.drive"
                   :label="t('Drive (number)')" outlined dense type="number"
                   :min="0" />
          <q-input v-model="labelForm.slots"
                   label='Slots (e.g. "1-10" or leave blank)'
                   outlined dense />
          <q-checkbox
            v-model="labelForm.encrypted"
            :label="t('encrypt newly labeled volumes')"
            dense
          />
        </q-card-section>
        <q-card-actions align="right">
           <q-btn flat :label="t('Cancel')" v-close-popup />
           <q-btn color="primary" :label="t('Label')"
                  :disable="!labelForm.pool || commandRunning"
                  :loading="commandRunning"
                  @click="doLabelBarcodes" />
        </q-card-actions>
      </q-card>
    </q-dialog>

    <!-- ── Mount Dialog ───────────────────────────────────── -->
    <q-dialog v-model="mountDialog">
      <q-card style="min-width:300px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">{{ t('Mount Tape') }}</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-input v-model="mountForm.slot"
                   :label="t('Slot number')" outlined dense type="number"
                   :min="1" :readonly="mountForm.slotFixed" />
          <q-input v-model="mountForm.drive"
                   :label="t('Drive number')" outlined dense type="number"
                   :min="0" :readonly="mountForm.driveFixed" />
        </q-card-section>
        <q-card-actions align="right">
           <q-btn flat :label="t('Cancel')" v-close-popup />
           <q-btn color="primary" :label="t('Mount')"
                  :disable="commandRunning"
                  :loading="commandRunning"
                  @click="doMount" />
        </q-card-actions>
      </q-card>
    </q-dialog>

    <!-- ── Transfer Dialog ──────────────────────────────────── -->
    <q-dialog v-model="transferDialog">
      <q-card style="min-width:320px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">{{ t('Transfer from Slot {slot}', { slot: transferForm.srcSlot }) }}</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-select v-model="transferForm.dstSlot"
                    :options="emptySlotOptions"
                    :label="t('Destination Slot')" outlined dense
                    emit-value map-options />
        </q-card-section>
        <q-card-actions align="right">
           <q-btn flat :label="t('Cancel')" v-close-popup />
           <q-btn color="primary" :label="t('Transfer')"
                 :disable="transferForm.dstSlot == null || commandRunning"
                 :loading="commandRunning"
                 @click="doTransfer" />
        </q-card-actions>
      </q-card>
    </q-dialog>

    <!-- ── Slot Label Dialog ──────────────────────────────── -->
    <q-dialog v-model="slotLabelDialog">
      <q-card style="min-width:360px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">{{ t('Label Slot {slot}', { slot: slotLabelForm.slot }) }}</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-select v-model="slotLabelForm.pool"
                    :options="poolNames"
                    :label="t('Pool')" outlined dense />
          <q-input v-model="slotLabelForm.drive"
                   :label="t('Drive (number)')" outlined dense type="number"
                   :min="0" />
          <q-checkbox
            v-model="slotLabelForm.encrypted"
            :label="t('encrypt newly labeled volumes')"
            dense
          />
        </q-card-section>
        <q-card-actions align="right">
           <q-btn flat :label="t('Cancel')" v-close-popup />
           <q-btn color="primary" :label="t('Label')" :disable="!slotLabelForm.pool"
                  :loading="commandRunning"
                  @click="doSlotLabel" />
         </q-card-actions>
       </q-card>
     </q-dialog>

    <q-card
      v-if="commandLogVisible"
      flat
      bordered
      class="bareos-panel q-mt-md"
    >
      <q-card-section class="row items-center q-pb-sm">
        <div class="col">
          <div class="text-subtitle1">{{ commandLogTitle || t('Command output') }}</div>
          <div class="text-caption text-grey-7 command-log-command">{{ commandLogCommand }}</div>
          <div v-if="commandStatusSummary" class="text-caption text-grey-7 q-mt-xs">
            {{ commandStatusSummary }}
          </div>
        </div>
        <q-badge
          v-if="commandRunning"
          :color="commandStatusColor"
          class="q-mr-sm"
        >
          <q-spinner size="14px" class="q-mr-xs" />
          {{ commandStatusLabel }}
        </q-badge>
        <q-badge v-else :color="commandStatusColor" class="q-mr-sm">{{ commandStatusLabel }}</q-badge>
        <q-btn
          flat
          round
          dense
          icon="delete_sweep"
          :disable="commandRunning"
          :title="t('Clear')"
          @click="clearCommandLog"
        />
      </q-card-section>
      <q-separator />
      <q-card-section class="q-pa-none">
        <q-scroll-area ref="commandLogScroll" style="height:220px">
          <pre class="command-log-text">{{ commandLogText }}</pre>
        </q-scroll-area>
      </q-card-section>
    </q-card>
  </component>
</template>

<script setup>
import { ref, computed, watch, onMounted, onUnmounted, nextTick } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { useQuasar } from 'quasar'
import { directorCollection } from '../composables/useDirectorFetch.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { fetchAggregatedAutochangerStorages } from '../composables/storagesAggregate.js'
import {
  buildExportCommand,
  buildImportCommand,
  buildLabelBarcodesCommand,
  formatInDriveLabel,
  shouldReloadAutochangerAfterCommand,
  shouldRefreshAutochangerTables,
} from '../utils/autochanger.js'
import {
  AUTOCHANGER_DIRECTOR_QUERY_KEY,
  AUTOCHANGER_STORAGE_QUERY_KEY,
  buildAutochangerSelectionQuery,
  buildStoragesTabQuery,
  resolveAutochangerSelection,
  resolveStoragesScopeDirector,
} from '../utils/storagesRoute.js'
import VolumeNameLink from '../components/VolumeNameLink.vue'

const { embedded } = defineProps({
  embedded: {
    type: Boolean,
    default: false,
  },
})

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const route = useRoute()
const router = useRouter()
const { t } = useI18n()

// ── State ───────────────────────────────────────────────────

const autochangerStorages = ref([])
const storagesLoading = ref(false)
const loadError = ref(null)
const directorErrors = ref([])
const selectedStorage = ref(null)

const allSlots = ref([])
const slotsLoading = ref(false)
const volumeDetailsByName = ref({})

const pools = ref([])

const labelDialog = ref(false)
const labelForm = ref({ pool: '', drive: 0, slots: '', encrypted: false })

const slotLabelDialog = ref(false)
const slotLabelForm = ref({ slot: null, pool: '', drive: 0, encrypted: false })

const mountDialog = ref(false)
const mountForm = ref({ drive: 0, slot: '', slotFixed: false, driveFixed: false })

const transferDialog = ref(false)
const transferForm = ref({ srcSlot: null, dstSlot: null })

const commandRunning = ref(false)
const commandLogVisible = ref(false)
const commandLogTitle = ref('')
const commandLogCommand = ref('')
const commandLogText = ref('')
const commandStatus = ref('idle')
const commandStatusPrompt = ref('')
const commandStatusMessage = ref('')
const commandLogScroll = ref(null)
let _slotsTimer = null

// drag-and-drop state
const dragSlot = ref(null)      // slot row being dragged
const dragOverDrive = ref(null) // drive number being hovered during drag
const dragOverSlot = ref(null)  // slot number being hovered during drag
const dragOverImportSlot = ref(null) // import/export slot number hovered during drag

// ── Computed ─────────────────────────────────────────────────

const reachableDirectors = computed(() => [...new Set([
  ...director.availableDirectors,
  auth.user?.director,
  settings.directorName,
].filter(Boolean))])

const directorOptions = computed(() => (
  reachableDirectors.value.map(value => ({ label: value, value }))
))

function syncSelectedDirectors() {
  const validDirectors = reachableDirectors.value
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

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    reachableDirectors.value.includes(value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isAutochangerRouteActive = computed(() => !embedded || route.query.tab === 'autochangers')
const queriedStorageName = computed(() => (
  typeof route.query[AUTOCHANGER_STORAGE_QUERY_KEY] === 'string'
    ? route.query[AUTOCHANGER_STORAGE_QUERY_KEY]
    : ''
))
const queriedDirectorName = computed(() => (
  typeof route.query[AUTOCHANGER_DIRECTOR_QUERY_KEY] === 'string'
    ? route.query[AUTOCHANGER_DIRECTOR_QUERY_KEY]
    : ''
))
const storagesScopeDirector = computed(() => resolveStoragesScopeDirector(route.query))
const autochangerScopeDirectors = computed(() => (
  storagesScopeDirector.value ? [storagesScopeDirector.value] : activeDirectors.value
))
const currentStorage = computed(() => (
  autochangerStorages.value.find(storage => storage.scopeKey === selectedStorage.value) ?? null
))
const selectedStorageName = computed(() => currentStorage.value?.name ?? null)
const isCommonAutochangerScope = computed(() => autochangerScopeDirectors.value.length > 1)
const storageOptions = computed(() => autochangerStorages.value.map(storage => ({
  label: isCommonAutochangerScope.value ? storage.label : storage.name,
  value: storage.scopeKey,
})))

const drives = computed(() =>
  allSlots.value.filter(s => s.type === 'drive')
    .sort((a, b) => a.slotnr - b.slotnr)
)

const importSlots = computed(() =>
  allSlots.value.filter(s => s.type === 'import_slot')
    .sort((a, b) => a.slotnr - b.slotnr)
)

const storageSlots = computed(() =>
  allSlots.value.filter(s => s.type === 'slot')
    .sort((a, b) => a.slotnr - b.slotnr)
)

const poolNames = computed(() => pools.value.map(p => p.name))

const emptySlotOptions = computed(() =>
  storageSlots.value
    .filter(s => s.content === 'empty')
    .map(s => ({ label: `Slot ${s.slotnr}`, value: s.slotnr }))
)

function buildAutochangerVolumeDetailsQuery(storage) {
  if (!storage) {
    return null
  }

  return {
    ...buildAutochangerSelectionQuery(route.query, storage),
    ...(storage.director ? { director: storage.director } : {}),
  }
}

// Map from slot number → drive number for tapes currently in a drive
const slotInDriveMap = computed(() => {
  const map = {}
  for (const d of drives.value) {
    if (d.content === 'full' && d.loaded != null) {
      map[d.loaded] = d.slotnr
    }
  }
  return map
})

const commandStatusColor = computed(() => {
  switch (commandStatus.value) {
    case 'starting':
      return 'info'
    case 'running':
      return 'primary'
    case 'waiting_for_input':
      return 'warning'
    case 'failed':
      return 'negative'
    case 'completed':
      return 'positive'
    default:
      return 'grey-6'
  }
})

const commandStatusLabel = computed(() => {
  switch (commandStatus.value) {
    case 'starting':
      return t('Starting...')
    case 'running':
      return t('Running')
    case 'waiting_for_input':
      return t('Waiting for input')
    case 'failed':
      return t('Failed')
    case 'completed':
      return t('Completed')
    default:
      return t('Idle')
  }
})

const commandStatusSummary = computed(() => {
  if (commandStatusMessage.value) {
    return commandStatusMessage.value
  }

  if (commandStatus.value === 'waiting_for_input') {
    if (commandStatusPrompt.value === 'select') {
      return t('The director is waiting for a numeric selection.')
    }

    if (commandStatusPrompt.value === 'sub') {
      return t('The director is waiting for additional input.')
    }

    return t('The director returned a non-final prompt.')
  }

  if (commandStatus.value === 'completed') {
    return t('The command finished and returned to the main prompt.')
  }

  return ''
})

// ── Column definitions ────────────────────────────────────────

const ieSlotCols = [
  { name: 'drag',    label: '',       field: 'drag',     align: 'left', style: 'width:32px; padding:0 4px' },
  { name: 'slotnr',  label: 'Slot',   field: 'slotnr',   align: 'left', sortable: true },
  { name: 'content', label: 'Status', field: 'content',  align: 'left' },
  { name: 'volname', label: 'Volume', field: 'volname',  align: 'left' },
  { name: 'actions', label: '',       field: 'actions',  align: 'right' },
]

const driveCols = [
  { name: 'slotnr',  label: 'Drive',  field: 'slotnr',   align: 'left', sortable: true },
  { name: 'content', label: 'Status', field: 'content',  align: 'left' },
  { name: 'volname', label: 'Volume', field: 'volname',  align: 'left' },
  { name: 'loaded',  label: 'Slot',   field: 'loaded',   align: 'left' },
  { name: 'actions', label: '',       field: 'actions',  align: 'right' },
]

const slotCols = [
  { name: 'drag',        label: '',          field: 'drag',        align: 'left',  style: 'width:32px; padding:0 4px' },
  { name: 'slotnr',      label: 'Slot',      field: 'slotnr',      align: 'left',  sortable: true },
  { name: 'content',     label: 'Status',    field: 'content',     align: 'left' },
  { name: 'mr_volname',  label: 'Volume',    field: 'mr_volname',  align: 'left' },
  { name: 'mr_volstatus',label: 'Vol Status',field: 'mr_volstatus',align: 'left' },
  { name: 'pr_name',     label: 'Pool',      field: 'pr_name',     align: 'left' },
  { name: 'actions',     label: '',          field: 'actions',     align: 'right' },
]

// ── Helpers ───────────────────────────────────────────────────

function copyName(name) {
  $q.copyToClipboard(name).then(() => {
    $q.notify({ type: 'positive', message: `Copied: ${name}`, timeout: 1500 })
  })
}

function volStatusColor(status) {
  switch ((status ?? '').toLowerCase()) {
    case 'append':  return 'green'
    case 'full':    return 'blue'
    case 'used':    return 'orange'
    case 'error':   return 'red'
    case 'purged':  return 'deep-orange'
    case 'recycle': return 'teal'
    default:        return 'primary'
  }
}

function formatCountLabel(count, label) {
  return `${count} ${label}`
}

// ── Data Loading ──────────────────────────────────────────────

async function loadStorages() {
  storagesLoading.value = true
  loadError.value = null
  directorErrors.value = []
  try {
    if (autochangerScopeDirectors.value.length === 0) {
      autochangerStorages.value = []
      selectedStorage.value = null
      return
    }

    if (isCommonAutochangerScope.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedAutochangerStorages(
        credentials,
        autochangerScopeDirectors.value
      )
      autochangerStorages.value = result.storages
      directorErrors.value = result.directorErrors
    } else {
      await ensureScopeDirector(autochangerScopeDirectors.value[0])
      const res = await director.call('list storages')
      const list = directorCollection(res?.storages)
      autochangerStorages.value = list
        .filter(s => String(s.autochanger) === '1')
        .map(storage => ({
          ...storage,
          director: autochangerScopeDirectors.value[0],
          scopeKey: `${autochangerScopeDirectors.value[0]}:${storage.name}`,
          label: `${autochangerScopeDirectors.value[0]} / ${storage.name}`,
        }))
    }

    const queriedSelection = resolveAutochangerSelection(autochangerStorages.value, {
      storageName: queriedStorageName.value,
      directorName: queriedDirectorName.value,
      scopeDirector: storagesScopeDirector.value,
      activeDirectors: autochangerScopeDirectors.value,
    })
    if (queriedSelection) {
      selectedStorage.value = queriedSelection.scopeKey
    } else if (!autochangerStorages.value.some(storage => storage.scopeKey === selectedStorage.value)) {
      selectedStorage.value = autochangerStorages.value[0]?.scopeKey ?? null
    }
  } catch (e) {
    autochangerStorages.value = []
    selectedStorage.value = null
    loadError.value = e?.message ?? String(e)
  } finally {
    storagesLoading.value = false
  }
}

async function ensureScopeDirector(targetDirector) {
  if (!targetDirector) {
    return
  }

  if (auth.user?.director === targetDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(targetDirector)
}

async function ensureSelectedStorageDirector() {
  if (!currentStorage.value?.director) {
    return
  }

  await ensureScopeDirector(currentStorage.value.director)
}

async function loadPools() {
  if (!selectedStorageName.value) {
    pools.value = []
    return
  }

  try {
    await ensureSelectedStorageDirector()
    const res = await director.call('list pools')
    pools.value = res?.pools ?? []
  } catch (e) {
    pools.value = []
    loadError.value = e?.message ?? String(e)
  }
}

async function loadSlots() {
  if (!selectedStorageName.value) {
    allSlots.value = []
    volumeDetailsByName.value = {}
    return
  }

  slotsLoading.value = true
  loadError.value = null
  try {
    await ensureSelectedStorageDirector()
    const res = await director.call(
      `status storage="${selectedStorageName.value}" slots`
    )
    allSlots.value = res?.contents ?? []

    const volumeNames = [...new Set(
      allSlots.value.flatMap(slot => [slot.volname, slot.mr_volname])
        .filter(name => name && name !== '?')
    )]
    const volumeResults = await Promise.allSettled(
      volumeNames.map(name =>
        director.call(
          `llist volume="${String(name).replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`
        )
      )
    )

    volumeDetailsByName.value = volumeNames.reduce((acc, name, index) => {
      const result = volumeResults[index]
      if (result.status !== 'fulfilled') {
        return acc
      }

      const raw = result.value?.volumes ?? result.value?.volume ?? null
      acc[name] = Array.isArray(raw) ? (raw[0] ?? null) : raw
      return acc
    }, {})
  } catch (e) {
    allSlots.value = []
    volumeDetailsByName.value = {}
    loadError.value = e?.message ?? String(e)
  } finally {
    slotsLoading.value = false
  }
}

function startAutoRefresh() {
  clearInterval(_slotsTimer)
  _slotsTimer = setInterval(() => {
    if (!shouldRefreshAutochangerTables({
      selectedStorage: selectedStorageName.value,
      slotsLoading: slotsLoading.value,
      commandRunning: commandRunning.value,
    })) return
    void loadSlots()
  }, settings.refreshInterval * 1000)
}

function stopAutoRefresh() {
  clearInterval(_slotsTimer)
  _slotsTimer = null
}

function manualRefresh() {
  void loadSlots()
  startAutoRefresh()
}

// ── Actions ───────────────────────────────────────────────────

async function runCmd(title, cmd) {
  if (commandRunning.value) return

  commandLogTitle.value = title
  commandLogCommand.value = cmd
  commandLogText.value = `$ ${cmd}\n\n`
  commandLogVisible.value = true
  commandStatus.value = 'starting'
  commandStatusPrompt.value = ''
  commandStatusMessage.value = ''
  commandRunning.value = true
  await scrollCommandLogToBottom()

  let finalState = {
    status: 'starting',
    prompt: '',
    message: '',
  }

  const applyCommandState = (nextState) => {
    finalState = {
      ...finalState,
      ...nextState,
      status: nextState?.status || finalState.status,
    }

    commandStatus.value = finalState.status
    commandStatusPrompt.value = finalState.prompt ?? ''
    commandStatusMessage.value = finalState.message ?? ''
  }

  try {
    await ensureSelectedStorageDirector()
    await director.rawCall(cmd, {
      onChunk: (chunk) => {
        if (!chunk) return
        commandLogText.value += chunk
        void scrollCommandLogToBottom()
      },
      onStateChange: applyCommandState,
    })
  } catch (e) {
    commandLogText.value += `\nError: ${e.message}\n`
    applyCommandState({
      status: 'failed',
      message: e.message,
    })
    void scrollCommandLogToBottom()
  } finally {
    commandRunning.value = false
  }

  if (finalState.status === 'starting' || finalState.status === 'running') {
    applyCommandState({
      status: 'completed',
      prompt: 'main',
    })
  }

  return finalState
}

async function doUpdateSlots() {
  const result = await runCmd(
    'Update Slots',
    `update slots storage="${selectedStorageName.value}"`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doImportAll() {
  const result = await runCmd(
    'Import All',
    `import storage="${selectedStorageName.value}"`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doImport(slotNr) {
  const result = await runCmd(
      `Import Slot ${slotNr}`,
      buildImportCommand({
        storage: selectedStorageName.value,
        srcSlot: slotNr,
      })
    )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doExport(slotNr) {
  $q.dialog({
    title: 'Export Slot',
    message: `Export volume from slot ${slotNr}?`,
    ok: { color: 'primary', label: 'Export' },
    cancel: true,
  }).onOk(async () => {
    const result = await runCmd(
      `Export Slot ${slotNr}`,
      buildExportCommand({
        storage: selectedStorageName.value,
        srcSlot: slotNr,
      })
    )
    if (shouldReloadAutochangerAfterCommand(result?.status)) {
      await loadSlots()
    }
  })
}

// Open mount dialog from an empty drive (slot is free-input)
function openMountDialog(driveNr) {
  mountForm.value = { slot: '', drive: driveNr, slotFixed: false, driveFixed: true }
  mountDialog.value = true
}

// Open mount dialog from a storage slot (drive is free-input)
function openSlotMountDialog(slotNr) {
  const firstEmptyDrive = drives.value.find(d => d.content === 'empty')
  mountForm.value = {
    slot: slotNr,
    drive: firstEmptyDrive?.slotnr ?? 0,
    slotFixed: true,
    driveFixed: false,
  }
  mountDialog.value = true
}

function openTransferDialog(slotNr) {
  transferForm.value = { srcSlot: slotNr, dstSlot: null }
  transferDialog.value = true
}

async function doTransfer() {
  transferDialog.value = false
  const { srcSlot, dstSlot } = transferForm.value
  const result = await runCmd(
    `Transfer Slot ${srcSlot} → Slot ${dstSlot}`,
    `move storage="${selectedStorageName.value}"` +
    ` srcslots=${srcSlot} dstslots=${dstSlot}`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doMount() {
  mountDialog.value = false
  const result = await runCmd(
    'Mount',
    `mount storage="${selectedStorageName.value}"` +
    ` slot=${mountForm.value.slot} drive=${mountForm.value.drive}`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doUnmount(driveNr) {
  const result = await runCmd(
    `Unmount Drive ${driveNr}`,
    `unmount storage="${selectedStorageName.value}" drive=${driveNr}`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doRelease(driveNr) {
  const result = await runCmd(
    `Release Drive ${driveNr}`,
    `release storage="${selectedStorageName.value}" drive=${driveNr}`
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function doLabelBarcodes() {
  labelDialog.value = false
  const { pool, drive, slots, encrypted } = labelForm.value
  const cmd = buildLabelBarcodesCommand({
    storage: selectedStorageName.value,
    pool,
    drive,
    slots,
    encrypted,
  })
  const result = await runCmd(t('Label barcodes'), cmd)
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

function openSlotLabelDialog(slotNr) {
  slotLabelForm.value = {
    slot: slotNr,
    pool: poolNames.value[0] ?? '',
    drive: 0,
    encrypted: false,
  }
  slotLabelDialog.value = true
}

async function doSlotLabel() {
  slotLabelDialog.value = false
  const { slot, pool, drive, encrypted } = slotLabelForm.value
  const cmd = buildLabelBarcodesCommand({
    storage: selectedStorageName.value,
    pool,
    drive,
    slots: slot,
    encrypted,
  })
  const result = await runCmd(`Label Slot ${slot}`, cmd)
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

// ── Drag-and-drop ─────────────────────────────────────────────

function onSlotDragStart(event, row) {
  dragSlot.value = row
  event.dataTransfer.effectAllowed = 'move'
  event.dataTransfer.setData('text/plain', String(row.slotnr))
}

function onDragOverDrive(event, drive) {
  if (dragSlot.value?.type !== 'slot') return
  if (drive.content !== 'empty') return
  event.preventDefault()
  event.dataTransfer.dropEffect = 'move'
  dragOverDrive.value = drive.slotnr
}

function onDragLeaveDrive() {
  dragOverDrive.value = null
}

async function onDropToDrive(event, drive) {
  event.preventDefault()
  dragOverDrive.value = null
  const slot = dragSlot.value
  dragSlot.value = null
  if (!slot || slot.type !== 'slot' || drive.content !== 'empty') return
  const result = await runCmd(
      `Mount Slot ${slot.slotnr} → Drive ${drive.slotnr}`,
      `mount storage="${selectedStorageName.value}"` +
      ` slot=${slot.slotnr} drive=${drive.slotnr}`
    )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

function onDragOverSlot(event, row) {
  if (!dragSlot.value) return
  if (row.slotnr === dragSlot.value.slotnr) return // same slot
  if (row.content !== 'empty') return
  event.preventDefault()
  event.dataTransfer.dropEffect = 'move'
  dragOverSlot.value = row.slotnr
}

function onDragLeaveSlot() {
  dragOverSlot.value = null
}

async function onDropToSlot(event, row) {
  event.preventDefault()
  dragOverSlot.value = null
  const src = dragSlot.value
  dragSlot.value = null
  if (!src || row.content !== 'empty' || row.slotnr === src.slotnr) return
  if (src.type === 'import_slot') {
    const result = await runCmd(
        `Import Slot ${src.slotnr} → Slot ${row.slotnr}`,
        buildImportCommand({
          storage: selectedStorageName.value,
          srcSlot: src.slotnr,
          dstSlot: row.slotnr,
        })
    )
    if (shouldReloadAutochangerAfterCommand(result?.status)) {
      await loadSlots()
    }
  } else {
      const result = await runCmd(
        `Move Slot ${src.slotnr} → Slot ${row.slotnr}`,
        `move storage="${selectedStorageName.value}"` +
        ` srcslots=${src.slotnr} dstslots=${row.slotnr}`
      )
      if (shouldReloadAutochangerAfterCommand(result?.status)) {
        await loadSlots()
      }
   }
}

function onDragOverImportSlot(event, row) {
  if (dragSlot.value?.type !== 'slot') return
  if (!dragSlot.value) return
  if (row.content !== 'empty') return
  event.preventDefault()
  event.dataTransfer.dropEffect = 'move'
  dragOverImportSlot.value = row.slotnr
}

function onDragLeaveImportSlot() {
  dragOverImportSlot.value = null
}

async function onDropToImportSlot(event, row) {
  event.preventDefault()
  dragOverImportSlot.value = null
  const src = dragSlot.value
  dragSlot.value = null
  if (!src || src.type !== 'slot' || row.content !== 'empty') return
  const result = await runCmd(
    `Export Slot ${src.slotnr} → Import/Export Slot ${row.slotnr}`,
    buildExportCommand({
      storage: selectedStorageName.value,
      srcSlot: src.slotnr,
      dstSlot: row.slotnr,
    })
  )
  if (shouldReloadAutochangerAfterCommand(result?.status)) {
    await loadSlots()
  }
}

async function showStatus() {
  await runCmd(
    `Status: ${selectedStorageName.value}`,
    `status storage="${selectedStorageName.value}"`
  )
}

async function refreshSelectedStorageViewsIfStable(previousSelection) {
  if (!selectedStorage.value || selectedStorage.value !== previousSelection) {
    return
  }

  await loadPools()
  await loadSlots()
  startAutoRefresh()
}

async function syncRouteToSelectedStorage() {
  if (!isAutochangerRouteActive.value) {
    return
  }

  const targetQuery = buildAutochangerSelectionQuery(route.query, currentStorage.value)
  const currentStorageName = queriedStorageName.value
  const currentDirectorName = queriedDirectorName.value
  const targetStorageName = targetQuery[AUTOCHANGER_STORAGE_QUERY_KEY] ?? ''
  const targetDirectorName = targetQuery[AUTOCHANGER_DIRECTOR_QUERY_KEY] ?? ''

  if (
    route.query.tab === targetQuery.tab
    && currentStorageName === targetStorageName
    && currentDirectorName === targetDirectorName
  ) {
    return
  }

  await router.replace({
    path: '/storages',
    query: targetQuery,
  })
}

function syncSelectedStorageFromRoute() {
  const queriedSelection = resolveAutochangerSelection(autochangerStorages.value, {
    storageName: queriedStorageName.value,
    directorName: queriedDirectorName.value,
    scopeDirector: storagesScopeDirector.value,
    activeDirectors: autochangerScopeDirectors.value,
  })
  if (!queriedSelection || selectedStorage.value === queriedSelection.scopeKey) {
    return
  }

  selectedStorage.value = queriedSelection.scopeKey
}

function clearCommandLog() {
  commandLogVisible.value = false
  commandLogTitle.value = ''
  commandLogCommand.value = ''
  commandLogText.value = ''
}

async function scrollCommandLogToBottom() {
  await nextTick()
  commandLogScroll.value?.setScrollPosition('vertical', Number.MAX_SAFE_INTEGER, 0)
}

// ── Lifecycle ─────────────────────────────────────────────────

onMounted(async () => {
  await director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  const previousSelection = selectedStorage.value
  await loadStorages()
  await refreshSelectedStorageViewsIfStable(previousSelection)
})

watch(() => director.status, async (s) => {
  if (s === 'connected') {
    syncSelectedDirectors()
    const previousSelection = selectedStorage.value
    await loadStorages()
    await refreshSelectedStorageViewsIfStable(previousSelection)
  }
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => autochangerScopeDirectors.value.join('\u0000'), async () => {
  const previousSelection = selectedStorage.value
  await loadStorages()
  await refreshSelectedStorageViewsIfStable(previousSelection)
})

watch(() => [
  queriedStorageName.value,
  queriedDirectorName.value,
  storagesScopeDirector.value,
  route.query.tab,
], () => {
  if (!autochangerStorages.value.length || !isAutochangerRouteActive.value) {
    return
  }

  syncSelectedStorageFromRoute()
})

watch(selectedStorage, async (next) => {
  if (!next) {
    await syncRouteToSelectedStorage()
    pools.value = []
    allSlots.value = []
    volumeDetailsByName.value = {}
    stopAutoRefresh()
    return
  }

  await syncRouteToSelectedStorage()
  await loadPools()
  await loadSlots()
  startAutoRefresh()
})

watch(isAutochangerRouteActive, async (active) => {
  if (active) {
    syncSelectedStorageFromRoute()
    await syncRouteToSelectedStorage()
  }
})

watch(() => settings.refreshInterval, () => {
  startAutoRefresh()
})

onUnmounted(() => {
  stopAutoRefresh()
})
</script>

<style scoped>
.dnd-drop-target {
  background: rgba(var(--q-primary), 0.12);
  outline: 2px dashed var(--q-primary);
  outline-offset: -2px;
}

.dnd-drop-container {
  position: relative;
}

.dnd-drop-hint {
  position: absolute;
  right: 8px;
  top: 50%;
  transform: translateY(-50%);
  pointer-events: none;
}

.command-log-command,
.command-log-text {
  font-family: monospace;
}

.command-log-text {
  margin: 0;
  padding: 16px;
  white-space: pre-wrap;
}
</style>
