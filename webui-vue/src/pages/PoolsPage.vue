<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />
    <div v-if="poolsListScopeDirector" class="q-mb-md">
      <DirectorBadge
        removable
        icon="dns"
        :director="poolsListScopeDirector"
        @remove="router.replace({ path: '/pools', query: withStoragesScopeDirectorQuery(route.query, '') })"
      >
        {{ t('Director') }}: {{ poolsListScopeDirector }}
      </DirectorBadge>
    </div>

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-route-tab name="pools"   :label="t('Pools')"   no-caps :to="{ path: '/pools', query: buildPoolsTabQuery(route.query, 'pools') }" data-testid="pools-tab-pools" />
      <q-route-tab name="volumes" :label="t('Volumes')" no-caps :to="{ path: '/pools', query: buildPoolsTabQuery(route.query, 'volumes') }" data-testid="pools-tab-volumes" />
    </q-tabs>

    <q-banner v-if="error" dense rounded class="bg-negative text-white q-mb-md">
      {{ error }}
    </q-banner>

    <q-tab-panels v-model="tab" animated :swipeable="$q.platform.has.touch">
      <!-- POOLS -->
      <q-tab-panel name="pools" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Pools') }}</span>
            <q-space />
            <q-input v-model="poolSearch" dense outlined :placeholder="t('Search…')" style="width:200px" clearable data-testid="storages-pool-search">
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <ColumnPickerMenu :columns="toggleablePoolCols" @toggle="togglePoolCol" />
          </q-card-section>
          <q-card-section class="q-py-sm pools-list-stats">
            <div class="row items-center q-gutter-sm">
              <q-chip
                dense square outline color="grey-8"
                icon="inventory_2"
                clickable
                :selected="poolQuickFilter === 'all'"
                @click="poolQuickFilter = 'all'"
              >
                {{ t('Total') }}: {{ poolStats.total }}
              </q-chip>
              <q-chip
                v-for="(count, type) in poolStats.byType"
                :key="type"
                dense square outline color="primary"
                clickable
                :selected="poolQuickFilter === type"
                @click="poolQuickFilter = type"
              >
                {{ type }}: {{ count }}
              </q-chip>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              v-if="!(loading && !pools.length)"
              :rows="pools"
              :columns="visiblePoolCols"
              row-key="scopeKey"
              dense
              flat
              :loading="loading"
              :filter="poolSearch"
              v-model:pagination="poolsPagination"
              v-model:expanded="expandedPools"
            >
              <template #body="props">
                <q-tr :props="props" :data-testid="`pool-row-${props.row.name}`">
                  <q-td v-for="col in props.cols" :key="col.name" :props="props" :class="poolCellClass(col.name)">
                    <template v-if="col.name === 'expand'">
                      <q-btn
                        flat round dense size="sm"
                        :icon="props.expand ? 'expand_less' : 'expand_more'"
                        :title="props.expand ? t('Hide volumes') : t('Show volumes')"
                        :aria-label="props.expand ? t('Hide volumes') : t('Show volumes')"
                        :data-testid="`pool-expand-${props.row.name}`"
                        @click="props.expand = !props.expand"
                      />
                    </template>
                    <template v-else-if="col.name === 'name'">
                      <a href="#" class="text-primary" @click.prevent="openPoolDetails(props.row)">
                        {{ col.value }}
                      </a>
                    </template>
                    <template v-else-if="col.name === 'director'">
                      <DirectorLabel :director="props.row.director || col.value || ''" />
                    </template>
                    <template v-else-if="col.name === 'pooltype'">
                      <div class="row items-center no-wrap q-gutter-xs">
                        <PoolTypeBadge :type="col.value" />
                        <span>{{ col.value }}</span>
                      </div>
                    </template>
                    <template v-else-if="col.name === 'numvols'">
                      <div>{{ col.value }}</div>
                      <q-linear-progress
                        :value="poolGauge(col.value, maxNumVols)"
                        color="primary" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'maxvols'">
                      <div>{{ Number(col.value) > 0 ? col.value : '∞' }}</div>
                      <q-linear-progress v-if="Number(col.value) > 0"
                        :value="poolGauge(col.value, maxMaxVols)"
                        color="grey-6" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'volretention'">
                      <div>{{ formatDuration(col.value) }}</div>
                      <q-linear-progress
                        :value="poolGauge(Number(col.value), maxRetentionSecs)"
                        color="orange" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'maxvoljobs'">
                      <div>{{ Number(col.value) > 0 ? col.value : '∞' }}</div>
                      <q-linear-progress v-if="Number(col.value) > 0"
                        :value="poolGauge(col.value, maxMaxVolJobs)"
                        color="deep-purple" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'maxvolbytes'">
                      <div>{{ Number(col.value) > 0 ? formatBytes(col.value) : '∞' }}</div>
                      <q-linear-progress v-if="Number(col.value) > 0"
                        :value="poolGauge(col.value, maxMaxVolBytes)"
                        color="teal" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'totalbytes'">
                      <div>{{ formatBytes(col.value) }}</div>
                      <q-linear-progress
                        :value="poolBytesGauge(col.value)"
                        color="cyan-7" track-color="grey-3"
                        size="4px" class="q-mt-xs" rounded
                      />
                    </template>
                    <template v-else-if="col.name === 'prunablebytes'">
                      <div>{{ Number(col.value) > 0 ? formatBytes(col.value) : '—' }}</div>
                      <div v-if="props.row.prunablejobs > 0" class="text-caption text-grey-6">
                        {{ props.row.prunablejobs }} {{ t('jobs') }} / {{ props.row.prunablevolumes }} {{ t('volumes') }}
                      </div>
                    </template>
                    <template v-else>{{ col.value }}</template>
                  </q-td>
                </q-tr>
                <q-tr v-if="props.expand" :props="props" class="pool-volumes-row">
                  <q-td colspan="100%" class="q-pa-sm">
                    <div class="row items-center q-mb-xs">
                      <span class="text-weight-medium">
                        {{ t('Volumes') }} ({{ volumesOfPool(props.row).length }})
                      </span>
                      <q-space />
                      <q-btn
                        flat dense no-caps size="sm" color="primary" icon="filter_list"
                        :label="t('Show in volume list')"
                        :data-testid="`pool-show-volumes-${props.row.name}`"
                        @click="showPoolInVolumeList(props.row)"
                      />
                    </div>
                    <div v-if="!volumesOfPool(props.row).length" class="text-grey-6">
                      {{ t('No volumes') }}
                    </div>
                    <q-markup-table v-else dense flat bordered separator="horizontal" class="pool-volumes-table">
                      <thead>
                        <tr>
                          <th class="text-left">{{ t('Volume Name') }}</th>
                          <th class="text-center">{{ t('Status') }}</th>
                          <th class="text-left">{{ t('Media Type') }}</th>
                          <th class="text-left">{{ t('Last Written') }}</th>
                          <th class="text-right">{{ t('Used Bytes') }}</th>
                          <th class="text-left">{{ t('Comment') }}</th>
                        </tr>
                      </thead>
                      <tbody>
                        <tr v-for="volume in volumesOfPool(props.row)" :key="volume.scopeKey">
                          <td class="text-left">
                            <a href="#" class="text-primary" @click.prevent="openVolumeDetails(volume, 'pools')">
                              {{ volume.volumename }}
                            </a>
                          </td>
                          <td class="text-center">
                            <q-badge :color="statusColor(volume.volstatus)" :label="volume.volstatus" />
                          </td>
                          <td class="text-left">{{ volume.mediatype }}</td>
                          <td class="text-left">
                            <span :title="catalogTime(volume.lastwritten).title">{{ catalogTime(volume.lastwritten).text }}</span>
                          </td>
                          <td class="text-right">{{ formatBytes(volume.volbytes) }}</td>
                          <td class="text-left volume-comment-cell">{{ volume.comment }}</td>
                        </tr>
                      </tbody>
                    </q-markup-table>
                  </q-td>
                </q-tr>
              </template>
            </q-table>
            <TableSkeleton v-else :columns="visiblePoolCols.length" :rows="6" />
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- VOLUMES -->
      <q-tab-panel name="volumes" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header volumes-list-header row items-center">
            <span class="volumes-list-header__title">{{ t('Volumes') }}</span>
            <q-select
              :model-value="volumeFilters.pools"
              dense outlined clearable emit-value map-options multiple use-chips
              :options="poolFilterOptions"
              :label="t('Pool')"
              class="volumes-list-header__filter"
              data-testid="volumes-filter-pool"
              @update:model-value="value => setVolumeFilter('pools', value)"
            />
            <q-select
              :model-value="volumeFilters.statuses"
              dense outlined clearable emit-value map-options multiple use-chips
              :options="statusFilterOptions"
              :label="t('Status')"
              class="volumes-list-header__filter"
              data-testid="volumes-filter-status"
              @update:model-value="value => setVolumeFilter('statuses', value)"
            />
            <q-select
              :model-value="volumeFilters.mediatypes"
              dense outlined clearable emit-value map-options multiple use-chips
              :options="mediaTypeFilterOptions"
              :label="t('Media Type')"
              class="volumes-list-header__filter"
              data-testid="volumes-filter-mediatype"
              @update:model-value="value => setVolumeFilter('mediatypes', value)"
            />
            <q-input
              v-model="volSearch"
              dense outlined clearable
              :label="t('Search in results')"
              class="volumes-list-header__search"
              data-testid="storages-volume-search"
            >
              <template #prepend><q-icon name="search" /></template>
              <template #append>
                <q-icon name="info" size="16px" class="cursor-help">
                  <q-tooltip>{{ t('Searches only within the volumes already filtered here.') }}</q-tooltip>
                </q-icon>
              </template>
            </q-input>
            <ColumnPickerMenu :columns="toggleableVolumeCols" @toggle="toggleVolumeCol" />
          </q-card-section>

          <q-card-section class="q-py-sm pools-list-stats">
            <div class="row items-center q-gutter-sm">
              <q-chip dense square outline color="grey-8" icon="album">
                {{ t('Total') }}: {{ filteredVolumes.length }} / {{ volumeRows.length }}
                ({{ formatBytes(filteredVolumesBytes) }})
              </q-chip>
              <q-chip
                v-for="pool in volumeFilters.pools"
                :key="`pool:${pool}`"
                removable dense color="deep-purple" text-color="white" icon="inventory_2"
                @remove="removeVolumeFilterValue('pools', pool)"
              >
                {{ t('Pool') }}: {{ pool }}
              </q-chip>
              <q-chip
                v-for="status in volumeFilters.statuses"
                :key="`status:${status}`"
                removable dense color="primary" text-color="white" icon="filter_list"
                @remove="removeVolumeFilterValue('statuses', status)"
              >
                {{ t('Status') }}: {{ status }}
              </q-chip>
              <q-chip
                v-for="mediatype in volumeFilters.mediatypes"
                :key="`mediatype:${mediatype}`"
                removable dense color="teal" text-color="white" icon="filter_list"
                @remove="removeVolumeFilterValue('mediatypes', mediatype)"
              >
                {{ t('Media Type') }}: {{ mediatype }}
              </q-chip>
              <q-chip
                v-if="volSearch"
                removable dense color="grey-8" text-color="white" icon="search"
                @remove="volSearch = ''"
              >
                {{ t('Search') }}: {{ volSearch }}
              </q-chip>
            </div>
          </q-card-section>

          <q-card-section class="q-py-sm row items-center q-gutter-sm volumes-selection-bar">
            <span class="text-body2" data-testid="volumes-selection-count">
              {{ t('{count} selected', { count: selectedVolumes.length }) }}
            </span>
            <q-btn
              flat dense no-caps size="sm" icon="select_all"
              :label="t('Select all filtered ({count})', { count: filteredVolumes.length })"
              :disable="!filteredVolumes.length"
              data-testid="volumes-select-all-filtered"
              @click="selectAllFilteredVolumes"
            />
            <q-btn
              flat dense no-caps size="sm" icon="deselect"
              :label="t('Clear selection')"
              :disable="!selectedVolumes.length"
              @click="selectedVolumes = []"
            />
            <q-space />
            <q-btn-dropdown
              color="primary" dense no-caps icon="playlist_play"
              :label="t('Bulk actions')"
              :disable="!selectedVolumes.length"
              data-testid="volumes-bulk-actions"
            >
              <q-list dense style="min-width:220px">
                <q-item
                  v-for="action in nonDestructiveBulkActions"
                  :key="action.id"
                  clickable v-close-popup
                  :data-testid="`volumes-bulk-${action.id}`"
                  @click="openBulkAction(action.id)"
                >
                  <q-item-section avatar><q-icon :name="action.icon" /></q-item-section>
                  <q-item-section>{{ t(action.label) }}</q-item-section>
                </q-item>
                <q-separator />
                <q-item-label header class="text-negative">
                  <q-icon name="warning" class="q-mr-xs" />{{ t('Destructive') }}
                </q-item-label>
                <q-item
                  v-for="action in destructiveBulkActions"
                  :key="action.id"
                  clickable v-close-popup
                  class="text-negative"
                  :data-testid="`volumes-bulk-${action.id}`"
                  @click="openBulkAction(action.id)"
                >
                  <q-item-section avatar><q-icon :name="action.icon" color="negative" /></q-item-section>
                  <q-item-section>{{ t(action.label) }}</q-item-section>
                </q-item>
              </q-list>
            </q-btn-dropdown>
          </q-card-section>

          <q-card-section class="q-pa-none">
            <q-table
              v-if="!(loading && !volumeRows.length)"
              :rows="filteredVolumes"
              :columns="visibleVolumeCols"
              row-key="scopeKey"
              dense
              flat
              selection="multiple"
              v-model:selected="selectedVolumes"
              :loading="loading"
              v-model:pagination="volumesPagination"
              :rows-per-page-options="volumesRowsPerPageOptions"
            >
              <template #body-cell-volumename="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <a href="#" class="text-primary" @click.prevent="openVolumeDetails(props.row, 'volumes')">
                      {{ props.value }}
                    </a>
                    <q-icon
                      v-if="volumeHasEncryptionKey(props.row)"
                      name="vpn_key"
                      size="xs"
                      color="amber-8"
                      role="img"
                      :aria-label="t('Encryption key stored in catalog')"
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
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <EnabledBadge :enabled="props.value" />
                </q-td>
              </template>
              <template #body-cell-pool="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <a href="#" class="text-primary" @click.prevent="openPoolDetails({ name: props.row.pool, director: props.row.director })">
                      {{ props.value }}
                    </a>
                    <q-btn
                      v-if="props.value"
                      flat round dense size="xs" icon="filter_alt"
                      :title="`${t('Filter by pool')}: ${props.value}`"
                      :aria-label="`${t('Filter by pool')}: ${props.value}`"
                      @click="addVolumeFilterValue('pools', props.value)"
                    />
                  </div>
                </q-td>
              </template>
              <template #body-cell-mediatype="props">
                <q-td :props="props">
                  <a
                    v-if="props.value"
                    href="#"
                    class="inline-volume-filter"
                    :title="`${t('Filter by media type')}: ${props.value}`"
                    @click.prevent="addVolumeFilterValue('mediatypes', props.value)"
                  >{{ props.value }}</a>
                </q-td>
              </template>
              <template #body-cell-volstatus="props">
                <q-td :props="props">
                  <q-badge
                    :color="statusColor(props.value)"
                    :label="props.value"
                    class="cursor-pointer"
                    :title="`${t('Filter by status')}: ${props.value}`"
                    @click="addVolumeFilterValue('statuses', props.value)"
                  />
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
              <template #body-cell-lastwritten="props">
                <q-td :props="props">
                  <span :title="catalogTime(props.value).title">
                    {{ catalogTime(props.value).text }}
                  </span>
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
              <template #body-cell-comment="props">
                <q-td :props="props" class="volume-comment-cell">
                  <span :title="props.value">{{ props.value }}</span>
                </q-td>
              </template>
            </q-table>
            <TableSkeleton v-else :columns="visibleVolumeCols.length" :rows="8" />
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>

    <VolumeBulkDialog
      v-model="bulkDialog.open"
      :action-id="bulkDialog.action"
      :volumes="bulkDialog.volumes"
      :pool-options="bulkPoolOptions"
      :run-command="runVolumeCommand"
      @done="onBulkDone"
    />
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { usePersistedTablePagination, UNBOUNDED_TABLE_ROWS_PER_PAGE } from '../composables/usePersistedTablePagination.js'
import { usePersistedTableFilter } from '../composables/usePersistedTableFilter.js'
import { usePersistedTableColumns } from '../composables/usePersistedTableColumns.js'
import {
  fetchAggregatedPoolsState,
  fetchDirectorPoolsState,
} from '../composables/storagesAggregate.js'
import {
  buildPoolsTabQuery,
  normalisePoolsTab,
  resolveStoragesScopeDirector,
  withStoragesScopeDirectorQuery,
} from '../utils/storagesRoute.js'
import { buildPoolDetailsQuery } from '../utils/pools.js'
import { buildVolumeDetailsQuery, volumeHasEncryptionKey } from '../utils/volumes.js'
import {
  buildVolumeFiltersQuery,
  filterVolumes,
  resolveVolumeFilters,
  volumeFilterOptions,
  volumeFiltersEqual,
} from '../utils/volumeFilters.js'
import { VOLUME_BULK_ACTIONS } from '../utils/volumeBulk.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import { formatCatalogTimestamp } from '../utils/locales.js'
import BoolIcon from '../components/BoolIcon.vue'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import EnabledBadge from '../components/EnabledBadge.vue'
import PoolTypeBadge from '../components/PoolTypeBadge.vue'
import ColumnPickerMenu from '../components/ColumnPickerMenu.vue'
import TableSkeleton from '../components/TableSkeleton.vue'
import VolumeBulkDialog from '../components/VolumeBulkDialog.vue'

