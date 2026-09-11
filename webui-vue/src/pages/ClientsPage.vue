<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />
    <div v-if="clientsListScopeDirector" class="q-mb-md">
      <DirectorBadge
        removable
        icon="dns"
        :director="clientsListScopeDirector"
        @remove="router.replace({ path: '/clients', query: withClientsScopeDirectorQuery(route.query, '') })"
      >
        {{ t('Director') }}: {{ clientsListScopeDirector }}
      </DirectorBadge>
    </div>

    <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header clients-list-header row items-center">
            <span class="clients-list-header__title">{{ t('Client List') }}</span>
            <q-input
              v-model="clientsSearch"
              dense
              outlined
              clearable
              :label="t('Search clients')"
              class="clients-list-header__search"
              data-testid="clients-search"
            >
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn-toggle
              v-model="clientsQuickFilter"
              dense
              no-caps
              unelevated
              :options="clientsQuickFilterOptions"
              color="grey-3"
              text-color="grey-9"
              toggle-color="primary"
              toggle-text-color="white"
              class="clients-list-header__filters"
              data-testid="clients-quick-filter"
            />
            <q-input
              v-model.number="settings.clientBackupWarningDays"
              dense
              outlined
              type="number"
              min="1"
              max="365"
              :label="t('Backup warning after days')"
              class="clients-list-header__threshold"
              data-testid="clients-backup-warning-days"
            >
              <template #prepend><q-icon name="schedule" /></template>
              <q-tooltip>
                {{ t('Backups older than this threshold are shown as stale.') }}
              </q-tooltip>
            </q-input>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refresh(true)" />
          </q-card-section>
          <q-card-section class="q-py-sm clients-list-stats">
            <div class="row items-center q-gutter-sm">
              <q-chip
                dense square outline color="grey-8"
                icon="groups"
                clickable
                :selected="clientsQuickFilter === 'all'"
                @click="clientsQuickFilter = 'all'"
              >
                {{ t('Total') }}: {{ clientStats.total }}
              </q-chip>
              <q-chip
                dense square outline color="positive"
                icon="check_circle"
                clickable
                :selected="clientsQuickFilter === 'enabled'"
                @click="clientsQuickFilter = 'enabled'"
              >
                {{ t('Enabled') }}: {{ clientStats.enabled }}
              </q-chip>
              <q-chip
                dense square outline color="negative"
                icon="pause_circle"
                clickable
                :selected="clientsQuickFilter === 'disabled'"
                @click="clientsQuickFilter = 'disabled'"
              >
                {{ t('Disabled') }}: {{ clientStats.disabled }}
              </q-chip>
              <q-chip
                v-if="clientStats.staleBackup"
                dense square outline color="warning" text-color="black"
                icon="schedule"
                clickable
                :selected="clientsQuickFilter === 'stale_backup'"
                @click="clientsQuickFilter = 'stale_backup'"
              >
                {{ t('Stale backup') }}: {{ clientStats.staleBackup }}
              </q-chip>
              <q-chip
                v-if="clientStats.backupErrors"
                dense square outline color="negative"
                icon="error"
                clickable
                :selected="clientsQuickFilter === 'backup_errors'"
                @click="clientsQuickFilter = 'backup_errors'"
              >
                {{ t('Backup errors') }}: {{ clientStats.backupErrors }}
              </q-chip>
              <q-chip
                v-if="clientStats.outdated"
                dense square outline color="warning" text-color="black"
                icon="system_update_alt"
                clickable
                :selected="clientsQuickFilter === 'outdated'"
                @click="clientsQuickFilter = 'outdated'"
              >
                {{ t('Outdated') }}: {{ clientStats.outdated }}
              </q-chip>
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
            <div v-if="clientsListScopeDirector" class="q-px-md q-pt-sm">
              <q-chip
                removable
                color="blue-7"
                text-color="white"
                icon="dns"
                class="q-mb-xs"
                @remove="router.replace({ path: '/clients', query: withClientsScopeDirectorQuery(route.query, '') })"
              >
                {{ t('Director') }}: {{ clientsListScopeDirector }}
              </q-chip>
            </div>
            <q-table
              :rows="clients"
              :columns="columns"
              row-key="scopeKey"
              binary-state-sort
              dense
              flat
              :loading="loading"
              v-model:pagination="clientsPagination"
              @row-click="openClientDetailsFromRow"
            >
              <template #body-cell-name="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent.stop="openClientDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                    <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-os="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <q-icon :name="osIcon(props.row)" :color="osColor(props.row)" size="18px" />
                    <div>
                      <div>{{ osLabel(props.row.os) }}</div>
                      <div v-if="props.row.osInfo" class="text-caption text-grey-6" style="line-height:1.2">
                        {{ props.row.osInfo }}
                      </div>
                    </div>
                  </div>
                </q-td>
              </template>
              <template #body-cell-version="props">
                <q-td :props="props">
                  <q-badge
                    v-if="props.value"
                    :color="clientVersionColor(props.row)"
                    class="text-mono"
                    :label="props.value"
                  />
                  <span v-else class="text-grey-5">—</span>
                  <div v-if="props.row.arch || props.row.buildDate" class="text-caption text-grey-6" style="line-height:1.2">
                    <span v-if="props.row.arch">{{ props.row.arch }}</span>
                    <span v-if="props.row.arch && props.row.buildDate"> · </span>
                    <span v-if="props.row.buildDate">{{ props.row.buildDate }}</span>
                  </div>
                  <q-tooltip v-if="clientVersionTooltip(props.row)">{{ clientVersionTooltip(props.row) }}</q-tooltip>
                </q-td>
              </template>
              <template #body-cell-lastBackup="props">
                <q-td :props="props">
                  <template v-if="props.row.lastBackup">
                    <div class="row items-center no-wrap q-gutter-xs">
                      <router-link
                        :to="buildClientJobLocation(props.row.lastBackup)"
                        class="text-primary"
                        @click.stop
                      >
                        #{{ props.row.lastBackup.id }}
                      </router-link>
                      <JobStatusBadge :status="props.row.lastBackup.status" />
                    </div>
                    <div class="text-caption text-grey-6">
                      {{ formatClientBackupTime(props.row.lastBackup.starttime) }}
                    </div>
                    <div
                      class="client-backup-age-bar q-mt-xs"
                      :style="clientBackupAgeBarStyle(props.row)"
                    />
                  </template>
                  <span v-else class="text-grey-5">—</span>
                </q-td>
              </template>
              <template #body-cell-backupStatus="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <q-badge
                      v-for="item in clientBackupStatusItems(props.row)"
                      :key="item.label"
                      :color="item.color"
                      :label="item.label"
                    />
                  </div>
                </q-td>
              </template>
              <template #body-cell-versionStatus="props">
                <q-td :props="props">
                  <div class="row items-center no-wrap q-gutter-xs">
                    <q-badge
                      v-for="item in clientVersionStatusItems(props.row)"
                      :key="item.label"
                      :color="item.color"
                      :label="item.label"
                    />
                  </div>
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-toggle
                    :model-value="props.row.enabled"
                    :color="props.row.enabled ? 'positive' : 'negative'"
                    dense
                    :label="props.row.enabled ? t('Enabled') : t('Disabled')"
                    :loading="toggling === props.row.scopeKey"
                    @click.stop
                    @update:model-value="toggleEnabled(props.row)"
                  />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn-dropdown
                    flat
                    round
                    dense
                    size="sm"
                    dropdown-icon="more_vert"
                    :title="t('Actions')"
                    @click.stop
                  >
                    <q-list dense style="min-width: 190px">
                      <q-item clickable v-close-popup @click="openClientDetails(props.row)">
                        <q-item-section avatar><q-icon name="open_in_new" /></q-item-section>
                        <q-item-section>{{ t('Details') }}</q-item-section>
                      </q-item>
                      <q-item clickable v-close-popup @click="showStatus(props.row)">
                        <q-item-section avatar><q-icon name="info" /></q-item-section>
                        <q-item-section>{{ t('Status') }}</q-item-section>
                      </q-item>
                      <q-item clickable v-close-popup @click="openClientJobs(props.row)">
                        <q-item-section avatar><q-icon name="work_history" /></q-item-section>
                        <q-item-section>{{ t('Jobs for this client') }}</q-item-section>
                      </q-item>
                      <q-item clickable v-close-popup @click="openClientTimeline(props.row)">
                        <q-item-section avatar><q-icon name="timeline" /></q-item-section>
                        <q-item-section>{{ t('View Timeline') }}</q-item-section>
                      </q-item>
                      <q-item clickable v-close-popup @click="openClientRestore(props.row)">
                        <q-item-section avatar><q-icon name="restore" /></q-item-section>
                        <q-item-section>{{ t('Restore from client') }}</q-item-section>
                      </q-item>
                    </q-list>
                  </q-btn-dropdown>
                 </q-td>
               </template>
             </q-table>
          </q-card-section>
        </q-card>

    <!-- Client status dialog -->
    <q-dialog v-model="statusDialog.open">
      <q-card style="min-width:600px; max-width:90vw">
        <q-card-section class="panel-header row items-center q-py-sm">
          <span>{{ t('Status') }}: {{ statusDialog.client }}</span>
          <q-space />
          <q-btn
            flat
            round
            dense
            icon="close"
            color="white"
            :title="t('Close')"
            data-testid="client-status-close"
            v-close-popup
          />
        </q-card-section>
        <q-card-section class="q-pa-none">
          <q-inner-loading :showing="statusDialog.loading" />
          <template v-if="statusDialog.text">
            <div class="q-pa-md">
              <q-markup-table dense flat bordered>
                <tbody>
                  <tr v-for="row in clientStatusSummaryRows" :key="row.label">
                    <td class="text-weight-medium" style="width: 45%">{{ row.label }}</td>
                    <td>{{ row.value }}</td>
                  </tr>
                </tbody>
              </q-markup-table>
            </div>
            <q-expansion-item
              dense
              expand-separator
              icon="article"
              :label="t('Raw status output')"
              default-opened
            >
              <pre data-testid="client-status-output" class="client-status-output">{{ statusDialog.text }}</pre>
            </q-expansion-item>
          </template>
          <div v-else-if="!statusDialog.loading" class="text-grey q-pa-md text-center">{{ t('No output') }}</div>
        </q-card-section>
        <q-card-actions align="right">
          <q-btn flat no-caps :label="t('Close')" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import {
  directorCollection,
  normaliseClient,
  normaliseJob,
} from '../composables/useDirectorFetch.js'
import {
  decorateScheduledBackups,
  fetchAggregatedClients,
} from '../composables/clientsAggregate.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import {
  buildClientDetailsQuery,
  parseClientStatusSummary,
  resolveClientsScopeDirector,
  withClientsScopeDirectorQuery,
} from '../utils/clients.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { buildJobDetailsQuery } from '../utils/jobs.js'
import { formatSqlRelativeTime } from '../utils/locales.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useReleaseInfoStore } from '../stores/releaseInfo.js'
import { useSettingsStore } from '../stores/settings.js'
import { usePersistedTablePagination } from '../composables/usePersistedTablePagination.js'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const route = useRoute()
const auth = useAuthStore()
const director = useDirectorStore()
const releaseInfo = useReleaseInfoStore()
const settings = useSettingsStore()
const router = useRouter()
const $q = useQuasar()
const { t } = useI18n()
const clientsPagination = usePersistedTablePagination('clients.list', {
  rowsPerPage: 15,
  sortBy: 'name',
  descending: false,
}, {
  persistSort: true,
})

