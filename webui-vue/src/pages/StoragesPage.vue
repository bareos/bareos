<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />
    <div v-if="storagesListScopeDirector" class="q-mb-md">
      <DirectorBadge
        removable
        icon="dns"
        :director="storagesListScopeDirector"
        @remove="router.replace({ path: '/storages', query: withStoragesScopeDirectorQuery(route.query, '') })"
      >
        {{ t('Director') }}: {{ storagesListScopeDirector }}
      </DirectorBadge>
    </div>

    <!-- Tab bar: Devices | Pools | Volumes | Autochangers -->
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-route-tab name="storages" :label="t('Devices')"      no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'storages') }" />
      <q-route-tab name="pools"    :label="t('Pools')"        no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'pools') }" />
      <q-route-tab name="volumes"  :label="t('Volumes')"      no-caps :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'volumes') }" />
      <q-route-tab
        name="autochangers"
        :label="t('Autochangers')"
        no-caps
        :to="{ path: '/storages', query: buildStoragesTabQuery(route.query, 'autochangers') }"
      />
    </q-tabs>

    <q-banner v-if="error" dense class="bg-negative text-white q-mb-md">
      {{ error }}
    </q-banner>

    <q-tab-panels v-model="tab" animated :swipeable="$q.platform.has.touch">
      <!-- DEVICES -->
      <q-tab-panel name="storages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">{{ t('Storage Devices') }}</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="storages"
              :columns="storageCols"
              row-key="scopeKey"
              dense
              flat
              :loading="loading"
              :pagination="{ rowsPerPage: 15 }"
            >
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
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
                  <q-btn
                    v-if="props.row.autochanger"
                    flat
                    round
                    dense
                    size="sm"
                    icon="view_carousel"
                    :title="t('Open Autochanger')"
                    @click="openAutochanger(props.row)"
                  />
                  <q-btn flat round dense size="sm" icon="monitor_heart"
                         :title="t('Storage Status')"
                          @click="showStorageStatus(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- POOLS -->
      <q-tab-panel name="pools" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">{{ t('Pools') }}</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="pools"
              :columns="poolCols"
              row-key="scopeKey"
              dense
              flat
              :loading="loading"
              :pagination="{ rowsPerPage: 15 }"
            >
              <template #body-cell-name="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openPoolDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
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
              <template #body-cell-prunablebytes="props">
                <q-td :props="props" class="text-right" style="min-width:120px">
                  <div>{{ Number(props.value) > 0 ? formatBytes(props.value) : '—' }}</div>
                  <div v-if="props.row.prunablejobs > 0" class="text-caption text-grey-6">
                    {{ props.row.prunablejobs }} {{ t('jobs') }} / {{ props.row.prunablevolumes }} {{ t('volumes') }}
                  </div>
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
              <span>{{ t('Volumes') }}</span>
            <q-space />
            <q-input v-model="volSearch" dense outlined :placeholder="t('Search…')" style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="volumes"
              :columns="volumeCols"
              row-key="scopeKey"
              dense
              flat
              :loading="loading"
              :filter="volSearch"
              :pagination="{ rowsPerPage: 15 }"
            >
              <template #body-cell-volumename="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <a href="#" class="text-primary" @click.prevent="openVolumeDetails(props.row)">
                      {{ props.value }}
                    </a>
                    <q-icon
                      v-if="volumeHasEncryptionKey(props.row)"
                      name="vpn_key"
                      size="xs"
                      color="amber-8"
                    >
                      <q-tooltip>{{ t('Encryption key stored in catalog') }}</q-tooltip>
                    </q-icon>
                  </div>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-inchanger="props">
                <q-td :props="props" class="text-center">
                  <BoolIcon :value="props.value" />
                </q-td>
              </template>
              <template #body-cell-pool="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openPoolDetails(props.row)">
                    {{ props.value }}
                  </a>
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
                  <span class="text-grey-5">—</span>
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- AUTOCHANGERS -->
      <q-tab-panel name="autochangers" class="q-pa-none">
        <AutochangerPage embedded />
      </q-tab-panel>
    </q-tab-panels>

    <!-- Storage Status Dialog -->
    <q-dialog v-model="storageStatusDlg.open">
        <q-card style="min-width:600px;max-width:90vw">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Status') }}: {{ storageStatusDlg.name }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white"
                 @click="reloadStorageStatus(storageStatusDlg)"
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
import { ref, computed, onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import {
  fetchAggregatedStoragesState,
  normaliseDirectorStoragesState,
} from '../composables/storagesAggregate.js'
import {
  buildAutochangerSelectionQuery,
  buildStoragesTabQuery,
  resolveStoragesScopeDirector,
  withStoragesScopeDirectorQuery,
} from '../utils/storagesRoute.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { buildPoolDetailsQuery } from '../utils/pools.js'
import { buildVolumeDetailsQuery, volumeHasEncryptionKey } from '../utils/volumes.js'
import AutochangerPage from './AutochangerPage.vue'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import BoolIcon from '../components/BoolIcon.vue'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import EnabledBadge from '../components/EnabledBadge.vue'
import PoolTypeBadge from '../components/PoolTypeBadge.vue'

const route    = useRoute()
const router   = useRouter()
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const { t } = useI18n()
const validTabs = new Set(['storages', 'pools', 'volumes', 'autochangers'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'storages'
}
const tab      = ref(normaliseTab(route.query.tab))
const volSearch = ref('')
const loading = ref(false)
const error = ref(null)
const directorErrors = ref([])
const storageRows = ref([])
const poolRows = ref([])
const volumeRows = ref([])

const reachableDirectors = computed(() => [...new Set([
  ...director.availableDirectors,
  auth.user?.director,
  settings.directorName,
].filter(Boolean))])

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonStorages,
  isSingleDirectorScope,
  scopeLabel: storagesScopeLabel,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({
  t,
  buildOptions: () => reachableDirectors.value.map(value => ({ label: value, value })),
})

const storagesListScopeDirector = computed(() => {
  const requestedDirector = resolveStoragesScopeDirector(route.query)

  if (requestedDirector && activeDirectors.value.includes(requestedDirector)) {
    return requestedDirector
  }

  return ''
})
const storagesPageDirectors = computed(() => (
  storagesListScopeDirector.value ? [storagesListScopeDirector.value] : activeDirectors.value
))
const showDirectorColumn = computed(() => storagesPageDirectors.value.length > 1)

watch(() => route.query.tab, (value) => {
  const next = normaliseTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(tab, (next) => {
  const target = next
  const current = normaliseTab(route.query.tab)
  if (current === target) {
    return
  }

  router.replace({ path: '/storages', query: buildStoragesTabQuery(route.query, target) })
})

watch(() => route.query.scopeDirector, (value) => {
  if (typeof value === 'string' && value && !activeDirectors.value.includes(value)) {
    router.replace({
      path: '/storages',
      query: withStoragesScopeDirectorQuery(route.query, ''),
    })
    return
  }

  refresh()
})

async function switchToRowDirector(row) {
  if (!row?.director) {
    return
  }

  await ensureScopeDirector(row.director)
}

function reportRowError(row, reason) {
  directorErrors.value = [{
    director: row?.director ?? t('unknown'),
    message: reason?.message ?? String(reason),
  }]
}

const pools = computed(() => {
  return poolRows.value
})

const maxPoolBytes = computed(() =>
  Math.max(1, ...pools.value.map(p => p.totalbytes))
)
function poolBytesGauge(val) { return (Number(val) || 0) / maxPoolBytes.value }

const volumes = computed(() => {
  return volumeRows.value
})

const storages = computed(() => storageRows.value)

async function refresh() {
  loading.value = true
  error.value = null
  directorErrors.value = []
  try {
    if (storagesPageDirectors.value.length === 0) {
      storageRows.value = []
      poolRows.value = []
      volumeRows.value = []
      return
    }

    if (storagesPageDirectors.value.length > 1 || (storagesListScopeDirector.value && isCommonStorages.value)) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedStoragesState(credentials, storagesPageDirectors.value)
      storageRows.value = result.storages
      poolRows.value = result.pools
      volumeRows.value = result.volumes
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = storagesPageDirectors.value[0]
    await ensureSingleScopeDirector()
    const [storagesResult, poolsResult, volumesResult] = await Promise.all([
      director.call('list storages'),
      director.call('llist pools'),
      director.call('llist volumes'),
    ])
    const result = normaliseDirectorStoragesState(
      currentDirector,
      storagesResult?.storages,
      poolsResult?.pools,
      volumesResult?.volumes
    )
    storageRows.value = result.storages
    poolRows.value = result.pools
    volumeRows.value = result.volumes
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  } finally {
    loading.value = false
  }
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
function volGauge(val, max) { return (Number(val) || 0) / (max || 1) }

const poolCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',         label: t('Name'),          field: 'name',         align: 'left',  sortable: true },
  { name: 'pooltype',     label: t('Type'),          field: 'pooltype',     align: 'left',  sortable: true },
  { name: 'numvols',      label: t('Volumes'),       field: 'numvols',      align: 'right', sortable: true },
  { name: 'maxvols',      label: t('Max Volumes'),   field: 'maxvols',      align: 'right', sortable: true },
  { name: 'totalbytes',   label: t('Total Data'),    field: 'totalbytes',   align: 'right', sortable: true },
  { name: 'prunablebytes',label: t('Prunable Now'),  field: 'prunablebytes',align: 'right', sortable: true },
  { name: 'volretention', label: t('Retention'),     field: 'volretention', align: 'left',  sortable: true },
  { name: 'maxvoljobs',   label: t('Max Jobs/Vol'),  field: 'maxvoljobs',   align: 'right', sortable: true },
  { name: 'maxvolbytes',  label: t('Max Bytes/Vol'), field: 'maxvolbytes',  align: 'right', sortable: true },
])
const volumeCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'volumename',  label: t('Volume Name'),  field: 'volumename',  align: 'left',  sortable: true },
  { name: 'pool',        label: t('Pool'),         field: 'pool',        align: 'left',  sortable: true },
  { name: 'storage',     label: t('Storage'),      field: 'storage',     align: 'left',  sortable: true },
  { name: 'mediatype',   label: t('Media Type'),   field: 'mediatype',   align: 'left',  sortable: true },
  { name: 'lastwritten', label: t('Last Written'), field: 'lastwritten', align: 'left',  sortable: true },
  { name: 'volstatus',   label: t('Status'),       field: 'volstatus',   align: 'center', sortable: true },
  { name: 'inchanger',   label: t('In Changer'),   field: 'inchanger',   align: 'center', sortable: true },
  { name: 'retention',   label: t('Retention'),    field: 'retention',   align: 'left',  sortable: true },
  { name: 'maxvolbytes', label: t('Max Bytes'),    field: 'maxvolbytes', align: 'right', sortable: true },
  { name: 'volbytes',    label: t('Used Bytes'),   field: 'volbytes',    align: 'right', sortable: true },
  { name: 'actions',     label: '',                field: 'actions',     align: 'center', style: 'width:40px' },
])

const storageCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',        label: t('Name'),        field: 'name',        align: 'left',  sortable: true },
  { name: 'address',     label: t('Address'),     field: 'address',     align: 'left',  sortable: true },
  { name: 'mediatype',   label: t('Media Type'),  field: 'mediatype',   align: 'left',  sortable: true },
  { name: 'autochanger', label: t('Autochanger'), field: 'autochanger', align: 'center', sortable: true },
  { name: 'enabled',     label: t('Status'),      field: 'enabled',     align: 'center', sortable: true },
  { name: 'actions',     label: '',               field: 'actions',     align: 'center', style: 'width:110px' },
])

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative',
           Purged: 'grey', Used: 'orange', 'Read-Only': 'blue-grey', Cleaning: 'teal' }[s] || 'info'
}

// ── Storage Status Dialog ─────────────────────────────────────────────────────
const storageStatusDlg = ref({
  open: false,
  name: '',
  director: '',
  loading: false,
  error: null,
  text: '',
})

async function showStorageStatus(storage) {
  storageStatusDlg.value = {
    open: true,
    name: storage.name,
    director: storage.director ?? '',
    loading: true,
    error: null,
    text: '',
  }
  await reloadStorageStatus(storage)
}

async function openAutochanger(storage) {
  try {
    await router.push({
      path: '/storages',
      query: buildAutochangerSelectionQuery(route.query, storage),
    })
  } catch (reason) {
    reportRowError(storage, reason)
  }
}

