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
  <q-page class="q-pa-md">
    <!-- ── Toolbar ─────────────────────────────────────────── -->
    <div class="row items-center q-gutter-sm q-mb-md">
      <q-select
        v-model="selectedStorage"
        :options="autochangerStorages"
        option-label="name"
        option-value="name"
        emit-value
        map-options
        label="Autochanger"
        outlined
        dense
        style="min-width:200px"
        :loading="storagesLoading"
        @update:model-value="loadSlots"
      />

      <q-btn flat round dense icon="refresh" title="Refresh"
             :loading="slotsLoading" @click="loadSlots" />

      <q-space />

      <q-btn outline color="primary" icon="sync_alt" label="Update Slots"
             :disable="!selectedStorage" @click="doUpdateSlots" />

      <q-btn outline color="primary" icon="label" label="Label Barcodes"
             :disable="!selectedStorage" @click="labelDialog = true" />

      <q-btn outline color="primary" icon="monitor_heart" label="Status"
             :disable="!selectedStorage" @click="showStatus" />
    </div>

    <!-- No autochanger storages -->
    <q-banner v-if="!storagesLoading && autochangerStorages.length === 0"
              class="bg-warning text-white q-mb-md" rounded>
      No autochanger storages configured.
    </q-banner>

    <!-- ── Slot Tables ────────────────────────────────────── -->
    <div v-if="selectedStorage" class="row q-col-gutter-md">

      <!-- Import/Export Slots -->
      <div class="col-12 col-md-4">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center no-wrap">
            <span class="col">Import/Export Slots</span>
            <q-btn flat round dense size="sm" icon="download"
                   title="Import all" @click="doImportAll" />
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
              <template #body-cell-content="props">
                <q-td :props="props">
                  <q-badge :color="props.value === 'full' ? 'green' : 'grey'"
                           :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                  <q-btn v-if="props.row.content === 'full'"
                         flat round dense size="sm" icon="download"
                         title="Import"
                         @click="doImport(props.row.slotnr)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </div>

      <!-- Drives -->
      <div class="col-12 col-md-4">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Drives</q-card-section>
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
              <template #body-cell-content="props">
                <q-td :props="props">
                  <q-badge :color="props.value === 'full' ? 'green' : 'grey'"
                           :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                  <template v-if="props.row.content === 'full'">
                    <q-btn flat round dense size="sm" icon="eject"
                           title="Unmount"
                           @click="doUnmount(props.row.slotnr)" />
                    <q-btn flat round dense size="sm" icon="stop_circle"
                           title="Release"
                           @click="doRelease(props.row.slotnr)" />
                  </template>
                  <q-btn v-else
                         flat round dense size="sm" icon="play_circle"
                         title="Mount"
                         @click="openMountDialog(props.row.slotnr)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </div>

      <!-- Storage Slots -->
      <div class="col-12 col-md-4">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Storage Slots</q-card-section>
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
              style="max-height: 60vh"
            >
              <template #body-cell-content="props">
                <q-td :props="props">
                  <q-badge :color="props.value === 'full' ? 'green' : 'grey'"
                           :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-mr_volname="props">
                <q-td :props="props">
                  <router-link
                    v-if="props.value && props.value !== '?'"
                    :to="{ name: 'volume-details', params: { name: props.value } }"
                    class="text-primary"
                  >{{ props.value }}</router-link>
                </q-td>
              </template>
              <template #body-cell-mr_volstatus="props">
                <q-td :props="props">
                  <span v-if="props.value && props.value !== '?'">
                    {{ props.value }}
                  </span>
                </q-td>
              </template>
              <template #body-cell-pr_name="props">
                <q-td :props="props">
                  <router-link
                    v-if="props.value && props.value !== '?'"
                    :to="{ name: 'pool-details', params: { name: props.value } }"
                    class="text-primary"
                  >{{ props.value }}</router-link>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-right">
                  <q-btn v-if="props.row.content === 'full'"
                         flat round dense size="sm" icon="upload"
                         title="Export"
                         @click="doExport(props.row.slotnr)" />
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
          <span class="text-h6">Label Barcodes</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-select v-model="labelForm.pool"
                    :options="poolNames"
                    label="Pool" outlined dense />
          <q-input v-model="labelForm.drive"
                   label="Drive (number)" outlined dense type="number"
                   :min="0" />
          <q-input v-model="labelForm.slots"
                   label='Slots (e.g. "1-10" or leave blank)'
                   outlined dense />
        </q-card-section>
        <q-card-actions align="right">
          <q-btn flat label="Cancel" v-close-popup />
          <q-btn color="primary" label="Label" @click="doLabelBarcodes" />
        </q-card-actions>
      </q-card>
    </q-dialog>

    <!-- ── Mount Dialog ───────────────────────────────────── -->
    <q-dialog v-model="mountDialog">
      <q-card style="min-width:300px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">Mount Tape</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section class="q-gutter-sm">
          <q-input v-model="mountForm.slot"
                   label="Slot number" outlined dense type="number"
                   :min="1" />
        </q-card-section>
        <q-card-actions align="right">
          <q-btn flat label="Cancel" v-close-popup />
          <q-btn color="primary" label="Mount" @click="doMount" />
        </q-card-actions>
      </q-card>
    </q-dialog>

    <!-- ── Result Dialog ─────────────────────────────────── -->
    <q-dialog v-model="resultDialog">
      <q-card style="min-width:480px; max-width:720px">
        <q-card-section class="row items-center q-pb-none">
          <span class="text-h6">{{ resultTitle }}</span>
          <q-space />
          <q-btn icon="close" flat round dense v-close-popup />
        </q-card-section>
        <q-card-section>
          <q-scroll-area style="height:300px">
            <pre class="text-caption" style="white-space:pre-wrap">{{ resultText }}</pre>
          </q-scroll-area>
        </q-card-section>
        <q-card-actions align="right">
          <q-btn flat label="Close" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue'