const route    = useRoute()
const router   = useRouter()
const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const $q = useQuasar()
const { t } = useI18n()

function catalogTime(value) {
  return formatCatalogTimestamp(value, settings.relativeTime, settings.locale)
}

const poolsPagination = usePersistedTablePagination('storages.pools', {
  rowsPerPage: 20,
})
const volumesPagination = usePersistedTablePagination('storages.volumes', {
  rowsPerPage: 20,
}, { allowedRowsPerPage: UNBOUNDED_TABLE_ROWS_PER_PAGE })
const volumesRowsPerPageOptions = UNBOUNDED_TABLE_ROWS_PER_PAGE
const tab = ref(normalisePoolsTab(route.query.tab))
const poolSearch = usePersistedTableFilter('storages.pools')
const volSearch = usePersistedTableFilter('storages.volumes')
const poolQuickFilter = ref('all')
const expandedPools = ref([])
const volumeFilters = ref(resolveVolumeFilters(route.query))
const selectedVolumes = ref([])
const loading = ref(false)
const error = ref(null)
const directorErrors = ref([])
const poolRows = ref([])
const volumeRows = ref([])

const reachableDirectors = computed(() => [...new Set([
  ...director.availableDirectors,
  auth.user?.director,
  settings.directorName,
].filter(Boolean))])

