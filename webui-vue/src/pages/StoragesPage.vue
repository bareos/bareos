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

    <q-banner v-if="error" dense rounded class="bg-negative text-white q-mb-md">
      {{ error }}
    </q-banner>

    <q-card flat bordered class="bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Storages') }}</span>
        <q-space />
        <q-input v-model="deviceSearch" dense outlined :placeholder="t('Search…')" style="width:200px" clearable data-testid="storages-device-search">
          <template #prepend><q-icon name="search" /></template>
        </q-input>
        <ColumnPickerMenu :columns="toggleableStorageCols" @toggle="toggleStorageCol" />
      </q-card-section>
      <q-card-section class="q-py-sm storages-list-stats">
        <div class="row items-center q-gutter-sm">
          <q-chip
            dense square outline color="grey-8"
            icon="dns"
            clickable
            :selected="deviceQuickFilter === 'all'"
            @click="deviceQuickFilter = 'all'"
          >
            {{ t('Total') }}: {{ deviceStats.total }}
          </q-chip>
          <q-chip
            dense square outline color="positive"
            icon="check_circle"
            clickable
            :selected="deviceQuickFilter === 'enabled'"
            @click="deviceQuickFilter = 'enabled'"
          >
            {{ t('Enabled') }}: {{ deviceStats.enabled }}
          </q-chip>
          <q-chip
            dense square outline color="negative"
            icon="pause_circle"
            clickable
            :selected="deviceQuickFilter === 'disabled'"
            @click="deviceQuickFilter = 'disabled'"
          >
            {{ t('Disabled') }}: {{ deviceStats.disabled }}
          </q-chip>
          <q-chip
            v-if="deviceStats.autochanger"
            dense square outline color="info"
            icon="mdi-robot-industrial"
            clickable
            :selected="deviceQuickFilter === 'autochanger'"
            @click="deviceQuickFilter = 'autochanger'"
          >
            {{ t('Autochanger') }}: {{ deviceStats.autochanger }}
          </q-chip>
        </div>
      </q-card-section>
      <q-card-section class="q-pa-none">
        <q-table
          v-if="!(loading && !storages.length)"
          :rows="storages"
          :columns="visibleStorageCols"
          row-key="scopeKey"
          dense
          flat
          :loading="loading"
          :filter="deviceSearch"
          :row-class="storageRowClass"
          v-model:pagination="devicesPagination"
          @row-click="(event, row) => toggleSelectedStorage(row)"
        >
          <template #body-cell-name="props">
            <q-td :props="props">
              <div class="row items-center no-wrap q-gutter-xs">
                <q-icon
                  :name="props.row.autochanger ? 'mdi-robot-industrial' : 'mdi-harddisk'"
                  :color="props.row.autochanger ? 'info' : 'grey-7'"
                  size="xs"
                  role="img"
                  :aria-label="props.row.autochanger ? t('Autochanger') : t('Single device')"
                  :data-testid="props.row.autochanger ? 'storage-icon-autochanger' : 'storage-icon-single'"
                >
                  <q-tooltip>{{ props.row.autochanger ? t('Autochanger') : t('Single device') }}</q-tooltip>
                </q-icon>
                <span data-testid="storages-row-name">{{ props.value }}</span>
              </div>
            </q-td>
          </template>
          <template #body-cell-address="props">
            <q-td :props="props">
              <span v-if="props.value">{{ props.value }}<span v-if="props.row.port" class="text-grey-6">:{{ props.row.port }}</span></span>
              <span v-else class="text-grey-5">—</span>
            </q-td>
          </template>
          <template #body-cell-mediatype="props">
            <q-td :props="props">
              <span v-if="props.value">{{ props.value }}</span>
              <span v-else class="text-grey-5">—</span>
            </q-td>
          </template>
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
        </q-table>
        <TableSkeleton v-else :columns="visibleStorageCols.length" :rows="6" />
      </q-card-section>
    </q-card>

    <!-- Detail panel for the selected storage -->
    <div class="q-mt-md">
      <q-card
        v-if="!selectedStorage"
        flat
        bordered
        class="bareos-panel storages-detail-hint"
        data-testid="storages-detail-hint"
      >
        <q-card-section class="text-center text-grey-6">
          <q-icon name="mdi-robot-industrial" size="32px" class="block q-mx-auto q-mb-sm" />
          {{ t('Select a storage to manage it.') }}
        </q-card-section>
      </q-card>

      <div v-else data-testid="storages-detail">
        <div class="row items-center q-gutter-sm q-mb-sm">
          <q-icon
            :name="selectedStorage.autochanger ? 'mdi-robot-industrial' : 'mdi-harddisk'"
            :color="selectedStorage.autochanger ? 'info' : 'grey-7'"
            size="sm"
          />
          <span class="text-h6" data-testid="storages-detail-name">{{ selectedStorage.name }}</span>
          <q-space />
          <q-btn
            flat round dense icon="close"
            :title="t('Close')" :aria-label="t('Close')"
            data-testid="storages-detail-close"
            @click="selectStorage(null)"
          />
        </div>

        <AutochangerPanel
          v-if="selectedStorage.autochanger"
          :storage="selectedStorage"
          :show-director="showDirectorColumn"
        />
        <StorageDetailsPanel
          v-else
          :storage="selectedStorage"
          :show-director="showDirectorColumn"
        />
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute, useRouter } from 'vue-router'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { usePersistedTablePagination } from '../composables/usePersistedTablePagination.js'
import { usePersistedTableFilter } from '../composables/usePersistedTableFilter.js'
import { usePersistedTableColumns } from '../composables/usePersistedTableColumns.js'
import {
  fetchAggregatedStorages,
  fetchDirectorStorages,
} from '../composables/storagesAggregate.js'
import {
  STORAGE_SELECTION_QUERY_KEY,
  resolveAutochangerSelection,
  resolveStorageSelectionQuery,
  resolveStoragesScopeDirector,
  withStorageSelectionQuery,
  withStoragesScopeDirectorQuery,
} from '../utils/storagesRoute.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import BoolIcon from '../components/BoolIcon.vue'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import EnabledBadge from '../components/EnabledBadge.vue'
import ColumnPickerMenu from '../components/ColumnPickerMenu.vue'
import TableSkeleton from '../components/TableSkeleton.vue'
import AutochangerPanel from '../components/AutochangerPanel.vue'
import StorageDetailsPanel from '../components/StorageDetailsPanel.vue'