const rawClients = ref([])
const rawRecentBackups = ref([])
const rawScheduledBackups = ref([])
const clientsSearch = ref('')
const clientsQuickFilter = ref('all')
const loading    = ref(false)
const error      = ref(null)
const directorErrors = ref([])

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonClients,
  isSingleDirectorScope,
  scopeLabel: clientsScopeLabel,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

const clientsListScopeDirector = computed(() => {
  const requestedDirector = resolveClientsScopeDirector(route.query)

  if (requestedDirector && activeDirectors.value.includes(requestedDirector)) {
    return requestedDirector
  }

  return ''
})
const clientsPageDirectors = computed(() => (
  clientsListScopeDirector.value ? [clientsListScopeDirector.value] : activeDirectors.value
))
const showDirectorColumn = computed(() => clientsPageDirectors.value.length > 1)

async function refresh(forceRefresh = false) {
  loading.value = true
  error.value   = null
  directorErrors.value = []
  try {
    if (clientsPageDirectors.value.length === 0) {
      rawClients.value = []
      rawRecentBackups.value = []
      rawScheduledBackups.value = []
      return
    }

    if (clientsPageDirectors.value.length > 1 || (clientsListScopeDirector.value && isCommonClients.value)) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedClients(credentials, clientsPageDirectors.value, { forceRefresh })
      rawClients.value = result.clients
      rawRecentBackups.value = result.recentBackups
      rawScheduledBackups.value = result.scheduledBackups
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = clientsPageDirectors.value[0]
    await ensureSingleScopeDirector()
    const [listResult, dotResult, recentBackupsResult, schedulerResult] = await Promise.all([
      director.call('llist clients'),
      director.call('.clients'),
      director.call('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
      director.call('status scheduler days=-31,1'),
    ])
    const list = directorCollection(listResult?.clients)
    const dot = directorCollection(dotResult?.clients)
    const enabledMap = Object.fromEntries(dot.map(c => [c.name, c.enabled]))
    rawClients.value = list.map(c => ({
      ...c,
      enabled: enabledMap[c.name] ?? true,
      director: currentDirector,
      scopeKey: `${currentDirector}:${c.name}`,
    }))
    rawRecentBackups.value = directorCollection(recentBackupsResult?.jobs).map(job => ({
      ...normaliseJob(job),
      director: currentDirector,
    }))
    rawScheduledBackups.value = decorateScheduledBackups(schedulerResult, currentDirector)
  } catch (e) {
    error.value = e.message ?? String(e)
  } finally {
    loading.value = false
  }
}

onMounted(refresh)
onMounted(() => {
  releaseInfo.refresh().catch(() => {})
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
})

const allClientsData = computed(() => directorCollection(rawClients.value).map((entry) => {
  const client = normaliseClient(entry)
  const lastBackup = lastBackupByClient.value.get(`${entry.director ?? ''}:${client.name}`) ?? null
  return {
    ...client,
    director: entry.director,
    scopeKey: entry.scopeKey ?? `${entry.director ?? ''}:${client.name}`,
    lastBackup,
  }
}))

const clients = computed(() => allClientsData.value.filter(client => clientMatchesFilters(client)))

const clientStats = computed(() => {
  const all = allClientsData.value
  return {
    total: all.length,
    enabled: all.filter(client => client.enabled).length,
    disabled: all.filter(client => !client.enabled).length,
    outdated: all.filter(client => (
      ['update_required', 'upgrade_required'].includes(clientVersionInfo(client).status)
    )).length,
    staleBackup: all.filter(client => !isFreshBackup(client.lastBackup)).length,
    backupErrors: all.filter(client => clientHasLastBackupError(client)).length,
  }
})

const clientsQuickFilterOptions = computed(() => [
  { label: t('All'), value: 'all', icon: 'select_all' },
  { label: t('Enabled'), value: 'enabled', icon: 'check_circle', toggleColor: 'positive' },
  { label: t('Disabled'), value: 'disabled', icon: 'pause_circle', toggleColor: 'negative' },
  { label: t('Outdated'), value: 'outdated', icon: 'system_update_alt', toggleColor: 'warning', toggleTextColor: 'black' },
  { label: t('Stale backup'), value: 'stale_backup', icon: 'schedule', toggleColor: 'warning', toggleTextColor: 'black' },
  { label: t('Backup errors'), value: 'backup_errors', icon: 'error', toggleColor: 'negative' },
])

const lastBackupByClient = computed(() => {
  const backupsByClient = new Map()
  for (const backup of directorCollection(rawRecentBackups.value)) {
    const job = {
      ...normaliseJob(backup),
      director: backup.director,
    }
    if (!job.client) {
      continue
    }
    const key = `${job.director ?? ''}:${job.client}`
    const current = backupsByClient.get(key)
    if (!current || String(job.starttime ?? '') > String(current.starttime ?? '')) {
      backupsByClient.set(key, job)
    }
  }
  return backupsByClient
})

const expectedBackupByClient = computed(() => {
  const expectedByClient = new Map()
  for (const expected of directorCollection(rawScheduledBackups.value)) {
    if (!expected.client || !Number.isFinite(expected.runtime) || expected.runtime <= 0) {
      continue
    }
    const key = `${expected.director ?? ''}:${expected.client}`
    const current = expectedByClient.get(key)
    if (!current || expected.runtime > current.runtime) {
      expectedByClient.set(key, expected)
    }
  }
  return expectedByClient
})

function normalizedClientSearchText(client) {
  return [
    client.name,
    client.director,
    osLabel(client.os),
    client.osInfo,
    client.version,
    client.arch,
    client.buildDate,
    client.enabled ? t('Enabled') : t('Disabled'),
    ...clientBackupStatusItems(client).map(item => item.label),
    ...clientVersionStatusItems(client).map(item => item.label),
    client.lastBackup?.id ? `#${client.lastBackup.id}` : '',
    client.lastBackup?.name,
    client.lastBackup?.status,
    client.lastBackup?.starttime,
  ].filter(Boolean).join(' ').toLowerCase()
}

function lastBackupAgeDays(job, now = Date.now()) {
  if (!job?.starttime) {
    return null
  }

  const timestamp = Date.parse(String(job.starttime).replace(' ', 'T'))
  if (!Number.isFinite(timestamp)) {
    return null
  }

  return Math.max(0, (now - timestamp) / (24 * 60 * 60 * 1000))
}

function backupWarningDays() {
  const normalized = Number(settings.clientBackupWarningDays)
  return Number.isInteger(normalized) && normalized > 0 ? normalized : 2
}

function backupAgeRatio(job) {
  const ageDays = lastBackupAgeDays(job)
  if (ageDays === null) {
    return 1
  }

  return Math.min(ageDays / backupWarningDays(), 1)
}

function backupAgeColor(job) {
  const hue = Math.round(120 - backupAgeRatio(job) * 120)
  return `hsl(${hue}, 75%, 42%)`
}

function clientBackupAgeBarStyle(client) {
  const ratio = backupAgeRatio(client.lastBackup)
  return {
    '--client-backup-age-width': `${Math.max(8, Math.round(ratio * 100))}%`,
    '--client-backup-age-color': backupAgeColor(client.lastBackup),
  }
}

function isFreshBackup(job) {
  if (!job) {
    return false
  }

  const clientKey = `${job.director ?? ''}:${job.client ?? ''}`
  const expected = expectedBackupByClient.value.get(clientKey)
  if (expected?.runtime) {
    const backupTimestamp = Date.parse(String(job.starttime ?? '').replace(' ', 'T'))
    return Number.isFinite(backupTimestamp)
      && backupTimestamp >= expected.runtime * 1000
  }

  const ageDays = lastBackupAgeDays(job)
  return ageDays !== null && ageDays <= backupWarningDays()
}

function clientHasLastBackupError(client) {
  return ['E', 'f'].includes(client.lastBackup?.status)
}

function clientMatchesQuickFilter(client) {
  const versionStatus = clientVersionInfo(client).status
  if (clientsQuickFilter.value === 'enabled') return client.enabled
  if (clientsQuickFilter.value === 'disabled') return !client.enabled
  if (clientsQuickFilter.value === 'outdated') {
    return ['update_required', 'upgrade_required'].includes(versionStatus)
  }
  if (clientsQuickFilter.value === 'stale_backup') {
    return !isFreshBackup(client.lastBackup)
  }
  if (clientsQuickFilter.value === 'backup_errors') {
    return clientHasLastBackupError(client)
  }
  return true
}

function clientMatchesFilters(client) {
  const needle = String(clientsSearch.value ?? '').trim().toLowerCase()
  const matchesSearch = !needle || normalizedClientSearchText(client).includes(needle)
  return matchesSearch && clientMatchesQuickFilter(client)
}

function osIcon(client)  { return osIconName(client)  }
function osColor(client) { return osIconColor(client) }

function clientVersionInfo(client) {
  return releaseInfo.getVersionInfo(client.version)
}

function clientVersionColor(client) {
  return ({
    uptodate: 'positive',
    update_required: 'warning',
    upgrade_required: 'negative',
    unknown: 'grey-6',
  })[clientVersionInfo(client).status] ?? 'grey-6'
}

function clientVersionTooltip(client) {
  const info = clientVersionInfo(client)
  if (info.status === 'unknown') return info.package_update_info
  return info.package_update_info || client.uname || ''
}

const columns = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',       label: t('Name'),        field: 'name',       align: 'left',   sortable: true },
  { name: 'os',         label: t('OS'),          field: 'os',         align: 'left',   sortable: true },
  { name: 'version',    label: t('Version'),     field: 'version',    align: 'left',   sortable: true },
  { name: 'lastBackup',    label: t('Last Backup'),    field: row => row.lastBackup?.starttime ?? '', align: 'left', sortable: true },
  { name: 'backupStatus',  label: t('Backup Status'),  field: row => clientBackupStatusSortValue(row), align: 'left', sortable: true },
  { name: 'versionStatus', label: t('Version Status'), field: row => clientVersionStatusSortValue(row), align: 'left', sortable: true },
  { name: 'enabled',       label: t('Enabled'),        field: 'enabled', align: 'center', sortable: true },
  { name: 'actions',       label: '',                  field: 'actions', align: 'center',  style: 'width:56px' },
])