const {
  directorOptions,
  activeDirectors,
  isCommonScope: isCommonPools,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({
  t,
  buildOptions: () => reachableDirectors.value.map(value => ({ label: value, value })),
})

const poolsListScopeDirector = computed(() => {
  const requestedDirector = resolveStoragesScopeDirector(route.query)

  if (requestedDirector && activeDirectors.value.includes(requestedDirector)) {
    return requestedDirector
  }

  return ''
})
const poolsPageDirectors = computed(() => (
  poolsListScopeDirector.value ? [poolsListScopeDirector.value] : activeDirectors.value
))
const showDirectorColumn = computed(() => poolsPageDirectors.value.length > 1)

watch(() => route.query.tab, (value) => {
  const next = normalisePoolsTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(tab, (next) => {
  if (normalisePoolsTab(route.query.tab) === next) {
    return
  }

  router.replace({ path: '/pools', query: buildPoolsTabQuery(route.query, next) })
})

watch(() => route.query.scopeDirector, (value) => {
  if (typeof value === 'string' && value && !activeDirectors.value.includes(value)) {
    router.replace({
      path: '/pools',
      query: withStoragesScopeDirectorQuery(route.query, ''),
    })
    return
  }

  refresh()
})

watch(() => [route.query.volPool, route.query.volStatus, route.query.volMediaType], () => {
  const next = resolveVolumeFilters(route.query)
  if (!volumeFiltersEqual(volumeFilters.value, next)) {
    volumeFilters.value = next
  }
})

watch(volumeFilters, (next) => {
  const query = buildVolumeFiltersQuery(route.query, next)
  if (!volumeFiltersEqual(resolveVolumeFilters(route.query), next)) {
    router.replace({ path: '/pools', query })
  }
}, { deep: true })

function setVolumeFilter(name, values) {
  volumeFilters.value = { ...volumeFilters.value, [name]: [...(values ?? [])] }
}

function addVolumeFilterValue(name, value) {
  const current = volumeFilters.value[name] ?? []
  if (!value || current.includes(value)) {
    return
  }
  setVolumeFilter(name, [...current, value])
}

function removeVolumeFilterValue(name, value) {
  setVolumeFilter(name, (volumeFilters.value[name] ?? []).filter(item => item !== value))
}

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

// ── Pools ─────────────────────────────────────────────────────────────────────
const pools = computed(() => poolRows.value.filter(poolMatchesQuickFilter))

const poolStats = computed(() => {
  const all = poolRows.value
  const byType = {}
  for (const pool of all) {
    const type = pool.pooltype || t('Unknown')
    byType[type] = (byType[type] ?? 0) + 1
  }
  return { total: all.length, byType }
})

function poolMatchesQuickFilter(pool) {
  if (poolQuickFilter.value === 'all') return true
  return (pool.pooltype || t('Unknown')) === poolQuickFilter.value
}

const volumesByPool = computed(() => {
  const map = new Map()
  for (const volume of volumeRows.value) {
    const key = `${volume.director}:${volume.pool}`
    if (!map.has(key)) {
      map.set(key, [])
    }
    map.get(key).push(volume)
  }
  return map
})

function volumesOfPool(pool) {
  return volumesByPool.value.get(`${pool.director}:${pool.name}`) ?? []
}

function showPoolInVolumeList(pool) {
  const filters = { ...volumeFilters.value, pools: [pool.name], statuses: [], mediatypes: [] }
  volumeFilters.value = filters
  router.replace({
    path: '/pools',
    query: buildVolumeFiltersQuery(buildPoolsTabQuery(route.query, 'volumes'), filters),
  })
}

const maxPoolBytes = computed(() =>
  Math.max(1, ...pools.value.map(p => p.totalbytes))
)
function poolBytesGauge(val) { return (Number(val) || 0) / maxPoolBytes.value }

function maxOf(arr, field) {
  return Math.max(1, ...arr.map(p => Number(p[field]) || 0))
}
const maxNumVols      = computed(() => maxOf(pools.value, 'numvols'))
const maxMaxVols      = computed(() => maxOf(pools.value.filter(p => Number(p.maxvols) > 0), 'maxvols'))
const maxRetentionSecs= computed(() => maxOf(pools.value, 'volretention'))
const maxMaxVolJobs   = computed(() => maxOf(pools.value.filter(p => Number(p.maxvoljobs) > 0), 'maxvoljobs'))
const maxMaxVolBytes  = computed(() => maxOf(pools.value.filter(p => Number(p.maxvolbytes) > 0), 'maxvolbytes'))
function poolGauge(val, max) { return (Number(val) || 0) / (max || 1) }

const RIGHT_ALIGNED_POOL_COLS = new Set(['numvols', 'maxvols', 'maxvoljobs', 'maxvolbytes', 'totalbytes', 'prunablebytes'])
function poolCellClass(name) {
  return RIGHT_ALIGNED_POOL_COLS.has(name) ? 'text-right' : ''
}

// ── Volumes ───────────────────────────────────────────────────────────────────
const SEARCH_FIELDS = ['volumename', 'pool', 'storage', 'mediatype', 'volstatus', 'comment', 'director']

const filteredVolumes = computed(() => {
  const byFilters = filterVolumes(volumeRows.value, volumeFilters.value)
  const needle = String(volSearch.value ?? '').trim().toLowerCase()
  if (!needle) {
    return byFilters
  }
  return byFilters.filter(volume => SEARCH_FIELDS.some(field => (
    String(volume[field] ?? '').toLowerCase().includes(needle)
  )))
})

const filteredVolumesBytes = computed(() => (
  filteredVolumes.value.reduce((sum, vol) => sum + (Number(vol.volbytes) || 0), 0)
))

const poolFilterOptions = computed(() => volumeFilterOptions(volumeRows.value, 'pools'))
const statusFilterOptions = computed(() => volumeFilterOptions(volumeRows.value, 'statuses'))
const mediaTypeFilterOptions = computed(() => volumeFilterOptions(volumeRows.value, 'mediatypes'))

function selectAllFilteredVolumes() {
  selectedVolumes.value = [...filteredVolumes.value]
}

// Drop selected rows that disappeared (refresh, deletion, filter change).
watch(filteredVolumes, (rows) => {
  const visible = new Map(rows.map(row => [row.scopeKey, row]))
  const next = selectedVolumes.value
    .map(row => visible.get(row.scopeKey))
    .filter(Boolean)
  if (next.length !== selectedVolumes.value.length
    || next.some((row, index) => row !== selectedVolumes.value[index])) {
    selectedVolumes.value = next
  }
})

const maxVolBytes     = computed(() => maxOf(filteredVolumes.value, 'volbytes'))
const maxVolMaxBytes  = computed(() => maxOf(filteredVolumes.value.filter(v => Number(v.maxvolbytes) > 0), 'maxvolbytes'))
const maxVolRetention = computed(() => maxOf(filteredVolumes.value, 'retention'))
function volBytesGauge(val) { return (Number(val) || 0) / maxVolBytes.value }
function volGauge(val, max) { return (Number(val) || 0) / (max || 1) }

// ── Bulk operations ───────────────────────────────────────────────────────────
const nonDestructiveBulkActions = VOLUME_BULK_ACTIONS.filter(action => !action.destructive)
const destructiveBulkActions = VOLUME_BULK_ACTIONS.filter(action => action.destructive)

const bulkDialog = ref({ open: false, action: '', volumes: [] })

// Pools that exist on every director involved in the selection.
const bulkPoolOptions = computed(() => {
  const directors = [...new Set(bulkDialog.value.volumes.map(volume => volume.director))]
  const names = directors.map(dir => new Set(
    poolRows.value.filter(pool => pool.director === dir).map(pool => pool.name)
  ))
  if (!names.length) {
    return []
  }
  return [...names[0]].filter(name => names.every(set => set.has(name))).sort()
})

function openBulkAction(actionId) {
  bulkDialog.value = {
    open: true,
    action: actionId,
    volumes: [...selectedVolumes.value],
  }
}

async function runVolumeCommand(volume, command) {
  if (showDirectorColumn.value || isCommonPools.value) {
    await switchToRowDirector(volume)
  }
  return director.call(command)
}

async function onBulkDone() {
  selectedVolumes.value = []
  await refresh()
}

// ── Columns ───────────────────────────────────────────────────────────────────
const poolCols = computed(() => [
  { name: 'expand', label: '', field: 'expand', align: 'left', style: 'width:32px' },
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
  { name: 'enabled',     label: t('Enabled'),      field: 'enabled',     align: 'center', sortable: true },
  { name: 'inchanger',   label: t('In Changer'),   field: 'inchanger',   align: 'center', sortable: true },
  { name: 'retention',   label: t('Retention'),    field: 'retention',   align: 'left',  sortable: true },
  { name: 'maxvolbytes', label: t('Max Bytes'),    field: 'maxvolbytes', align: 'right', sortable: true },
  { name: 'volbytes',    label: t('Used Bytes'),   field: 'volbytes',    align: 'right', sortable: true },
  { name: 'comment',     label: t('Comment'),      field: 'comment',     align: 'left',  sortable: true },
])

const {
  visibleColumns: visiblePoolCols,
  toggleableColumns: toggleablePoolCols,
  toggleColumn: togglePoolCol,
} = usePersistedTableColumns('storages.pools', poolCols, { essential: ['expand', 'name', 'director'] })

const {
  visibleColumns: visibleVolumeCols,
  toggleableColumns: toggleableVolumeCols,
  toggleColumn: toggleVolumeCol,
} = usePersistedTableColumns('storages.volumes', volumeCols, {
  essential: ['volumename', 'director'],
  defaultHidden: ['enabled'],
})

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative',
           Purged: 'grey', Used: 'orange', 'Read-Only': 'blue-grey', Cleaning: 'teal' }[s] || 'info'
}

// ── Loading ───────────────────────────────────────────────────────────────────
async function refresh() {
  loading.value = true
  error.value = null
  directorErrors.value = []
  try {
    if (poolsPageDirectors.value.length === 0) {
      poolRows.value = []
      volumeRows.value = []
      return
    }

    if (poolsPageDirectors.value.length > 1 || (poolsListScopeDirector.value && isCommonPools.value)) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedPoolsState(credentials, poolsPageDirectors.value)
      poolRows.value = result.pools
      volumeRows.value = result.volumes
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = poolsPageDirectors.value[0]
    await ensureSingleScopeDirector()
    const result = await fetchDirectorPoolsState(
      command => director.call(command),
      currentDirector
    )
    poolRows.value = result.pools
    volumeRows.value = result.volumes
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  } finally {
    loading.value = false
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
        poolsTab: tab.value,
        poolsScopeDirector: pool.director,
      }),
    })
  } catch (reason) {
    reportRowError(pool, reason)
  }
}

