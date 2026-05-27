<template>
  <q-page class="q-pa-md">
    <DirectorScopePanel
      v-model="selectedDirectorsModel"
      :title="t('Restore Scope')"
      :summary-label="restoreScopeLabel"
      :options="directorOptions"
      :help-text="t('Select the directors that contribute to the restore source choices.')"
      :errors="directorErrors"
      data-test-id="restore-directors"
    />

    <q-card flat bordered class="bareos-panel">
      <q-card-section class="panel-header">{{ t('Restore Files') }}</q-card-section>
      <q-card-section>

        <!-- Source + Destination -->
        <div class="row q-col-gutter-md q-mb-md">

          <!-- Source -->
          <div class="col-12 col-md-6">
            <q-card flat bordered>
                <q-card-section class="text-subtitle2 q-pb-xs">{{ t('Source') }}</q-card-section>
                <q-card-section class="q-pt-none q-gutter-sm">
                  <q-select
                    v-if="isCommonRestore"
                    v-model="commonSourceDirector"
                    :options="commonSourceDirectorOptions"
                    :label="t('Source Director')"
                    outlined dense emit-value map-options
                    :hint="t('Restore jobs and destination clients come from the selected director')"
                  />
                  <q-select
                    v-model="sourceClientKey"
                    :options="clientOptions"
                    :label="t('Backup Client')"
                    outlined dense emit-value map-options
                    :loading="loadingClients"
                    :hint="t('Select a client to browse its backups')"
                    data-testid="restore-source-client"
                    @update:model-value="onClientChange"
                  />
                  <q-select
                    v-model="form.jobid"
                    :options="backupOptions"
                    :label="t('Backup Job')"
                    outlined dense emit-value map-options
                    :loading="loadingBackups"
                    :disable="!form.client || loadingBackups"
                    no-error-icon
                    :hint="t('Select a completed backup job')"
                    data-testid="restore-backup-job"
                    @update:model-value="initBrowser"
                  />
                <q-checkbox
                  v-model="form.mergeJobsets"
                  :label="t('Include related jobs (Full + Incremental/Differential)')"
                  dense
                />
              </q-card-section>
            </q-card>
          </div>

          <!-- Destination -->
          <div class="col-12 col-md-6">
            <q-card flat bordered>
              <q-card-section class="text-subtitle2 q-pb-xs">{{ t('Destination') }}</q-card-section>
              <q-card-section class="q-pt-none q-gutter-sm">
                <q-select
                  v-model="form.restoreclient"
                  :options="restoreClientOptions"
                  :label="t('Restore to Client')"
                  outlined dense emit-value map-options
                  :loading="loadingClients"
                  :disable="!sourceDirector"
                  data-testid="restore-target-client"
                />
                <q-select
                  v-model="form.restorejob"
                  :options="restoreJobOptions"
                  :label="t('Restore Job')"
                  outlined dense emit-value map-options
                  :loading="loadingRestoreJobs"
                  :disable="!sourceDirector"
                  data-testid="restore-job"
                />
                <q-input
                  v-model="form.where"
                  :label="t('Restore to (Where)')"
                  outlined dense
                  placeholder="/ or /tmp/bareos-restores"
                  data-testid="restore-where"
                />
                <q-select
                  v-model="form.replace"
                  :options="replaceOptions"
                  :label="t('Replace Policy')"
                  outlined dense emit-value map-options
                  data-testid="restore-replace-policy"
                />
              </q-card-section>
            </q-card>
          </div>
        </div>

        <!-- File Browser -->
        <q-card flat bordered class="q-mb-md">
          <q-card-section class="panel-header q-py-xs q-px-md">
            <div class="row items-center justify-between">
                <span class="text-body2">{{ t('Browse Files') }}</span>
              <div class="row items-center q-gutter-sm">
                <span v-if="selectedFiles.size || selectedDirs.size" class="text-caption text-grey-5">
                  <q-icon name="description" size="14px" class="q-mr-xs" />
                  {{ selectedFiles.size }}
                  <q-icon name="folder" size="14px" class="q-mx-xs" />
                  {{ selectedDirs.size }}
                </span>
                <q-btn
                  flat dense round size="sm"
                  :icon="versionCheckEnabled ? 'history' : 'history_toggle_off'"
                  color="white"
                  :style="versionCheckEnabled ? '' : 'opacity:0.45'"
                  @click="toggleVersionCheck"
                >
                  <q-tooltip>{{
                    versionCheckEnabled
                      ? t('Version check enabled – click to disable (improves performance)')
                      : t('Version check disabled – click to enable')
                  }}</q-tooltip>
                </q-btn>
                <q-spinner v-if="loadingBrowser && !browserReady" size="20px" color="primary" />
              </div>
            </div>
          </q-card-section>

          <!-- Breadcrumbs -->
          <q-card-section v-if="browserReady" class="q-py-sm q-px-md">
            <div class="row items-center q-gutter-xs text-caption">
              <q-icon name="storage" size="16px" color="grey-7" />
              <template v-for="(crumb, i) in navStack" :key="i">
                <q-btn
                  flat dense no-caps size="sm"
                  :label="crumb.label"
                  :class="i === navStack.length - 1 ? 'text-dark text-weight-medium' : 'text-primary'"
                  class="q-px-xs"
                  @click="navigateTo(i)"
                />
                <q-icon v-if="i < navStack.length - 1" name="chevron_right" size="14px" color="grey-7" />
              </template>
            </div>
          </q-card-section>

          <!-- Browser table -->
          <template v-if="browserReady">
            <q-table
              :rows="currentEntries"
              :columns="browserCols"
              row-key="key"
              flat dense
              :loading="loadingBrowser"
              :pagination="{ rowsPerPage: 200, sortBy: 'name' }"
              hide-bottom
              style="min-height: 260px"
              data-testid="restore-browser"
            >
              <template #header-cell-sel="props">
                <q-th :props="props" style="width:36px">
                  <q-checkbox
                    dense size="sm"
                    :model-value="allCurrentSelected"
                    @update:model-value="toggleSelectAll"
                  />
                </q-th>
              </template>
              <template #body="props">
                <q-tr :props="props">
                  <q-td style="width:36px">
                    <q-checkbox
                      dense size="sm"
                      :model-value="isSelected(props.row)"
                      @update:model-value="(v) => toggleItem(props.row, v)"
                    />
                  </q-td>
                  <q-td style="width:28px">
                    <q-icon
                      :name="props.row.isDir ? 'folder' : fileIcon(props.row.name)"
                      :color="props.row.isDir ? 'amber-6' : 'blue-grey-4'"
                      size="18px"
                    />
                  </q-td>
                  <q-td>
                    <span
                      v-if="props.row.isDir"
                      class="cursor-pointer text-primary"
                      @click="navigateInto(props.row)"
                    >{{ props.row.name }}</span>
                    <span v-else>{{ props.row.name }}</span>
                    <template v-if="!props.row.isDir">
                      <q-btn
                        v-if="hasMultipleVersions(props.row.fileId)"
                        flat dense round size="xs"
                        :color="hasVersionOverride(props.row.fileId) ? 'orange' : 'grey-5'"
                        icon="history"
                        class="q-ml-xs"
                        @click.stop="openVersions(props.row)"
                      >
                        <q-tooltip>{{
                          hasVersionOverride(props.row.fileId)
                            ? 'Specific version selected – click to change'
                            : `Browse file versions – ${versionCount(props.row.fileId)} available`
                        }}</q-tooltip>
                      </q-btn>
                    </template>
                  </q-td>
                  <q-td class="text-caption text-grey-6 text-mono" style="width:90px">
                    {{ props.row.mode }}
                  </q-td>
                  <q-td class="text-caption text-grey-6" style="width:80px">
                    {{ props.row.user }}
                  </q-td>
                  <q-td class="text-caption text-grey-6" style="width:80px">
                    {{ props.row.group }}
                  </q-td>
                  <q-td class="text-right text-caption text-grey-6" style="width:90px">
                    {{ props.row.isDir ? '' : formatBytes(props.row.size) }}
                  </q-td>
                  <q-td class="text-caption text-grey-6" style="width:160px">
                    {{ props.row.mtime ? formatMtime(props.row.mtime) : '' }}
                  </q-td>
                </q-tr>
              </template>
            </q-table>
          </template>
          <template v-else>
            <div class="text-center text-grey q-py-xl">
              <template v-if="browserPlaceholder === 'error'">
                <q-icon name="error_outline" size="48px" color="negative" /><br />
                <div class="text-caption text-negative q-mt-sm q-mb-md">{{ browserError }}</div>
                <q-btn flat dense color="primary" :label="t('Retry')" icon="refresh" @click="initBrowser" :loading="loadingBrowser" />
              </template>
              <template v-else-if="browserPlaceholder === 'loading'">
                <q-spinner size="48px" color="primary" /><br />
                <span class="text-caption q-mt-sm">{{ t('Loading file list...') }}</span>
              </template>
              <template v-else>
                <q-icon name="folder_open" size="48px" /><br />
                <span class="text-caption q-mt-sm">{{ t('Select a client and backup job above to browse files') }}</span>
              </template>
            </div>
          </template>
        </q-card>

        <!-- Action row -->
        <div class="row items-center q-gutter-sm">
          <q-btn
            color="primary" :label="t('Restore')" icon="restore"
            :disable="!canRestore"
            :loading="loadingRestore"
            data-testid="restore-submit"
            @click="doRestore"
          />
          <q-btn flat :label="t('Reset')" @click="resetAll" />
          <q-space />
          <span v-if="!canRestore && form.jobid" class="text-caption text-grey-6">
            {{ t('Select at least one file or folder to restore') }}
          </span>
        </div>

        <!-- Result banner -->
        <q-banner
          v-if="restoreResult"
          :class="restoreResult.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
          class="q-mt-md"
          rounded
          data-testid="restore-result"
        >
          <template #avatar>
            <q-icon :name="restoreResult.ok ? 'check_circle' : 'error'" />
          </template>
          {{ restoreResult.message }}
          <template v-if="restoreResult.jobid" #action>
            <router-link
              :to="{
                name: 'job-details',
                params: { id: restoreResult.jobid },
                query: buildRestoreJobDetailsQuery(),
              }"
              class="text-white"
            >
              {{ t('View Job') }} {{ restoreResult.jobid }}
            </router-link>
          </template>
        </q-banner>

      </q-card-section>
    </q-card>
  </q-page>

  <!-- File Versions Dialog -->
  <q-dialog v-model="versionsDialog.open" max-width="700px">
    <q-card style="min-width:500px;max-width:700px">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Versions of') }} {{ versionsDialog.fname }}</span>
        <q-space />
        <q-btn flat round dense icon="close" color="white" v-close-popup />
      </q-card-section>
      <q-card-section>
        <q-inner-loading :showing="versionsDialog.loading" />
        <div v-if="versionsDialog.error" class="text-negative">{{ versionsDialog.error }}</div>
        <q-table
          v-else
          flat dense
          :rows="versionsDialog.versions"
          :columns="versionsCols"
          row-key="fileid"
          hide-pagination
          :rows-per-page-options="[0]"
        >
          <template #body="props">
            <q-tr
              :props="props"
              :class="versionsDialog.selectedFileId === props.row.fileid ? 'bg-blue-1' : ''"
              class="cursor-pointer"
              @click="selectVersion(props.row)"
            >
              <q-td>
                <q-radio
                  dense
                  :model-value="versionsDialog.selectedFileId"
                  :val="props.row.fileid"
                  @update:model-value="selectVersion(props.row)"
                />
              </q-td>
              <q-td>{{ props.row.jobid }}</q-td>
              <q-td>{{ formatMtime(props.row.stat?.mtime) }}</q-td>
              <q-td class="text-right">{{ formatBytes(props.row.stat?.size ?? 0) }}</q-td>
              <q-td>{{ props.row.volumename }}</q-td>
              <q-td>{{ props.row.md5 }}</q-td>
            </q-tr>
          </template>
        </q-table>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="t('Cancel')" v-close-popup />
        <q-btn
          color="primary" :label="t('Use this version')"
          :disable="versionsDialog.selectedFileId === null"
          @click="applyVersion"
          v-close-popup
        />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { directorCollection } from '../composables/useDirectorFetch.js'
