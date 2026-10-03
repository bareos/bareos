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
              v-model.number="settings.clientBackupWarningFailedJobs"
              dense
              outlined
              type="number"
              min="1"
              max="1000"
              :label="t('Warn after N consecutive failures')"
              class="clients-list-header__threshold"
              data-testid="clients-backup-warning-failed-jobs"
            >
              <template #prepend><q-icon name="repeat" /></template>
              <q-tooltip>
                {{ t('Clients with at least this many consecutive failed backups since their last successful backup are shown as warning.') }}
              </q-tooltip>
            </q-input>
            <q-space />
            <q-input
              v-model="clientsSearch"
              dense
              outlined
              clearable
              :placeholder="t('Search…')"
              class="clients-list-header__search"
              data-testid="clients-search"
            >
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" :title="t('Refresh')" :aria-label="t('Refresh')" @click="refresh(true)" />
          </q-card-section>
          <q-card-section class="q-py-sm clients-list-stats" data-testid="clients-quick-filter">
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
                v-if="clientStats.consecutiveFailures"
                dense square outline color="warning" text-color="black"
                icon="repeat"
                clickable
                :selected="clientsQuickFilter === 'consecutive_failures'"
                @click="clientsQuickFilter = 'consecutive_failures'"
              >
                {{ t('Consecutive failures') }}: {{ clientStats.consecutiveFailures }}
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
            <q-banner v-if="error" dense rounded class="bg-negative text-white q-mb-md">{{ error }}</q-banner>
            <q-table
              v-if="!(loading && !clients.length)"
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
                      class="client-failure-bar q-mt-xs"
                      :style="clientFailureBarStyle(props.row)"
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
                    :title="t('Actions')" :aria-label="t('Actions')"
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
             <TableSkeleton v-else :columns="columns.length" :rows="8" />
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
            :title="t('Close')" :aria-label="t('Close')"
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
import { buildJobDetailsQuery, consecutiveFailedJobsSinceSuccess } from '../utils/jobs.js'
import { formatSqlRelativeTime } from '../utils/locales.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useReleaseInfoStore } from '../stores/releaseInfo.js'
import { useSettingsStore } from '../stores/settings.js'
import { usePersistedTablePagination } from '../composables/usePersistedTablePagination.js'
import { usePersistedTableFilter } from '../composables/usePersistedTableFilter.js'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import TableSkeleton from '../components/TableSkeleton.vue'
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
  rowsPerPage: 20,
  sortBy: 'name',
  descending: false,
}, {
  persistSort: true,
})

const rawClients = ref([])
const rawRecentBackups = ref([])
const clientsSearch = usePersistedTableFilter('clients.list')
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
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = clientsPageDirectors.value[0]
    await ensureSingleScopeDirector()
    const [listResult, dotResult, recentBackupsResult] = await Promise.all([
      director.call('llist clients'),
      director.call('.clients'),
      director.call('llist jobs reverse limit=1000 sortby=starttime jobtype=B'),
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
  const key = `${entry.director ?? ''}:${client.name}`
  const lastBackup = lastBackupByClient.value.get(key) ?? null
  const failureStreak = failureStreakByClient.value.get(key) ?? 0
  return {
    ...client,
    director: entry.director,
    scopeKey: entry.scopeKey ?? key,
    lastBackup,
    failureStreak,
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
    consecutiveFailures: all.filter(client => hasFailureStreakWarning(client)).length,
    backupErrors: all.filter(client => clientHasLastBackupError(client)).length,
  }
})

const jobsByClient = computed(() => {
  const jobsByClientKey = new Map()
  for (const backup of directorCollection(rawRecentBackups.value)) {
    const job = {
      ...normaliseJob(backup),
      director: backup.director,
    }
    if (!job.client) {
      continue
    }
    const key = `${job.director ?? ''}:${job.client}`
    if (!jobsByClientKey.has(key)) {
      jobsByClientKey.set(key, [])
    }
    jobsByClientKey.get(key).push(job)
  }
  // Sort each client's jobs newest-first explicitly: the underlying
  // "llist jobs reverse sortby=starttime" query is already newest-first,
  // but the aggregated-directors path merges multiple directors' results
  // without re-sorting, so this must not be assumed here.
  for (const jobs of jobsByClientKey.values()) {
    jobs.sort((a, b) => String(b.starttime ?? '').localeCompare(String(a.starttime ?? '')))
  }
  return jobsByClientKey
})

const lastBackupByClient = computed(() => {
  const backupsByClient = new Map()
  for (const [key, jobs] of jobsByClient.value) {
    if (jobs.length) {
      backupsByClient.set(key, jobs[0])
    }
  }
  return backupsByClient
})

const failureStreakByClient = computed(() => {
  const streakByClient = new Map()
  for (const [key, jobs] of jobsByClient.value) {
    streakByClient.set(key, consecutiveFailedJobsSinceSuccess(jobs))
  }
  return streakByClient
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

function backupWarningFailedJobs() {
  const normalized = Number(settings.clientBackupWarningFailedJobs)
  return Number.isInteger(normalized) && normalized > 0 ? normalized : 2
}

function hasFailureStreakWarning(client) {
  return (client.failureStreak ?? 0) >= backupWarningFailedJobs()
}

function failureStreakRatio(client) {
  const streak = client.failureStreak ?? 0
  if (streak <= 0) {
    return 0
  }

  return Math.min(streak / backupWarningFailedJobs(), 1)
}

function failureStreakColor(client) {
  const hue = Math.round(120 - failureStreakRatio(client) * 120)
  return `hsl(${hue}, 75%, 42%)`
}

function clientFailureBarStyle(client) {
  const ratio = failureStreakRatio(client)
  return {
    '--client-failure-bar-width': `${Math.max(8, Math.round(ratio * 100))}%`,
    '--client-failure-bar-color': failureStreakColor(client),
  }
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
  if (clientsQuickFilter.value === 'consecutive_failures') {
    return hasFailureStreakWarning(client)
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

  if (hasFailureStreakWarning(client)) {
    return [{
      label: t('{n} consecutive failures', { n: client.failureStreak }),
      color: 'warning',
    }]
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
  width: 200px;
}

.clients-list-header__threshold {
  min-width: 190px;
}

.clients-list-stats {
  flex-wrap: wrap;
}

.clients-list-stats :deep(.q-chip) {
  font-weight: 600;
}

.client-failure-bar {
  background: rgba(0, 0, 0, 0.12);
  border-radius: 999px;
  height: 4px;
  overflow: hidden;
  width: 120px;
}

.client-failure-bar::before {
  background: var(--client-failure-bar-color);
  border-radius: inherit;
  content: '';
  display: block;
  height: 100%;
  transition: background-color 0.2s ease, width 0.2s ease;
  width: var(--client-failure-bar-width);
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