async function openVolumeDetails(volume, originTab) {
  try {
    await switchToRowDirector(volume)
    await router.push({
      name: 'volume-details',
      params: { name: volume.volumename },
      query: buildVolumeDetailsQuery({
        director: volume.director,
        poolsTab: originTab,
        poolsScopeDirector: volume.director,
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
      path: '/pools',
      query: withStoragesScopeDirectorQuery(route.query, ''),
    })
  }
  refresh()
})
</script>

<style scoped>
.pools-list-stats {
  flex-wrap: wrap;
}

.pools-list-stats :deep(.q-chip) {
  font-weight: 600;
}

.volumes-list-header {
  flex-wrap: wrap;
  gap: 8px;
}

.volumes-list-header__title {
  flex: 0 0 auto;
  margin-right: auto;
}

.volumes-list-header__filter {
  flex: 1 1 200px;
  min-width: 170px;
  max-width: 260px;
}

.volumes-list-header__search {
  flex: 1 1 200px;
  min-width: 170px;
  max-width: 220px;
}

.volumes-selection-bar {
  flex-wrap: wrap;
}

.inline-volume-filter {
  color: inherit;
  text-decoration: underline dotted;
}

.volume-comment-cell {
  max-width: 280px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pool-volumes-table {
  max-height: 320px;
  overflow: auto;
}
</style>