function clientBackupStatusSortValue(client) {
  return clientBackupStatusItems(client).map(item => item.label).join(' ')
}

function clientVersionStatusSortValue(client) {
  return clientVersionStatusItems(client).map(item => item.label).join(' ')
}

function clientBackupStatusItems(client) {
  if (!client.enabled) {
    return [{ label: t('Disabled'), color: 'negative' }]
  }

  if (clientHasLastBackupError(client)) {
    return [{ label: t('Last backup failed'), color: 'negative' }]
  }

  if (!isFreshBackup(client.lastBackup)) {
    return [{ label: t('Stale backup'), color: 'warning' }]
  }

  return [{ label: t('Backup OK'), color: 'positive' }]
}

function clientVersionStatusItems(client) {
  const versionStatus = clientVersionInfo(client).status
  if (versionStatus === 'upgrade_required') {
    return [{ label: t('Upgrade required'), color: 'negative' }]
  }
  if (versionStatus === 'update_required') {
    return [{ label: t('Update available'), color: 'warning' }]
  }

  return [{ label: t('OK'), color: 'positive' }]
}

function buildClientJobLocation(job) {
  return {
    name: 'job-details',
    params: { id: job.id },
    query: buildJobDetailsQuery({
      director: job.director,
      clientName: job.client,
      clientDirector: job.director,
      clientsScopeDirector: clientsListScopeDirector.value,
    }),
  }
}

