<template>
  <q-page class="q-pa-md">
    <!-- Tab bar: Devices | Pools | Volumes -->
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="storages" label="Devices"  no-caps />
      <q-tab name="pools"    label="Pools"    no-caps />
      <q-tab name="volumes"  label="Volumes"  no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- DEVICES -->
      <q-tab-panel name="storages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Storage Devices</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="storages" :columns="storageCols" row-key="name" dense flat :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-autochanger="props">
                <q-td :props="props" class="text-center">
                  <BoolIcon :value="props.value" />
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <EnabledBadge :enabled="props.value" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm" icon="monitor_heart"
                         title="Storage Status"
                         @click="showStorageStatus(props.row.name)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- POOLS -->
      <q-tab-panel name="pools" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Pools</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="pools" :columns="poolCols" row-key="name" dense flat :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-name="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'pool-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-pooltype="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <PoolTypeBadge :type="props.value" />
                    <span>{{ props.value }}</span>
                  </div>
                </q-td>
              </template>
              <template #body-cell-numvols="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ props.value }}</div>
                  <q-linear-progress
                    :value="poolGauge(props.value, maxNumVols)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-maxvols="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ Number(props.value) > 0 ? props.value : '∞' }}</div>
                  <q-linear-progress v-if="Number(props.value) > 0"
                    :value="poolGauge(props.value, maxMaxVols)"
                    color="grey-6" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-volretention="props">
                <q-td :props="props" style="min-width:90px">
                  <div>{{ formatDuration(props.value) }}</div>
                  <q-linear-progress
                    :value="poolGauge(Number(props.value), maxRetentionSecs)"
                    color="orange" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-maxvoljobs="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ Number(props.value) > 0 ? props.value : '∞' }}</div>
                  <q-linear-progress v-if="Number(props.value) > 0"
                    :value="poolGauge(props.value, maxMaxVolJobs)"
                    color="deep-purple" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-maxvolbytes="props">
                <q-td :props="props" class="text-right" style="min-width:100px">
                  <div>{{ Number(props.value) > 0 ? formatBytes(props.value) : '∞' }}</div>
                  <q-linear-progress v-if="Number(props.value) > 0"
                    :value="poolGauge(props.value, maxMaxVolBytes)"
                    color="teal" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-totalbytes="props">
                <q-td :props="props" class="text-right" style="min-width:110px">
                  <div>{{ formatBytes(props.value) }}</div>
                  <q-linear-progress
                    :value="poolBytesGauge(props.value)"
                    color="cyan-7" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- VOLUMES -->
      <q-tab-panel name="volumes" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Volumes</span>
            <q-space />
            <q-input v-model="volSearch" dense outlined placeholder="Search…" style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="volumes" :columns="volumeCols" row-key="volumename" dense flat
                     :filter="volSearch" :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-volumename="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'volume-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-inchanger="props">
                <q-td :props="props" class="text-center">
                  <BoolIcon :value="props.value" />
                </q-td>
              </template>
              <template #body-cell-pool="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'pool-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-volstatus="props">
                <q-td :props="props">
                  <q-badge :color="statusColor(props.value)" :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-volbytes="props">
                <q-td :props="props" class="text-right" style="min-width:100px">
                  <div>{{ formatBytes(props.value) }}</div>
                  <q-linear-progress
                    :value="volBytesGauge(props.value)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-maxvolbytes="props">
                <q-td :props="props" class="text-right" style="min-width:100px">
                  <div>{{ Number(props.value) > 0 ? formatBytes(props.value) : '∞' }}</div>
                  <q-linear-progress v-if="Number(props.value) > 0"
                    :value="volGauge(props.value, maxVolMaxBytes)"
                    color="teal" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-retention="props">
                <q-td :props="props" style="min-width:90px">
                  <div>{{ formatDuration(props.value) }}</div>
                  <q-linear-progress
                    :value="volGauge(Number(props.value), maxVolRetention)"
                    color="orange" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm" icon="edit" title="Change status">
                    <q-menu anchor="bottom right" self="top right" auto-close>
                      <q-list dense style="min-width:130px">
                        <q-item-label header class="text-caption">Set status</q-item-label>
                        <q-item
                          v-for="s in volStatuses" :key="s"
                          clickable
                          :active="props.row.volstatus === s"
                          @click="setVolStatus(props.row, s)"
                        >
                          <q-item-section>
                            <q-badge :color="statusColor(s)" :label="s" />
                          </q-item-section>
                        </q-item>
                      </q-list>
                    </q-menu>
                  </q-btn>
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Status-change result snackbar -->
        <q-dialog v-model="statusMsg.show" position="bottom">
          <q-card :class="statusMsg.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
                  style="min-width:300px">
            <q-card-section class="row items-center q-py-sm">
              <q-icon :name="statusMsg.ok ? 'check_circle' : 'error'" class="q-mr-sm" />
              <span>{{ statusMsg.text }}</span>
              <q-space />
              <q-btn flat round dense icon="close" v-close-popup />
            </q-card-section>
          </q-card>
        </q-dialog>
      </q-tab-panel>
    </q-tab-panels>

    <!-- Storage Status Dialog -->
    <q-dialog v-model="storageStatusDlg.open">
      <q-card style="min-width:600px;max-width:90vw">
        <q-card-section class="panel-header row items-center">
          <span>Status: {{ storageStatusDlg.name }}</span>
          <q-space />
          <q-btn flat round dense icon="refresh" color="white"
                 @click="reloadStorageStatus(storageStatusDlg.name)"
                 :loading="storageStatusDlg.loading" />
          <q-btn flat round dense icon="close" color="white" v-close-popup class="q-ml-xs" />
        </q-card-section>
        <q-card-section>
          <q-inner-loading :showing="storageStatusDlg.loading" />
          <div v-if="storageStatusDlg.error" class="text-negative">{{ storageStatusDlg.error }}</div>
          <div v-else class="console-output">{{ storageStatusDlg.text }}</div>
        </q-card-section>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRoute, RouterLink } from 'vue-router'
