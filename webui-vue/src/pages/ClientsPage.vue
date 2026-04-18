<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     label="Show"     no-caps />
      <q-tab name="timeline" label="Timeline" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>
      <!-- SHOW -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Client List</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
            <q-table :rows="clients" :columns="columns" row-key="name" dense flat :loading="loading" :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-name="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'client-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
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
                  <span v-if="props.value" class="text-mono">{{ props.value }}</span>
                  <span v-else class="text-grey-5">—</span>
                  <div v-if="props.row.arch || props.row.buildDate" class="text-caption text-grey-6" style="line-height:1.2">
                    <span v-if="props.row.arch">{{ props.row.arch }}</span>
                    <span v-if="props.row.arch && props.row.buildDate"> · </span>
                    <span v-if="props.row.buildDate">{{ props.row.buildDate }}</span>
                  </div>
                  <q-tooltip v-if="props.row.uname">{{ props.row.uname }}</q-tooltip>
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'" :label="props.value ? 'Enabled' : 'Disabled'" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                   <q-btn flat round dense size="sm"
                          :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                          :color="props.row.enabled ? 'warning' : 'positive'"
                          :title="props.row.enabled ? 'Disable' : 'Enable'"
                          :loading="toggling === props.row.name"
                          @click="toggleEnabled(props.row)" />
                   <q-btn flat round dense size="sm" icon="info" title="Status"
                          :data-testid="`client-status-${props.row.name}`"
                          @click="showStatus(props.row.name)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- TIMELINE -->
      <q-tab-panel name="timeline" class="q-pa-none">
        <JobTimeline />
      </q-tab-panel>
    </q-tab-panels>

    <!-- Client status dialog -->
    <q-dialog v-model="statusDialog.open">
      <q-card style="min-width:600px; max-width:90vw">
        <q-card-section class="panel-header row items-center q-py-sm">
          <span>Status: {{ statusDialog.client }}</span>
          <q-space />
          <q-btn flat round dense icon="close" color="white" v-close-popup />
        </q-card-section>
        <q-card-section class="q-pa-none">
          <q-inner-loading :showing="statusDialog.loading" />
          <pre v-if="statusDialog.text" data-testid="client-status-output" class="client-status-output">{{ statusDialog.text }}</pre>
          <div v-else-if="!statusDialog.loading" class="text-grey q-pa-md text-center">No output</div>
        </q-card-section>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import {
  directorCollection,
  normaliseClient,
} from '../composables/useDirectorFetch.js'
import { osIconName, osIconColor, osLabel } from '../utils/osIcon.js'
import { useDirectorStore } from '../stores/director.js'
import JobTimeline from '../components/JobTimeline.vue'

const tab = ref('list')
const director = useDirectorStore()

const rawClients = ref([])
const loading    = ref(false)
const error      = ref(null)

async function refresh() {
  if (!director.isConnected) {
    error.value = 'Not connected to director'
    return
  }
  loading.value = true
  error.value   = null
  try {
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
    }))
  } catch (e) {
    error.value = e.message ?? String(e)
  } finally {
    loading.value = false
  }
}

onMounted(refresh)

const clients = computed(() => directorCollection(rawClients.value).map(normaliseClient))

function osIcon(client)  { return osIconName(client)  }
function osColor(client) { return osIconColor(client) }

const columns = [
  { name: 'name',    label: 'Name',    field: 'name',    align: 'left',   sortable: true },
  { name: 'os',      label: 'OS',      field: 'os',      align: 'left'    },
  { name: 'version', label: 'Version', field: 'version', align: 'left',   sortable: true },
  { name: 'enabled', label: 'Status',  field: 'enabled', align: 'center'  },
  { name: 'actions', label: '',        field: 'actions', align: 'center',  style: 'width:80px' },
]

// ── Enable / Disable toggle ───────────────────────────────────────────────────

const toggling = ref(null)

async function toggleEnabled(client) {
  toggling.value = client.name
  try {
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

async function showStatus(name) {
  statusDialog.value = { open: true, client: name, loading: true, text: '' }
  try {
    const result = await director.rawCall(`status client=${name}`)
    statusDialog.value.text = result
  } catch (e) {
    statusDialog.value.text = `Error: ${e.message ?? e}`
  } finally {
    statusDialog.value.loading = false
  }
}
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