const route    = useRoute()
const router   = useRouter()
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const { t } = useI18n()
const devicesPagination = usePersistedTablePagination('storages.devices', {
  rowsPerPage: 20,
})
const deviceSearch = usePersistedTableFilter('storages.devices')
const deviceQuickFilter = ref('all')
const loading = ref(false)
const error = ref(null)
const directorErrors = ref([])
const storageRows = ref([])
const selectedStorageKey = ref(null)

const reachableDirectors = computed(() => [...new Set([
  ...director.availableDirectors,
  auth.user?.director,
  settings.directorName,
].filter(Boolean))])

const {
  directorOptions,
  activeDirectors,
  isCommonScope: isCommonStorages,
  syncSelectedDirectors,
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

const storages = computed(() => storageRows.value.filter(deviceMatchesQuickFilter))

const deviceStats = computed(() => {
  const all = storageRows.value
  return {
    total: all.length,
    enabled: all.filter(s => s.enabled).length,
    disabled: all.filter(s => !s.enabled).length,
    autochanger: all.filter(s => s.autochanger).length,
  }
})

function deviceMatchesQuickFilter(storage) {
  if (deviceQuickFilter.value === 'enabled') return storage.enabled
  if (deviceQuickFilter.value === 'disabled') return !storage.enabled
  if (deviceQuickFilter.value === 'autochanger') return !!storage.autochanger
  return true
}

async function refresh() {
  loading.value = true
  error.value = null
  directorErrors.value = []
  try {
    if (storagesPageDirectors.value.length === 0) {
      storageRows.value = []
      return
    }

    if (storagesPageDirectors.value.length > 1 || (storagesListScopeDirector.value && isCommonStorages.value)) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedStorages(credentials, storagesPageDirectors.value)
      storageRows.value = result.storages
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = storagesPageDirectors.value[0]
    await ensureSingleScopeDirector()
    storageRows.value = await fetchDirectorStorages(
      command => director.call(command),
      currentDirector
    )
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  } finally {
    loading.value = false
  }
}

const storageCols = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',        label: t('Name'),        field: 'name',        align: 'left',  sortable: true },
  { name: 'address',     label: t('Address'),     field: 'address',     align: 'left',  sortable: true },
  { name: 'mediatype',   label: t('Media Type'),  field: 'mediatype',   align: 'left',  sortable: true },
  { name: 'autochanger', label: t('Autochanger'), field: 'autochanger', align: 'center', sortable: true },
  { name: 'enabled',     label: t('Status'),      field: 'enabled',     align: 'center', sortable: true },
])

const {
  visibleColumns: visibleStorageCols,
  toggleableColumns: toggleableStorageCols,
  toggleColumn: toggleStorageCol,
} = usePersistedTableColumns('storages.devices', storageCols, {
  essential: ['name', 'director'],
  defaultHidden: ['autochanger'],
})

// Master-detail selection

const selectedStorage = computed(() => (
  storageRows.value.find(storage => storage.scopeKey === selectedStorageKey.value) ?? null
))

function storageRowClass(row) {
  return row.scopeKey === selectedStorageKey.value ? 'storages-row-selected' : ''
}

function selectStorage(storage) {
  selectedStorageKey.value = storage?.scopeKey ?? null
  syncRouteToSelection()
}

function toggleSelectedStorage(row) {
  selectStorage(row?.scopeKey === selectedStorageKey.value ? null : row)
}

function syncRouteToSelection() {
  const nextQuery = withStorageSelectionQuery(route.query, selectedStorage.value)

  if (JSON.stringify(nextQuery) === JSON.stringify(route.query)) {
    return
  }

  router.replace({ path: '/storages', query: nextQuery }).catch(() => {})
}

function syncSelectionFromRoute() {
  const requested = resolveStorageSelectionQuery(route.query)

  if (!requested) {
    selectedStorageKey.value = null
    return
  }

  const match = resolveAutochangerSelection(storageRows.value, {
    storageName: requested.name,
    directorName: requested.director,
    scopeDirector: storagesListScopeDirector.value,
    activeDirectors: storagesPageDirectors.value,
  })

  selectedStorageKey.value = match?.scopeKey ?? null
}

// Drop the selection when the selected storage is no longer listed, for
// instance after a director scope change or a refresh.
watch(storageRows, () => {
  syncSelectionFromRoute()

  if (!selectedStorage.value && resolveStorageSelectionQuery(route.query)) {
    syncRouteToSelection()
  }
})

watch(() => route.query[STORAGE_SELECTION_QUERY_KEY], () => {
  syncSelectionFromRoute()
})

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

<style scoped>
.storages-list-stats {
  flex-wrap: wrap;
}

.storages-list-stats :deep(.q-chip) {
  font-weight: 600;
}

.storages-detail-hint {
  border-style: dashed;
}

:deep(.q-table tbody tr) {
  cursor: pointer;
}

:deep(.storages-row-selected) > td {
  background: rgba(var(--q-primary-rgb, 25, 118, 210), 0.12);
}
</style>