import { fetchAggregatedClients } from '../composables/clientsAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { formatBytes } from '../mock/index.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { buildDirectorOptions } from '../utils/director.js'
import {
  canNavigateRestoreBrowser,
  buildRestoreSourceQuery,
  filterRestoreSourceClients,
  getRestoreBrowserPlaceholder,
  pushRestoreBreadcrumb,
  resolveRestoreSourceClient,
  resolveRestoreSourceDirector,
  truncateRestoreBreadcrumbs,
} from '../utils/restore.js'
import { buildJobDetailsQuery } from '../utils/jobs.js'
import DirectorScopePanel from '../components/DirectorScopePanel.vue'

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const route    = useRoute()
const router   = useRouter()
const { t } = useI18n()

// ── Form state ──────────────────────────────────────────────────────────────
const form = ref({
  client:        '',
  restoreclient: '',
  restorejob:    '',
  jobid:         null,
  where:         '/tmp/bareos-restores',
  replace:       'Always',
  mergeJobsets:  true,
})

const sourceClientKey = ref('')

const directorOptions = computed(() => {
  return buildDirectorOptions({
    availableDirectors: director.availableDirectors,
    selectedDirectors: settings.selectedDirectors,
    currentDirector: auth.user?.director,
    fallbackDirector: settings.directorName,
  })
})