function formatClientBackupTime(value) {
  if (!value) {
    return '—'
  }

  return settings.relativeTime
    ? formatSqlRelativeTime(value, settings.locale)
    : value
}

// ── Enable / Disable toggle ───────────────────────────────────────────────────

const toggling = ref(null)

async function switchToClientDirector(client) {
  if (!client?.director) {
    return
  }

  await ensureScopeDirector(client.director)
}

async function openClientDetails(client) {
  try {
    await switchToClientDirector(client)
    await router.push({
      name: 'client-details',
      params: { name: client.name },
      query: buildClientDetailsQuery({
        director: client.director,
        clientsScopeDirector: clientsListScopeDirector.value,
      }),
    })
  } catch (error) {
    directorErrors.value = [{
      director: client.director ?? t('unknown'),
      message: error.message,
    }]
  }
}

function openClientDetailsFromRow(_event, row) {
  openClientDetails(row)
}

async function openClientJobs(client) {
  try {
    await switchToClientDirector(client)
    await router.push({
      name: 'jobs',
      query: {
        client: client.name,
        scopeDirector: client.director,
      },
    })
  } catch (error) {
    directorErrors.value = [{
      director: client.director ?? t('unknown'),
      message: error.message,
    }]
  }
}

async function openClientTimeline(client) {
  try {
    await switchToClientDirector(client)
    await router.push({
      name: 'jobs',
      query: {
        action: 'timeline',
        client: client.name,
        scopeDirector: client.director,
      },
    })
  } catch (error) {
    directorErrors.value = [{
      director: client.director ?? t('unknown'),
      message: error.message,
    }]
  }
}

