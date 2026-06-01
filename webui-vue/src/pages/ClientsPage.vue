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

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     :label="t('Show')"     no-caps />
      <q-tab name="timeline" :label="t('Timeline')" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>
      <!-- SHOW -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Client List') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
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
            <q-table :rows="clients" :columns="columns" row-key="scopeKey" dense flat :loading="loading" :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-name="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openClientDetails(props.row)">
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
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                    <q-badge :color="props.value ? 'positive' : 'negative'" :label="props.value ? t('Enabled') : t('Disabled')" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                   <q-btn flat round dense size="sm"
                          :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                          :color="props.row.enabled ? 'orange-10' : 'positive'"
                          :title="props.row.enabled ? t('Disable') : t('Enable')"
                          :loading="toggling === props.row.scopeKey"
                          @click="toggleEnabled(props.row)" />
                    <q-btn flat round dense size="sm" icon="info" :title="t('Status')"
                          :data-testid="`client-status-${props.row.scopeKey}`"
                          @click="showStatus(props.row)" />
                 </q-td>
               </template>
             </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- TIMELINE -->
      <q-tab-panel name="timeline" class="q-pa-none">
        <JobTimeline
          :key="clientsPageDirectors.join('\u0000') || 'clients-timeline'"
          :directors="clientsPageDirectors"
          :client-details-query="timelineClientDetailsQuery"
        />
      </q-tab-panel>
    </q-tab-panels>

    <!-- Client status dialog -->
    <q-dialog v-model="statusDialog.open">
      <q-card style="min-width:600px; max-width:90vw">
        <q-card-section class="panel-header row items-center q-py-sm">
          <span>{{ t('Status') }}: {{ statusDialog.client }}</span>
          <q-space />
          <q-btn flat round dense icon="close" color="white" v-close-popup />
        </q-card-section>
        <q-card-section class="q-pa-none">
          <q-inner-loading :showing="statusDialog.loading" />
          <pre v-if="statusDialog.text" data-testid="client-status-output" class="client-status-output">{{ statusDialog.text }}</pre>
          <div v-else-if="!statusDialog.loading" class="text-grey q-pa-md text-center">{{ t('No output') }}</div>
        </q-card-section>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import {
  directorCollection,
  normaliseClient,
} from '../composables/useDirectorFetch.js'
import { fetchAggregatedClients } from '../composables/clientsAggregate.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import {
  buildClientDetailsQuery,
  resolveClientsScopeDirector,
  withClientsScopeDirectorQuery,
} from '../utils/clients.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useReleaseInfoStore } from '../stores/releaseInfo.js'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
import JobTimeline from '../components/JobTimeline.vue'

const validTabs = new Set(['list', 'timeline'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'list'
}

const route = useRoute()
const tab = ref(normaliseTab(route.query.tab))
const auth = useAuthStore()
const director = useDirectorStore()
const releaseInfo = useReleaseInfoStore()
const router = useRouter()
const { t } = useI18n()

const rawClients = ref([])
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
const timelineClientDetailsQuery = computed(() => buildClientDetailsQuery({
  clientsTab: tab.value,
  clientsScopeDirector: clientsListScopeDirector.value,
}))

async function refresh() {
  loading.value = true
  error.value   = null
  directorErrors.value = []
  try {
    if (clientsPageDirectors.value.length === 0) {
      rawClients.value = []
      return
    }

    if (clientsPageDirectors.value.length > 1 || (clientsListScopeDirector.value && isCommonClients.value)) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedClients(credentials, clientsPageDirectors.value)
      rawClients.value = result.clients
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = clientsPageDirectors.value[0]
    await ensureSingleScopeDirector()
    const [listResult, dotResult] = await Promise.all([
      director.call('llist clients'),
      director.call('.clients'),
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

const clients = computed(() => directorCollection(rawClients.value).map((entry) => {
  const client = normaliseClient(entry)
  return {
    ...client,
    director: entry.director,
    scopeKey: entry.scopeKey ?? `${entry.director ?? ''}:${client.name}`,
  }
}))

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
  { name: 'name',    label: t('Name'),    field: 'name',    align: 'left',   sortable: true },
  { name: 'os',      label: t('OS'),      field: 'os',      align: 'left',   sortable: true },
  { name: 'version', label: t('Version'), field: 'version', align: 'left',   sortable: true },
  { name: 'enabled', label: t('Status'),  field: 'enabled', align: 'center', sortable: true },
  { name: 'actions', label: '',           field: 'actions', align: 'center',  style: 'width:80px' },
])

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
        clientsTab: tab.value,
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

async function toggleEnabled(client) {
  toggling.value = client.scopeKey
  try {
    await switchToClientDirector(client)
    const cmd = client.enabled
      ? `disable client=${client.name}`
      : `enable client=${client.name}`
    await director.call(cmd)
    await refresh()
  } finally {
    toggling.value = null
  }
}

// ── Client status dialog ──────────────────────────────────────────────────────

const statusDialog = ref({ open: false, client: '', loading: false, text: '' })

async function showStatus(client) {
  statusDialog.value = { open: true, client: client.name, loading: true, text: '' }
  try {
    await switchToClientDirector(client)
    const result = await director.rawCall(`status client=${client.name}`)
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
  const next = normaliseTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(tab, (value) => {
  const next = normaliseTab(value)
  const current = normaliseTab(route.query.tab)
  if (current === next) {
    return
  }

  const query = { ...route.query }
  delete query.tab
  if (next !== 'list') {
    query.tab = next
  }

  router.replace({ path: '/clients', query })
})

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