function syncSelectedDirectors() {
  const validDirectors = directorOptions.value.map(option => option.value)
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

const selectedDirectorsModel = computed({
  get: () => settings.selectedDirectors,
  set: (value) => {
    const selected = Array.isArray(value) ? value : []
    if (selected.length > 0) {
      settings.setSelectedDirectors(selected)
      return
    }

    const fallbackDirector = auth.user?.director || settings.directorName
    settings.setSelectedDirectors(fallbackDirector ? [fallbackDirector] : [])
  },
})

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    directorOptions.value.some(option => option.value === value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isCommonRestore = computed(() => activeDirectors.value.length > 1)
const restoreScopeLabel = computed(() => (
  isCommonRestore.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))
const commonSourceDirector = ref('')
const commonSourceDirectorOptions = computed(() => (
  activeDirectors.value.map(value => ({ label: value, value }))
))

const replaceOptions = computed(() => [
  { label: t('Always'), value: 'Always' },
  { label: t('If Newer'), value: 'IfNewer' },
  { label: t('If Older'), value: 'IfOlder' },
  { label: t('Never'), value: 'Never' },
])

// ── Clients ─────────────────────────────────────────────────────────────────
const sourceClients   = ref([])
const restoreClients  = ref([])
const directorErrors  = ref([])
const loadingClients = ref(false)

const selectedSourceClient = computed(() => (
  sourceClients.value.find(client => client.scopeKey === sourceClientKey.value) ?? null
))
const visibleSourceClients = computed(() => (
  isCommonRestore.value
    ? filterRestoreSourceClients(sourceClients.value, commonSourceDirector.value)
    : sourceClients.value
))
const sourceDirector = computed(() => (
  selectedSourceClient.value?.director
    ?? (isCommonRestore.value ? commonSourceDirector.value : null)
    ?? (activeDirectors.value.length === 1 ? activeDirectors.value[0] : null)
))
const sourceClientName = computed(() => selectedSourceClient.value?.name ?? '')

const clientOptions = computed(() =>
  visibleSourceClients.value.map(client => ({
    label: isCommonRestore.value ? `${client.director} / ${client.name}` : client.name,
    value: client.scopeKey,
  }))
)

const restoreClientOptions = computed(() =>
  restoreClients.value.map(client => ({ label: client.name, value: client.name }))
)

async function ensureScopeDirector(targetDirector) {
  if (!targetDirector) {
    return
  }

  if (auth.user?.director === targetDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(targetDirector)
}

async function ensureSelectedSourceDirector() {
  await ensureScopeDirector(sourceDirector.value)
}

function buildRestoreJobDetailsQuery() {
  return buildJobDetailsQuery({
    director: sourceDirector.value,
    restoreClient: sourceClientName.value,
    restoreDirector: sourceDirector.value,
    restoreJobid: form.value.jobid,
  })
}

function extractQueuedJobId(value) {
  if (value == null) {
    return null
  }

  if (typeof value === 'number' && Number.isFinite(value)) {
    return String(value)
  }

  if (typeof value === 'string') {
    const match = value.match(/jobid["=: ]+("?)(\d+)/i)
      ?? value.match(/queued[^0-9]*(\d+)/i)
    return match ? match[2] ?? match[1] : null
  }

  if (Array.isArray(value)) {
    for (const entry of value) {
      const nested = extractQueuedJobId(entry)
      if (nested) {
        return nested
      }
    }
    return null
  }

  if (typeof value === 'object') {
    for (const [key, nestedValue] of Object.entries(value)) {
      if (/jobid/i.test(key)) {
        const direct = extractQueuedJobId(nestedValue)
        if (direct) {
          return direct
        }
      }
    }

    for (const nestedValue of Object.values(value)) {
      const nested = extractQueuedJobId(nestedValue)
      if (nested) {
        return nested
      }
    }
  }

  return null
}

async function syncRouteToSourceSelection() {
  const query = buildRestoreSourceQuery(route.query, {
    clientName: sourceClientName.value,
    directorName: sourceDirector.value,
    jobid: form.value.jobid,
  })

  const currentClient = typeof route.query.client === 'string' ? route.query.client : undefined
  const currentDirector = typeof route.query.director === 'string' ? route.query.director : undefined
  const currentJobid = typeof route.query.jobid === 'string' ? route.query.jobid : undefined

  if (
    currentClient === query.client
    && currentDirector === query.director
    && currentJobid === query.jobid
  ) {
    return
  }

  await router.replace({
    path: route.path,
    query,
  })
}

async function applyRouteSourceSelection() {
  const qClient = typeof route.query.client === 'string' ? route.query.client : ''
  const qDirector = typeof route.query.director === 'string' ? route.query.director : ''
  const qJobid = typeof route.query.jobid === 'string' ? route.query.jobid : ''
  const resolvedDirector = resolveRestoreSourceDirector(activeDirectors.value, qDirector)

  if (resolvedDirector) {
    commonSourceDirector.value = resolvedDirector
  } else {
    syncCommonSourceDirector()
  }

  if (qClient) {
    const resolvedClient = resolveRestoreSourceClient(sourceClients.value, {
      clientName: qClient,
      directorName: resolvedDirector,
      currentDirector: auth.user?.director || settings.directorName,
    })

    if (resolvedClient) {
      if (sourceClientKey.value !== resolvedClient.scopeKey) {
        sourceClientKey.value = resolvedClient.scopeKey
        await onClientChange(resolvedClient.scopeKey)
      }
      if (qJobid) {
        const nextJobid = Number(qJobid)
        if (form.value.jobid !== nextJobid) {
          form.value.jobid = nextJobid
          await initBrowser()
        }
      } else if (form.value.jobid !== null) {
        form.value.jobid = null
        browserReady.value = false
        clearBrowserState()
      }
      return
    }
  }

  if (sourceClientKey.value || form.value.client || form.value.jobid !== null) {
    sourceClientKey.value = ''
    clearSelectedSourceData()
  }

  if (sourceDirector.value) {
    await Promise.all([
      loadRestoreClients(),
      loadRestoreJobs(),
    ])
  } else {
    restoreClients.value = []
    restoreJobs.value = []
    form.value.restoreclient = ''
    form.value.restorejob = ''
  }
}

function syncCommonSourceDirector() {
  if (!isCommonRestore.value) {
    commonSourceDirector.value = ''
    return
  }

  const validDirectors = activeDirectors.value
  if (!validDirectors.length) {
    commonSourceDirector.value = ''
    return
  }

  const selectedDirector = selectedSourceClient.value?.director
  if (selectedDirector && validDirectors.includes(selectedDirector)) {
    commonSourceDirector.value = selectedDirector
    return
  }

  if (validDirectors.includes(commonSourceDirector.value)) {
    return
  }

  commonSourceDirector.value = validDirectors[0]
}

function clearSelectedSourceData({ keepRestoreClient = false } = {}) {
  form.value.client = ''
  form.value.jobid = null
  form.value.restorejob = ''
  if (!keepRestoreClient) {
    form.value.restoreclient = ''
  }
  backups.value = []
  restoreJobs.value = []
  browserReady.value = false
  clearBrowserState()
}

async function loadClients() {
  loadingClients.value = true
  directorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      sourceClients.value = []
      restoreClients.value = []
      sourceClientKey.value = ''
      clearSelectedSourceData()
      return
    }

    if (isCommonRestore.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedClients(credentials, activeDirectors.value)
      sourceClients.value = result.clients
      directorErrors.value = result.directorErrors
    } else {
      const currentDirector = activeDirectors.value[0]
      await ensureScopeDirector(currentDirector)
      const r = await director.call('list clients')
      sourceClients.value = directorCollection(r?.clients)
        .map(client => ({
          ...client,
          director: currentDirector,
          scopeKey: `${currentDirector}:${client.name}`,
        }))
        .sort((left, right) => String(left.name ?? '').localeCompare(String(right.name ?? '')))
      restoreClients.value = sourceClients.value
    }

    if (!sourceClients.value.some(client => client.scopeKey === sourceClientKey.value)) {
      sourceClientKey.value = ''
      clearSelectedSourceData()
    }
  } catch (_) {
    sourceClients.value = []
    restoreClients.value = []
    sourceClientKey.value = ''
    clearSelectedSourceData()
  } finally {
    loadingClients.value = false
  }
}

// ── Restore jobs ─────────────────────────────────────────────────────────────
const restoreJobs        = ref([])
const loadingRestoreJobs = ref(false)

const restoreJobOptions = computed(() =>
  restoreJobs.value.map(j => ({ label: j.name, value: j.name }))
)

async function loadRestoreJobs() {
  if (!sourceDirector.value) {
    restoreJobs.value = []
    form.value.restorejob = ''
    return
  }

  loadingRestoreJobs.value = true
  try {
    await ensureSelectedSourceDirector()
    const r = await director.call('.jobs type=R')
    restoreJobs.value = directorCollection(r?.jobs)
    if (!restoreJobs.value.some(job => job.name === form.value.restorejob)) {
      form.value.restorejob = ''
    }
    if (restoreJobs.value.length && !form.value.restorejob) {
      form.value.restorejob = restoreJobs.value[0].name
    }
  } catch (_) {
    restoreJobs.value = []
    form.value.restorejob = ''
  } finally {
    loadingRestoreJobs.value = false
  }
}

async function loadRestoreClients() {
  if (!sourceDirector.value) {
    restoreClients.value = []
    form.value.restoreclient = ''
    return
  }

  try {
    await ensureSelectedSourceDirector()
    const r = await director.call('list clients')
    restoreClients.value = directorCollection(r?.clients)
      .map(client => ({
        ...client,
        director: sourceDirector.value,
        scopeKey: `${sourceDirector.value}:${client.name}`,
      }))
      .sort((left, right) => String(left.name ?? '').localeCompare(String(right.name ?? '')))
    if (!restoreClients.value.some(client => client.name === form.value.restoreclient)) {
      form.value.restoreclient = sourceClientName.value || ''
    }
  } catch (_) {
    restoreClients.value = []
    form.value.restoreclient = ''
  }
}

// ── Backups for selected client ──────────────────────────────────────────────
const backups        = ref([])
const loadingBackups = ref(false)

const backupOptions = computed(() =>
  backups.value.map(b => ({
    label: `#${b.jobid}  ${b.name}  [${levelLabel(b.level)}]  ${b.starttime}  (${formatBytes(Number(b.jobbytes ?? 0))}, ${b.jobfiles ?? 0} files)`,
    value: b.jobid,
  }))
)

async function onClientChange(clientName) {
  form.value.jobid = null
  browserReady.value = false
  clearBrowserState()
  backups.value = []

  const selectedClient = sourceClients.value.find(client => client.scopeKey === clientName) ?? null
  form.value.client = selectedClient?.name ?? ''
  form.value.restoreclient = selectedClient?.name ?? ''
  if (selectedClient?.director && selectedClient.director !== commonSourceDirector.value) {
    commonSourceDirector.value = selectedClient.director
  }

  if (!selectedClient) {
    if (sourceDirector.value) {
      await Promise.all([
        loadRestoreClients(),
        loadRestoreJobs(),
      ])
    } else {
      restoreClients.value = []
      restoreJobs.value = []
      form.value.restorejob = ''
    }
    return
  }

  await Promise.all([
    loadRestoreClients(),
    loadRestoreJobs(),
  ])
  await loadBackups(selectedClient)
}

async function loadBackups(client) {
  loadingBackups.value = true
  try {
    await ensureScopeDirector(client.director)
    const r = await director.call(`llist backups client=${quoteDirectorString(client.name)}`)
    backups.value = directorCollection(r?.backups)
      .sort((a, b) => Number(b.jobid) - Number(a.jobid))
  } catch (_) {
    backups.value = []
  } finally {
    loadingBackups.value = false
  }
}

// ── BVFS browser ─────────────────────────────────────────────────────────────
const browserReady    = ref(false)
const loadingBrowser  = ref(false)
const browserError    = ref('')
const mergedJobids    = ref('')  // comma-separated job IDs for BVFS commands

// Navigation breadcrumb stack:  [{ label, pathId }]  pathId=null means root (path='')
const navStack = ref([{ label: '/', pathId: null }])

// Current directory contents: mixed dirs + files
const currentEntries = ref([])
const browserPlaceholder = computed(() => getRestoreBrowserPlaceholder({
  browserError: browserError.value,
  loadingBrowser: loadingBrowser.value,
  hasSelectedJob: !!form.value.jobid,
}))

// Selection: Map<key, {FileId or PathId, name, isDir}>
const selectedFiles = ref(new Map())  // key = FileId
const selectedDirs  = ref(new Map())  // key = PathId

const browserCols = computed(() => [
  { name: 'sel',   label: '',            field: 'sel',   align: 'center', style: 'width:36px' },
  { name: 'icon',  label: '',            field: 'isDir', align: 'center', style: 'width:28px' },
  { name: 'name',  label: t('Name'),     field: 'name',  align: 'left',   sortable: true },
  { name: 'mode',  label: t('Mode'),     field: 'mode',  align: 'left',   style: 'width:90px', sortable: true },
  { name: 'user',  label: t('User'),     field: 'user',  align: 'left',   style: 'width:80px', sortable: true },
  { name: 'group', label: t('Group'),    field: 'group', align: 'left',   style: 'width:80px', sortable: true },
  { name: 'size',  label: t('Size'),     field: 'size',  align: 'right',  style: 'width:90px', sortable: true },
  { name: 'mtime', label: t('Modified'), field: 'mtime', align: 'left',   style: 'width:160px', sortable: true },
])

async function initBrowser() {
  if (!form.value.jobid) return
  loadingBrowser.value = true
  browserReady.value   = false
  browserError.value   = ''
  try {
    await ensureSelectedSourceDirector()
    // Step 1: get merged jobids
    const flag = form.value.mergeJobsets ? ' all' : ''
    let gjr
    try {
      gjr = await director.call(`.bvfs_get_jobids jobid=${form.value.jobid}${flag}`)
    } catch (e) {
      // If get_jobids fails, fall back to using only the selected jobid
      gjr = null
    }
    const ids = directorCollection(gjr?.jobids).map(j => j.id).filter(Boolean).join(',')
    mergedJobids.value = ids || String(form.value.jobid)

    // Step 2: update BVFS cache (non-fatal — may be slow on large catalogs)
    try {
      await director.call(`.bvfs_update jobid=${mergedJobids.value}`)
    } catch (_) { /* ignore */ }

    // Step 3: browse root
    navStack.value = [{ label: '/', pathId: null }]
    await fetchDir(null)
    browserReady.value = true
  } catch (e) {
    browserError.value = e.message || t('Failed to load file tree')
  } finally {
    loadingBrowser.value = false
  }
}

async function fetchDir(pathId) {
  await ensureSelectedSourceDirector()
  const jids = mergedJobids.value
  const pathArg = pathId != null ? `pathid=${pathId}` : `path=${quoteDirectorString('/')}`
  const [dr, fr] = await Promise.all([
    director.call(`.bvfs_lsdirs jobid=${jids} ${pathArg} limit=2000`).catch(() => null),
    director.call(`.bvfs_lsfiles jobid=${jids} ${pathArg} limit=2000`).catch(() => null),
  ])
  const dirs  = (dr?.directories ?? [])
    .filter(d => d.name && d.name !== '.' && d.name !== '..' && d.name !== './' && d.name !== '../')
    .map(d => ({
      key:    `d-${d.pathid}`,
      isDir:  true,
      name:   d.name.endsWith('/') ? d.name : d.name + '/',
      pathId: d.pathid,
      fileId: d.fileid,
      mode:   formatMode(d.stat?.mode ?? 0),
      user:   d.stat?.user ?? '',
      group:  d.stat?.group ?? '',
      size:   0,
      mtime:  d.stat?.mtime ?? 0,
    }))
  const files = (fr?.files ?? [])
    .map(f => ({
      key:    `f-${f.fileid}`,
      isDir:  false,
      name:   f.name,
      pathId: f.pathid,
      fileId: f.fileid,
      mode:   formatMode(f.stat?.mode ?? 0),
      user:   f.stat?.user ?? '',
      group:  f.stat?.group ?? '',
      size:   f.stat?.size ?? 0,
      mtime:  f.stat?.mtime ?? 0,
    }))
  currentEntries.value = [...dirs, ...files]

  // Reset version counts for the new directory and check in background.
  versionCheckDirKey++
  fileHasVersions.value = new Map()
  const dirKey = versionCheckDirKey
  checkVersionsInBackground(files, dirKey)
}

async function navigateInto(row) {
  if (!canNavigateRestoreBrowser({
    browserReady: browserReady.value,
    loadingBrowser: loadingBrowser.value,
  })) {
    return
  }

  const nextStack = pushRestoreBreadcrumb(navStack.value, {
    label: row.name,
    pathId: row.pathId,
  })
  loadingBrowser.value = true
  browserReady.value = false
  browserError.value = ''
  try {
    await fetchDir(row.pathId)
    navStack.value = nextStack
    browserReady.value = true
  } catch (e) {
    browserError.value = e.message || t('Failed to load file tree')
  } finally {
    loadingBrowser.value = false
  }
}

async function navigateTo(index) {
  if (!canNavigateRestoreBrowser({
    browserReady: browserReady.value,
    loadingBrowser: loadingBrowser.value,
  }) || index === navStack.value.length - 1) {
    return
  }

  const nextStack = truncateRestoreBreadcrumbs(navStack.value, index)
  const pathId = nextStack[nextStack.length - 1].pathId
  loadingBrowser.value = true
  browserReady.value = false
  browserError.value = ''
  try {
    await fetchDir(pathId)
    navStack.value = nextStack
    browserReady.value = true
  } catch (e) {
    browserError.value = e.message || t('Failed to load file tree')
  } finally {
    loadingBrowser.value = false
  }
}

async function reloadCurrentDir() {
  if (!canNavigateRestoreBrowser({
    browserReady: browserReady.value,
    loadingBrowser: loadingBrowser.value,
  })) {
    return
  }

  const pathId = navStack.value[navStack.value.length - 1].pathId
  loadingBrowser.value = true
  browserReady.value = false
  browserError.value = ''
  try {
    await fetchDir(pathId)
    browserReady.value = true
  } catch (e) {
    browserError.value = e.message || t('Failed to load file tree')
  } finally {
    loadingBrowser.value = false
  }
}

// ── Selection helpers ─────────────────────────────────────────────────────────
function isSelected(row) {
  return row.isDir ? selectedDirs.value.has(row.pathId) : selectedFiles.value.has(row.fileId)
}

function toggleItem(row, value) {
  if (row.isDir) {
    if (value) selectedDirs.value.set(row.pathId, row.name)
    else        selectedDirs.value.delete(row.pathId)
  } else {
    if (value) selectedFiles.value.set(row.fileId, row.name)
    else        selectedFiles.value.delete(row.fileId)
  }
  // force reactivity
  selectedFiles.value = new Map(selectedFiles.value)
  selectedDirs.value  = new Map(selectedDirs.value)
}

const allCurrentSelected = computed(() => {
  if (!currentEntries.value.length) return false
  return currentEntries.value.every(r => isSelected(r))
})

function toggleSelectAll(value) {
  currentEntries.value.forEach(r => toggleItem(r, value))
}

function clearSelection() {
  selectedFiles.value = new Map()
  selectedDirs.value  = new Map()
}

// ── Restore execution ──────────────────────────────────────────────────────────
const loadingRestore = ref(false)
const restoreResult  = ref(null)

const canRestore = computed(() =>
  !!form.value.jobid &&
  !!form.value.restorejob &&
  (selectedFiles.value.size > 0 || selectedDirs.value.size > 0)
)

async function doRestore() {
  if (!canRestore.value) return
  loadingRestore.value = true
  restoreResult.value  = null
  const rnd     = Math.floor(Math.random() * 1000000)
  const bvfsPath = `b2000${rnd}`
  // Apply per-file version overrides
  const fileids  = [...selectedFiles.value.keys()]
    .map(fid => fileVersionOverrides.value.get(fid) ?? fid)
    .join(',')
  const dirids   = [...selectedDirs.value.keys()].join(',')
  const jids     = mergedJobids.value

  try {
    await ensureSelectedSourceDirector()
    // Step 1: build restore list in BVFS
    await director.call(
      `.bvfs_restore jobid=${jids} fileid=${fileids} dirid=${dirids} path=${quoteDirectorString(bvfsPath)}`
    )

    // Step 2: run restore job
    const src = form.value.client
    const dst = form.value.restoreclient || src
    const cmd = `restore file=?${bvfsPath}` +
      ` client=${quoteDirectorString(src)}` +
      ` restoreclient=${quoteDirectorString(dst)}` +
      ` restorejob=${quoteDirectorString(form.value.restorejob)}` +
      ` where=${quoteDirectorString(form.value.where)}` +
      ` replace=${quoteDirectorString(form.value.replace)}` +
      ` yes`
    const result = await director.call(cmd)

    const jobid = extractQueuedJobId(result)

    restoreResult.value = {
      ok:      true,
      message: jobid ? `Restore job queued with JobId ${jobid}.` : 'Restore job queued.',
      jobid,
    }

    // Step 3: clean up BVFS restore path
    try {
      await director.call(`.bvfs_cleanup path=${quoteDirectorString(bvfsPath)}`)
    } catch (_) {
      // Cleanup should not block the queued restore from being shown.
    }

    await router.push({
      name: 'jobs',
      query: {},
    })
  } catch (e) {
    restoreResult.value = { ok: false, message: `Restore failed: ${e.message}`, jobid: null }
  } finally {
    loadingRestore.value = false
  }
}

// ── Helpers ──────────────────────────────────────────────────────────────────
function clearBrowserState() {
  navStack.value      = [{ label: '/', pathId: null }]
  currentEntries.value = []
  selectedFiles.value  = new Map()
  selectedDirs.value   = new Map()
  fileVersionOverrides.value = new Map()
  fileHasVersions.value = new Map()
  versionCheckDirKey++
  mergedJobids.value   = ''
  browserError.value   = ''
}

function resetAll() {
  form.value.jobid = null
  form.value.where = '/tmp/bareos-restores'
  form.value.replace = 'Always'
  restoreResult.value = null
  browserReady.value = false
  clearBrowserState()
  backups.value = []
}

function levelLabel(l) {
  return l === 'F' ? 'Full' : l === 'I' ? 'Incremental' : l === 'D' ? 'Differential' : l
}

function formatMtime(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  const yyyy = d.getFullYear()
  const mm   = (d.getMonth() + 1).toString().padStart(2, '0')
  const dd   = d.getDate().toString().padStart(2, '0')
  const hh   = d.getHours().toString().padStart(2, '0')
  const min  = d.getMinutes().toString().padStart(2, '0')
  const ss   = d.getSeconds().toString().padStart(2, '0')
  return `${yyyy}-${mm}-${dd} ${hh}:${min}:${ss}`
}

function formatMode(mode) {
  if (!mode) return ''
  const types = { 0o040000: 'd', 0o120000: 'l', 0o060000: 'b',
                  0o020000: 'c', 0o010000: 'p', 0o140000: 's' }
  let type = '-'
  for (const [mask, ch] of Object.entries(types)) {
    if ((mode & 0o170000) === Number(mask)) { type = ch; break }
  }
  const bits = 'rwxrwxrwx'
  let perms = ''
  for (let i = 8; i >= 0; i--) {
    perms += (mode >> i) & 1 ? bits[8 - i] : '-'
  }
  // setuid/setgid/sticky
  if (mode & 0o4000) perms = perms.slice(0, 2) + (perms[2] === 'x' ? 's' : 'S') + perms.slice(3)
  if (mode & 0o2000) perms = perms.slice(0, 5) + (perms[5] === 'x' ? 's' : 'S') + perms.slice(6)
  if (mode & 0o1000) perms = perms.slice(0, 8) + (perms[8] === 'x' ? 't' : 'T')
  return type + perms
}

function fileIcon(name) {
  if (!name) return 'insert_drive_file'
  const ext = name.split('.').pop().toLowerCase()
  if (['jpg','jpeg','png','gif','svg','webp','bmp'].includes(ext)) return 'image'
  if (['mp4','mkv','avi','mov','webm'].includes(ext))               return 'movie'
  if (['mp3','flac','ogg','wav'].includes(ext))                     return 'music_note'
  if (['zip','tar','gz','bz2','xz','7z','rar'].includes(ext))       return 'folder_zip'
  if (['pdf'].includes(ext))                                         return 'picture_as_pdf'
  if (['txt','md','rst','log'].includes(ext))                        return 'article'
  if (['sh','py','js','ts','php','c','cpp','h','go','rs'].includes(ext)) return 'code'
  if (['conf','cfg','ini','yaml','yml','toml','json'].includes(ext)) return 'settings'
  return 'insert_drive_file'
}

// ── File Version Browser ──────────────────────────────────────────────────────
const versionsDialog = ref({
  open:           false,
  loading:        false,
  error:          '',
  fname:          '',
  pathId:         null,
  fileId:         null,   // original fileId (latest)
  versions:       [],
  selectedFileId: null,
})

// Tracks per-file overrides: Map<originalFileId, overrideFileId>
const fileVersionOverrides = ref(new Map())
// Computed for template reactivity (Vue unwraps the ref but Map mutations
// don't trigger updates — we always replace the Map to force reactivity)
const hasVersionOverride = (fileId) => fileVersionOverrides.value.has(fileId)

// ── Version check toggle (persisted) ──────────────────────────────────────────
const PREF_KEY = 'bareos.restorePage.versionCheckEnabled'
const versionCheckEnabled = ref(
  localStorage.getItem(PREF_KEY) !== 'false'
)
function toggleVersionCheck() {
  versionCheckEnabled.value = !versionCheckEnabled.value
  localStorage.setItem(PREF_KEY, String(versionCheckEnabled.value))
  if (!versionCheckEnabled.value) {
    // Clear cached counts when disabling
    versionCheckDirKey++
    fileHasVersions.value = new Map()
  }
}

// Tracks version counts per file: Map<fileId, number>
// Populated in background after each directory load.
const fileHasVersions = ref(new Map())
let versionCheckDirKey = 0  // incremented on each directory change to cancel stale checks

function hasMultipleVersions(fileId) {
  return (fileHasVersions.value.get(fileId) ?? 0) > 1 || hasVersionOverride(fileId)
}

function versionCount(fileId) {
  return fileHasVersions.value.get(fileId) ?? 0
}

async function checkVersionsInBackground(files, dirKey) {
  if (!files.length || !versionCheckEnabled.value) return
  await ensureSelectedSourceDirector()
  const jids = mergedJobids.value
  const client = sourceClientName.value
  await Promise.allSettled(
    files.map(async (f) => {
      if (dirKey !== versionCheckDirKey || !versionCheckEnabled.value) return
      try {
        const r = await director.call(
          `.bvfs_versions jobid=${jids} client="${client}" pathid=${f.pathId} fname=${f.name}`
        )
        if (dirKey !== versionCheckDirKey) return
        const count = directorCollection(r?.versions).length
        if (count > 1) {
          const next = new Map(fileHasVersions.value)
          next.set(f.fileId, count)
          fileHasVersions.value = next
        }
      } catch { /* ignore */ }
    })
  )
}

const versionsCols = [
  { name: 'sel',    label: '',        field: 'sel',                  align: 'center', style: 'width:36px' },
  { name: 'jobid',  label: 'Job ID',  field: 'jobid',                align: 'right',  style: 'width:70px', sortable: true },
  { name: 'mtime',  label: 'Date',    field: row => row.stat?.mtime, align: 'left', sortable: true },
  { name: 'size',   label: 'Size',    field: row => row.stat?.size,  align: 'right',  style: 'width:90px', sortable: true },
  { name: 'volume', label: 'Volume',  field: 'volumename',           align: 'left', sortable: true },
  { name: 'md5',    label: 'MD5',     field: 'md5',                  align: 'left', sortable: true },
]

async function openVersions(row) {
  versionsDialog.value = {
    open:           true,
    loading:        true,
    error:          '',
    fname:          row.name,
    pathId:         row.pathId,
    fileId:         row.fileId,
    versions:       [],
    selectedFileId: fileVersionOverrides.value.get(row.fileId) ?? row.fileId,
  }
  try {
    await ensureSelectedSourceDirector()
    const r = await director.call(
      `.bvfs_versions jobid=${mergedJobids.value} client="${sourceClientName.value}" pathid=${row.pathId} fname=${row.name}`
    )
    versionsDialog.value.versions = r?.versions ?? []
  } catch (e) {
    versionsDialog.value.error = e.message
  } finally {
    versionsDialog.value.loading = false
  }
}

function selectVersion(ver) {
  versionsDialog.value.selectedFileId = ver.fileid
}

function applyVersion() {
  const orig     = versionsDialog.value.fileId
  const selected = versionsDialog.value.selectedFileId
  if (selected === null) return
  const overrides = new Map(fileVersionOverrides.value)
  if (selected === orig) {
    overrides.delete(orig)
  } else {
    overrides.set(orig, selected)
  }
  fileVersionOverrides.value = overrides
  // Keep the file selected for restore using the chosen version
  if (!selectedFiles.value.has(orig)) {
    selectedFiles.value = new Map(selectedFiles.value)
    selectedFiles.value.set(orig, versionsDialog.value.fname)
  }
}

// ── Init ─────────────────────────────────────────────────────────────────────
async function init() {
  await director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  await loadClients()
  await applyRouteSourceSelection()
}

onMounted(() => { if (director.isConnected) init() })
watch(() => director.isConnected, (connected) => { if (connected) init() })
watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
  syncCommonSourceDirector()
})
watch(() => activeDirectors.value.join('\u0000'), async () => {
  const previousClientKey = sourceClientKey.value
  await loadClients()
  syncCommonSourceDirector()
  if (previousClientKey && sourceClients.value.some(client => client.scopeKey === previousClientKey)) {
    sourceClientKey.value = previousClientKey
    await onClientChange(previousClientKey)
    return
  }

  await applyRouteSourceSelection()
})
watch(() => commonSourceDirector.value, async (next, previous) => {
  if (!isCommonRestore.value || next === previous) {
    return
  }

  if (selectedSourceClient.value && selectedSourceClient.value.director !== next) {
    sourceClientKey.value = ''
    clearSelectedSourceData()
  }

  if (!next) {
    restoreClients.value = []
    restoreJobs.value = []
    form.value.restoreclient = ''
    form.value.restorejob = ''
    return
  }

  await Promise.all([
    loadRestoreClients(),
    loadRestoreJobs(),
  ])
})

watch(() => [sourceClientKey.value, sourceDirector.value, form.value.jobid], () => {
  syncRouteToSourceSelection()
})

watch(() => [route.query.client, route.query.director, route.query.jobid], async () => {
  await applyRouteSourceSelection()
})
</script>