async function openClientRestore(client) {
  try {
    await switchToClientDirector(client)
    await router.push({
      name: 'restore',
      query: {
        client: client.name,
        director: client.director,
      },
    })
  } catch (error) {
    directorErrors.value = [{
      director: client.director ?? t('unknown'),
      message: error.message,
    }]
  }
}

async function toggleEnabled(client) {
  toggling.value = client.scopeKey
  try {
    await switchToClientDirector(client)
    const cmd = client.enabled
      ? `disable client=${quoteDirectorString(client.name)}`
      : `enable client=${quoteDirectorString(client.name)}`
    await director.call(cmd)
    await refresh()
  } finally {
    toggling.value = null
  }
}

// ── Client status dialog ──────────────────────────────────────────────────────

const statusDialog = ref({ open: false, client: '', loading: false, text: '' })
const clientStatusSummary = computed(() => parseClientStatusSummary(statusDialog.value.text))
const clientStatusSummaryRows = computed(() => [
  {
    label: t('Connection'),
    value: clientStatusSummary.value.reachable ? t('Reachable') : t('Connection failed'),
  },
  {
    label: t('Version'),
    value: clientStatusSummary.value.version || t('Unknown'),
  },
  {
    label: t('Running jobs'),
    value: clientStatusSummary.value.runningJobs === null
      ? t('Unknown')
      : String(clientStatusSummary.value.runningJobs),
  },
  {
    label: t('Warnings'),
    value: String(clientStatusSummary.value.warningCount),
  },
  {
    label: t('Errors'),
    value: String(clientStatusSummary.value.errorCount),
  },
])

