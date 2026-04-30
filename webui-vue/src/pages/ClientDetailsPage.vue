<template>
  <q-page class="q-pa-md">
    <q-inner-loading :showing="loading" :label="t('Loading client…')" />
    <div v-if="error" class="text-negative q-pa-md">{{ error }}</div>
    <template v-else-if="!loading">
    <div class="row items-center q-mb-md">
      <q-btn
        flat
        icon="arrow_back"
        :label="backLabel"
        :to="backLocation"
        no-caps
        class="q-mr-md"
      />
      <q-icon v-if="client" :name="osIcon(client)" :color="osColor(client)" size="28px" class="q-mr-sm" />
      <div class="text-h6">{{ client?.name }}</div>
      <q-badge v-if="client?.version" color="grey-6" :label="'v' + client.version" class="q-ml-sm text-mono" />
    </div>
    <div v-if="client" class="row q-col-gutter-md">
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">{{ t('Client Details') }}</q-card-section>
          <q-card-section>
            <q-list dense separator>
              <q-item v-for="row in details" :key="row.label">
                <q-item-section class="text-weight-medium" style="max-width:140px">{{ row.label }}</q-item-section>
                <q-item-section>
                  <span v-if="row.icon" class="row items-center no-wrap">
                    <q-icon :name="row.icon" :color="row.iconColor" size="18px" class="q-mr-xs" />
                    {{ row.value }}
                  </span>
                  <span v-else>{{ row.value }}</span>
                </q-item-section>
              </q-item>
            </q-list>
          </q-card-section>
        </q-card>
      </div>
      <div class="col-12 col-md-6">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Recent Jobs') }}</span>
            <q-space />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="clientJobs" :columns="jobCols" row-key="id" dense flat hide-bottom
                     :pagination="{ rowsPerPage: 6 }">
              <template #body-cell-id="props">
                <q-td :props="props">
                  <router-link
                    :to="{ name: 'job-details', params: { id: props.value }, query: buildClientJobDetailsQuery(props.row) }"
                    class="text-primary"
                  >
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-level="props">
                <q-td :props="props" class="text-center">
                  <JobLevelBadge v-if="props.value" :level="props.value" />
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-starttime="props">
                <q-td :props="props">
                  <span :title="settings.relativeTime ? props.value : timeAgo(props.value, settings.locale)">
                    {{ settings.relativeTime ? timeAgo(props.value, settings.locale) : props.value }}
                  </span>
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right" style="min-width:90px">
                  <div>{{ fmtBytes(props.row.bytes) }}</div>
                  <q-linear-progress
                    :value="bytesGauge(props.row.bytes)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </div>
    </div>
    <div v-else class="text-grey text-center q-pa-xl">{{ t('Client not found.') }}</div>
    </template>
  </q-page>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { formatBytes, timeAgo } from '../mock/index.js'
import {
  directorCollection,
  normaliseClient,
  normaliseJob,
} from '../composables/useDirectorFetch.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildJobDetailsQuery } from '../utils/jobs.js'
import {
  resolveClientDetailsDashboardOrigin,
  resolveClientDetailsJobsOrigin,
  resolveClientsListQuery,
} from '../utils/clients.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'

const route         = useRoute()
const auth          = useAuthStore()
const director      = useDirectorStore()
const settings      = useSettingsStore()
const fmtBytes      = formatBytes
const { t } = useI18n()
const requestedDirector = computed(() => (
  typeof route.query.director === 'string' ? route.query.director : ''
))
const currentClientDirector = computed(() => (
  requestedDirector.value || auth.user?.director || settings.directorName || ''
))
const backToClientsQuery = computed(() => resolveClientsListQuery(route.query))
const jobsOrigin = computed(() => resolveClientDetailsJobsOrigin(route.query))
const dashboardOrigin = computed(() => resolveClientDetailsDashboardOrigin(route.query))
const backLabel = computed(() => (
  jobsOrigin.value && Object.keys(jobsOrigin.value).length > 0
    ? t('Back to Jobs')
    : (dashboardOrigin.value ? t('Back to Dashboard') : t('Back to Clients'))
))
const backLocation = computed(() => (
  jobsOrigin.value && Object.keys(jobsOrigin.value).length > 0
    ? { name: 'jobs', query: jobsOrigin.value }
    : (
      dashboardOrigin.value
        ? { name: 'dashboard' }
        : { name: 'clients', query: backToClientsQuery.value }
    )
))