async function reloadStorageStatus(storage) {
  storageStatusDlg.value.loading = true
  storageStatusDlg.value.error   = null
  try {
    await switchToRowDirector(storage)
    const r = await director.rawCall(
      `status storage=${quoteDirectorString(storage.name)}`
    )
    storageStatusDlg.value.text = r
  } catch (e) {
    storageStatusDlg.value.error = e.message
  } finally {
    storageStatusDlg.value.loading = false
  }
}

async function openPoolDetails(pool) {
  try {
    await switchToRowDirector(pool)
    await router.push({
      name: 'pool-details',
      params: { name: pool.name },
      query: buildPoolDetailsQuery({
        director: pool.director,
        storagesTab: tab.value,
        storagesScopeDirector: pool.director,
      }),
    })
  } catch (reason) {
    reportRowError(pool, reason)
  }
}

async function openVolumeDetails(volume) {
  try {
    await switchToRowDirector(volume)
    await router.push({
      name: 'volume-details',
      params: { name: volume.volumename },
      query: buildVolumeDetailsQuery({
        director: volume.director,
        storagesTab: tab.value,
        storagesScopeDirector: volume.director,
      }),
    })
  } catch (reason) {
    reportRowError(volume, reason)
  }
}

onMounted(() => {
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  refresh()
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  if (typeof route.query.scopeDirector === 'string'
    && route.query.scopeDirector
    && !activeDirectors.value.includes(route.query.scopeDirector)) {
    router.replace({
      path: '/storages',
      query: withStoragesScopeDirectorQuery(route.query, ''),
    })
  }
  refresh()
})
</script>