import { useDirectorStore } from '../stores/director.js'
import { useQuasar } from 'quasar'

const director = useDirectorStore()
const $q = useQuasar()

// ── State ───────────────────────────────────────────────────

const autochangerStorages = ref([])
const storagesLoading = ref(false)
const selectedStorage = ref(null)

const allSlots = ref([])
const slotsLoading = ref(false)

const pools = ref([])

const labelDialog = ref(false)
const labelForm = ref({ pool: '', drive: 0, slots: '' })

const mountDialog = ref(false)
const mountForm = ref({ drive: 0, slot: '' })
const mountDrive = ref(0)

const resultDialog = ref(false)
const resultTitle = ref('')
const resultText = ref('')

// ── Computed ─────────────────────────────────────────────────

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

// ── Column definitions ────────────────────────────────────────

const ieSlotCols = [
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
  { name: 'slotnr',      label: 'Slot',    field: 'slotnr',      align: 'left',  sortable: true },
  { name: 'content',     label: 'Status',  field: 'content',     align: 'left' },
  { name: 'mr_volname',  label: 'Volume',  field: 'mr_volname',  align: 'left' },
  { name: 'mr_volstatus',label: 'Vol Status', field: 'mr_volstatus', align: 'left' },
  { name: 'pr_name',     label: 'Pool',    field: 'pr_name',     align: 'left' },
  { name: 'actions',     label: '',        field: 'actions',     align: 'right' },
]

// ── Data Loading ──────────────────────────────────────────────

async function loadStorages() {
  storagesLoading.value = true
  try {
    const res = await director.call('list storages')
    const list = res?.storages ?? []
    autochangerStorages.value = list.filter(
      s => String(s.autochanger) === '1'
    )
    if (autochangerStorages.value.length > 0 && !selectedStorage.value) {
      selectedStorage.value = autochangerStorages.value[0].name
    }
  } catch (e) {
    console.error('Failed to list storages:', e)
  } finally {
    storagesLoading.value = false
  }
}