function buildClientJobDetailsQuery(job) {
  return buildJobDetailsQuery({
    director: job?.director,
    clientName: typeof route.params.name === 'string' ? route.params.name : '',
    clientDirector: currentClientDirector.value,
    clientsTab: typeof route.query.clientsTab === 'string' ? route.query.clientsTab : '',
    clientsScopeDirector: typeof route.query.clientsScopeDirector === 'string'
      ? route.query.clientsScopeDirector
      : '',
    clientDashboardOrigin: dashboardOrigin.value,
    clientJobsAction: typeof route.query.jobsAction === 'string' ? route.query.jobsAction : '',
    clientJobsStatus: typeof route.query.jobsStatus === 'string' ? route.query.jobsStatus : '',
    clientJobsSearch: typeof route.query.jobsSearch === 'string' ? route.query.jobsSearch : '',
    clientJobsScopeDirector: typeof route.query.jobsScopeDirector === 'string'
      ? route.query.jobsScopeDirector
      : '',
  })
}

const loading    = ref(true)
const clientData = ref(null)
const clientJobs = ref([])
const error      = ref(null)

function osIcon(client)  { return osIconName(client)  }
function osColor(client) { return osIconColor(client) }

async function ensureClientDirector() {
  if (!requestedDirector.value) {
    return
  }

  if (auth.user?.director === requestedDirector.value && director.isConnected) {
    return
  }

  await switchActiveDirector(requestedDirector.value)
}

async function loadClient() {
  const name = route.params.name
  await ensureClientDirector()

  const [clientRes, jobsRes, defaultsRes] = await Promise.allSettled([
    director.call(`llist clients`),
    director.call(`list jobs client=${name} reverse`),
    director.call(`.defaults client=${name}`),
  ])
  if (clientRes.status === 'fulfilled') {
    const list = directorCollection(clientRes.value?.clients)
    const found = list.find(c => c.name === name)
    if (found) {
      const defaults = defaultsRes.status === 'fulfilled' ? (defaultsRes.value ?? {}) : {}
      clientData.value = normaliseClient({ ...found, ...defaults })
    } else {
      clientData.value = null
    }
  }
  if (jobsRes.status === 'fulfilled') {
    clientJobs.value = directorCollection(jobsRes.value?.jobs).map(job => ({
      ...normaliseJob(job),
      director: currentClientDirector.value || null,
    }))
  }
}

watch(() => `${route.params.name}\u0000${requestedDirector.value}`, async () => {
  loading.value = true
  error.value = null
  clientData.value = null
  clientJobs.value = []
  try {
    await loadClient()
  } catch (loadError) {
    error.value = loadError.message
  } finally {
    loading.value = false
  }
}, { immediate: true })

const client = computed(() => clientData.value)

const maxBytes = computed(() =>
  Math.max(1, ...clientJobs.value.map(j => Number(j.bytes) || 0))
)
function bytesGauge(val) {
  const n = Number(val) || 0
  return n / maxBytes.value
}

const details = computed(() => {
  if (!client.value) return []
  const c = client.value
  return [
    { label: t('OS'),          value: osLabel(c.os) + (c.osInfo ? ` — ${c.osInfo}` : ''),
      icon: osIcon(c), iconColor: osColor(c) },
    { label: t('Architecture'), value: c.arch      || '—' },
    { label: t('Version'),      value: c.version   || '—' },
    { label: t('Build Date'),   value: c.buildDate || '—' },
    { label: t('Address'),      value: c.address   || '—' },
    { label: t('Port'),         value: c.port      || '—' },
    { label: t('Status'),       value: c.enabled   ? t('Enabled') : t('Disabled') },
    { label: t('File Retention'), value: c.fileretention || '—' },
    { label: t('Job Retention'),  value: c.jobretention  || '—' },
  ]
})

const jobCols = computed(() => [
  { name: 'id',        label: t('ID'),     field: 'id',        align: 'right' },
  { name: 'name',      label: t('Job'),    field: 'name',      align: 'left'  },
  { name: 'level',     label: t('Level'),  field: 'level',     align: 'center'},
  { name: 'starttime', label: t('Start'),  field: 'starttime', align: 'left'  },
  { name: 'bytes',     label: t('Bytes'),  field: 'bytes',     align: 'right' },
  { name: 'status',    label: t('Status'), field: 'status',    align: 'center'},
])
</script>