async function showStatus(client) {
  statusDialog.value = { open: true, client: client.name, loading: true, text: '' }
  try {
    await switchToClientDirector(client)
    const result = await director.rawCall(
      `status client=${quoteDirectorString(client.name)}`
    )
    statusDialog.value.text = result
  } catch (e) {
    statusDialog.value.text = `${t('Error')}: ${e.message ?? e}`
  } finally {
    statusDialog.value.loading = false
  }
}

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => route.query.tab, (value) => {
  if (value === undefined) {
    return
  }

  const query = { ...route.query }
  delete query.tab
  router.replace({ path: '/clients', query })
}, { immediate: true })

watch(() => route.query.scopeDirector, (value) => {
  if (typeof value === 'string' && value && !activeDirectors.value.includes(value)) {
    router.replace({
      path: '/clients',
      query: withClientsScopeDirectorQuery(route.query, ''),
    })
    return
  }

  refresh()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  if (typeof route.query.scopeDirector === 'string'
    && route.query.scopeDirector
    && !activeDirectors.value.includes(route.query.scopeDirector)) {
    router.replace({
      path: '/clients',
      query: withClientsScopeDirectorQuery(route.query, ''),
    })
  }
  refresh()
})
</script>

<style scoped>
.clients-list-header {
  gap: 8px;
}