async function loadPools() {
  try {
    const res = await director.call('list pools')
    pools.value = res?.pools ?? []
  } catch (e) {
    console.error('Failed to list pools:', e)
  }
}

async function loadSlots() {
  if (!selectedStorage.value) return
  slotsLoading.value = true
  try {
    const res = await director.call(
      `status storage="${selectedStorage.value}" slots`
    )
    allSlots.value = res?.contents ?? []
  } catch (e) {
    console.error('Failed to load slots:', e)
    allSlots.value = []
  } finally {
    slotsLoading.value = false
  }
}

// ── Actions ───────────────────────────────────────────────────

async function runCmd(title, cmd) {
  try {
    const result = await director.rawCall(cmd)
    showResult(title, result)
  } catch (e) {
    showResult(title, `Error: ${e.message}`)
  }
}

async function doUpdateSlots() {
  await runCmd(
    'Update Slots',
    `update slots storage="${selectedStorage.value}"`
  )
  await loadSlots()
}

async function doImportAll() {
  await runCmd(
    'Import All',
    `import storage="${selectedStorage.value}"`
  )
  await loadSlots()
}

async function doImport(slotNr) {
  await runCmd(
    `Import Slot ${slotNr}`,
    `import storage="${selectedStorage.value}" srcslots=${slotNr}`
  )
  await loadSlots()
}

async function doExport(slotNr) {
  $q.dialog({
    title: 'Export Slot',
    message: `Export volume from slot ${slotNr}?`,
    ok: { color: 'primary', label: 'Export' },
    cancel: true,
  }).onOk(async () => {
    await runCmd(
      `Export Slot ${slotNr}`,
      `export storage="${selectedStorage.value}" srcslots=${slotNr}`
    )
    await loadSlots()
  })
}

function openMountDialog(driveNr) {
  mountDrive.value = driveNr
  mountForm.value = { slot: '' }
  mountDialog.value = true
}

async function doMount() {
  mountDialog.value = false
  await runCmd(
    'Mount',
    `mount storage="${selectedStorage.value}"` +
    ` slot=${mountForm.value.slot} drive=${mountDrive.value}`
  )
  await loadSlots()
}

async function doUnmount(driveNr) {
  await runCmd(
    `Unmount Drive ${driveNr}`,
    `unmount storage="${selectedStorage.value}" drive=${driveNr}`
  )
  await loadSlots()
}

async function doRelease(driveNr) {
  await runCmd(
    `Release Drive ${driveNr}`,
    `release storage="${selectedStorage.value}" drive=${driveNr}`
  )
  await loadSlots()
}

async function doLabelBarcodes() {
  labelDialog.value = false
  const { pool, drive, slots } = labelForm.value
  let cmd = `label barcodes storage="${selectedStorage.value}"`
  if (pool) cmd += ` pool="${pool}"`
  cmd += ` drive=${drive ?? 0}`
  if (slots) cmd += ` slots=${slots}`
  cmd += ' yes'
  await runCmd('Label Barcodes', cmd)
  await loadSlots()
}

async function showStatus() {
  await runCmd(
    `Status: ${selectedStorage.value}`,
    `status storage="${selectedStorage.value}"`
  )
}

function showResult(title, text) {
  resultTitle.value = title
  resultText.value = typeof text === 'string' ? text : JSON.stringify(text, null, 2)
  resultDialog.value = true
}

// ── Lifecycle ─────────────────────────────────────────────────

onMounted(async () => {
  await loadStorages()
  await loadPools()
  if (selectedStorage.value) {
    await loadSlots()
  }
})

watch(() => director.status, async (s) => {
  if (s === 'connected') {
    await loadStorages()
    await loadPools()
    if (selectedStorage.value) await loadSlots()
  }
})
</script>