import {
  directorCollection,
  useDirectorFetch,
  normaliseVolume,
} from '../composables/useDirectorFetch.js'
import { useDirectorStore } from '../stores/director.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import BoolIcon from '../components/BoolIcon.vue'
import EnabledBadge from '../components/EnabledBadge.vue'
import PoolTypeBadge from '../components/PoolTypeBadge.vue'

const route    = useRoute()
const director = useDirectorStore()
const tab      = ref(route.query.tab || 'storages')
const volSearch = ref('')

const { data: rawStorages, loading: storagesLoading, error: storagesError, refresh: refreshStorages } =
  useDirectorFetch('list storages', 'storages')
const { data: rawPools,    loading: poolsLoading,    error: poolsError,    refresh: refreshPools } =
  useDirectorFetch('llist pools', 'pools')
const { data: rawVolumes,  loading: volsLoading,     error: volsError,     refresh: refreshVols } =
  useDirectorFetch('llist volumes', 'volumes')

const storages = computed(() => directorCollection(rawStorages.value).map(s => ({
  ...s,
  autochanger: s.autochanger === '1' || s.autochanger === true,
  enabled:     s.enabled !== '0' && s.enabled !== false,
})))

const pools = computed(() => {
  const bytesByPool = {}
  for (const v of volumes.value) {
    const key = v.pool ?? ''
    bytesByPool[key] = (bytesByPool[key] ?? 0) + (Number(v.volbytes) || 0)
  }
  return directorCollection(rawPools.value).map(p => ({
    ...p,
    numvols:    Number(p.numvols ?? 0),
    maxvols:    Number(p.maxvols ?? 0),
    totalbytes: bytesByPool[p.name] ?? 0,
  }))
})

const maxPoolBytes = computed(() =>
  Math.max(1, ...pools.value.map(p => p.totalbytes))
)
function poolBytesGauge(val) { return (Number(val) || 0) / maxPoolBytes.value }

// "list volumes" returns a nested object keyed by pool type; flatten it.
const volumes = computed(() => {
  const raw = rawVolumes.value
  if (!raw) return []
  if (Array.isArray(raw)) return raw.map(normaliseVolume)
  // Nested by pool: { full: [...], incremental: [...], ... }
  return Object.values(raw).flat().map(normaliseVolume)
})

function refresh() {
  refreshStorages(); refreshPools(); refreshVols()
}

// ── Pool gauge helpers ────────────────────────────────────────────────────────
function maxOf(arr, field) {
  return Math.max(1, ...arr.map(p => Number(p[field]) || 0))
}
const maxNumVols      = computed(() => maxOf(pools.value, 'numvols'))
const maxMaxVols      = computed(() => maxOf(pools.value.filter(p => Number(p.maxvols) > 0), 'maxvols'))
const maxRetentionSecs= computed(() => maxOf(pools.value, 'volretention'))
const maxMaxVolJobs   = computed(() => maxOf(pools.value.filter(p => Number(p.maxvoljobs) > 0), 'maxvoljobs'))
const maxMaxVolBytes  = computed(() => maxOf(pools.value.filter(p => Number(p.maxvolbytes) > 0), 'maxvolbytes'))
function poolGauge(val, max) { return (Number(val) || 0) / (max || 1) }