.clients-list-header__title {
  margin-right: 8px;
  white-space: nowrap;
}

.clients-list-header__search {
  min-width: 220px;
  width: min(320px, 100%);
}

.clients-list-header__threshold {
  min-width: 190px;
}

.clients-list-header__filters {
  border: 1px solid rgba(255, 255, 255, 0.55);
  border-radius: 6px;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.12);
  max-width: 100%;
  overflow: hidden;
  overflow-x: auto;
}

.clients-list-header__filters :deep(.q-btn + .q-btn) {
  border-left: 1px solid rgba(21, 101, 192, 0.16);
}

.clients-list-stats {
  flex-wrap: wrap;
}

.clients-list-stats :deep(.q-chip) {
  font-weight: 600;
}

.client-backup-age-bar {
  background: rgba(0, 0, 0, 0.12);
  border-radius: 999px;
  height: 4px;
  overflow: hidden;
  width: 120px;
}

.client-backup-age-bar::before {
  background: var(--client-backup-age-color);
  border-radius: inherit;
  content: '';
  display: block;
  height: 100%;
  transition: background-color 0.2s ease, width 0.2s ease;
  width: var(--client-backup-age-width);
}

.client-status-output {
  background: #1e1e1e;
  color: #d4d4d4;
  font-family: monospace;
  font-size: 0.82rem;
  line-height: 1.5;
  padding: 12px 16px;
  margin: 0;
  white-space: pre-wrap;
  overflow-x: auto;
  max-height: 70vh;
  overflow-y: auto;
}
</style>