// ── Volume gauge helpers ──────────────────────────────────────────────────────
const maxVolBytes     = computed(() => maxOf(volumes.value, 'volbytes'))
const maxVolMaxBytes  = computed(() => maxOf(volumes.value.filter(v => Number(v.maxvolbytes) > 0), 'maxvolbytes'))
const maxVolRetention = computed(() => maxOf(volumes.value, 'retention'))
function volBytesGauge(val) { return (Number(val) || 0) / maxVolBytes.value }
function volGauge(val, max)  { return (Number(val) || 0) / (max || 1) }

const poolCols = [
  { name: 'name',         label: 'Name',          field: 'name',         align: 'left',  sortable: true },
  { name: 'pooltype',     label: 'Type',          field: 'pooltype',     align: 'left'   },
  { name: 'numvols',      label: 'Volumes',       field: 'numvols',      align: 'right', sortable: true },
  { name: 'maxvols',      label: 'Max Volumes',   field: 'maxvols',      align: 'right'  },
  { name: 'totalbytes',   label: 'Total Data',    field: 'totalbytes',   align: 'right', sortable: true },
  { name: 'volretention', label: 'Retention',     field: 'volretention', align: 'left'   },
  { name: 'maxvoljobs',   label: 'Max Jobs/Vol',  field: 'maxvoljobs',   align: 'right'  },
  { name: 'maxvolbytes',  label: 'Max Bytes/Vol', field: 'maxvolbytes',  align: 'right'  },
]
const volumeCols = [
  { name: 'volumename',  label: 'Volume Name',  field: 'volumename',  align: 'left',  sortable: true },
  { name: 'pool',        label: 'Pool',         field: 'pool',        align: 'left',  sortable: true },
  { name: 'storage',     label: 'Storage',      field: 'storage',     align: 'left'   },
  { name: 'mediatype',   label: 'Media Type',   field: 'mediatype',   align: 'left'   },
  { name: 'lastwritten', label: 'Last Written', field: 'lastwritten', align: 'left',  sortable: true },
  { name: 'volstatus',   label: 'Status',       field: 'volstatus',   align: 'center', sortable: true },
  { name: 'inchanger',   label: 'In Changer',   field: 'inchanger',   align: 'center' },
  { name: 'retention',   label: 'Retention',    field: 'retention',   align: 'left'   },
  { name: 'maxvolbytes', label: 'Max Bytes',    field: 'maxvolbytes', align: 'right'  },
  { name: 'volbytes',    label: 'Used Bytes',   field: 'volbytes',    align: 'right'  },
  { name: 'actions',     label: '',             field: 'actions',     align: 'center', style: 'width:40px' },
]

const volStatuses = ['Append', 'Full', 'Used', 'Purged', 'Recycled', 'Read-Only', 'Error', 'Cleaning']

const statusMsg = ref({ show: false, ok: true, text: '' })

async function setVolStatus(vol, newStatus) {
  try {
    await director.call(
      `update volume=${vol.volumename} volstatus=${newStatus}`
    )
    vol.volstatus = newStatus
    statusMsg.value = {
      show: true, ok: true,
      text: `${vol.volumename} → ${newStatus}`,
    }
  } catch (e) {
    statusMsg.value = { show: true, ok: false, text: e.message }
  }
}
const storageCols = [
  { name: 'name',        label: 'Name',        field: 'name',        align: 'left',  sortable: true },
  { name: 'address',     label: 'Address',     field: 'address',     align: 'left'   },
  { name: 'mediatype',   label: 'Media Type',  field: 'mediatype',   align: 'left'   },
  { name: 'autochanger', label: 'Autochanger', field: 'autochanger', align: 'center' },
  { name: 'enabled',     label: 'Status',      field: 'enabled',     align: 'center' },
  { name: 'actions',     label: '',            field: 'actions',     align: 'center', style: 'width:80px' },
]

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative',
           Purged: 'grey', Used: 'orange', 'Read-Only': 'blue-grey', Cleaning: 'teal' }[s] || 'info'
}

// ── Storage Status Dialog ─────────────────────────────────────────────────────
const storageStatusDlg = ref({ open: false, name: '', loading: false, error: null, text: '' })

async function showStorageStatus(name) {
  storageStatusDlg.value = { open: true, name, loading: true, error: null, text: '' }
  await reloadStorageStatus(name)
}

async function reloadStorageStatus(name) {
  storageStatusDlg.value.loading = true
  storageStatusDlg.value.error   = null
  try {
    const r = await director.rawCall(`status storage=${name}`)
    storageStatusDlg.value.text = r
  } catch (e) {
    storageStatusDlg.value.error = e.message
  } finally {
    storageStatusDlg.value.loading = false
  }
}
</script>
