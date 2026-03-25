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
                  <q-btn flat round dense size="sm" icon="info" title="Details" />
                  <q-btn flat round dense size="sm" icon="monitor_heart" title="Status" />
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
              <template #body-cell-volretention="props">
                <q-td :props="props">{{ formatDuration(props.value) }}</q-td>
              </template>
              <template #body-cell-maxvolbytes="props">
                <q-td :props="props" class="text-right">
                  {{ Number(props.value) > 0 ? formatBytes(props.value) : '∞' }}
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
              <template #body-cell-inchanger="props">
                <q-td :props="props" class="text-center">
                  <BoolIcon :value="props.value" />
                </q-td>
              </template>
              <template #body-cell-volstatus="props">
                <q-td :props="props">
                  <q-badge :color="statusColor(props.value)" :label="props.value" />
                </q-td>
              </template>
              <template #body-cell-volbytes="props">
                <q-td :props="props" class="text-right">{{ formatBytes(props.value) }}</q-td>
              </template>
              <template #body-cell-maxvolbytes="props">
                <q-td :props="props" class="text-right">
                  {{ Number(props.value) > 0 ? formatBytes(props.value) : '∞' }}
                </q-td>
              </template>
              <template #body-cell-retention="props">
                <q-td :props="props">{{ formatDuration(props.value) }}</q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRoute, RouterLink } from 'vue-router'
import { useDirectorFetch, normaliseVolume } from '../composables/useDirectorFetch.js'
import { formatBytes, formatDuration } from '../mock/index.js'
import BoolIcon from '../components/BoolIcon.vue'
import EnabledBadge from '../components/EnabledBadge.vue'

const route = useRoute()
const tab = ref(route.query.tab || 'storages')
const volSearch = ref('')

const { data: rawStorages, loading: storagesLoading, error: storagesError, refresh: refreshStorages } =
  useDirectorFetch('list storages', 'storages')
const { data: rawPools,    loading: poolsLoading,    error: poolsError,    refresh: refreshPools } =
  useDirectorFetch('llist pools', 'pools')
const { data: rawVolumes,  loading: volsLoading,     error: volsError,     refresh: refreshVols } =
  useDirectorFetch('list volumes', 'volumes')

const storages = computed(() => (rawStorages.value ?? []).map(s => ({
  ...s,
  autochanger: s.autochanger === '1' || s.autochanger === true,
  enabled:     s.enabled !== '0' && s.enabled !== false,
})))

const pools = computed(() => (rawPools.value ?? []).map(p => ({
  ...p,
  numvols: Number(p.numvols ?? 0),
  maxvols: Number(p.maxvols ?? 0),
})))

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

const storageCols = [
  { name: 'name',        label: 'Name',        field: 'name',        align: 'left',  sortable: true },
  { name: 'address',     label: 'Address',     field: 'address',     align: 'left'   },
  { name: 'mediatype',   label: 'Media Type',  field: 'mediatype',   align: 'left'   },
  { name: 'autochanger', label: 'Autochanger', field: 'autochanger', align: 'center' },
  { name: 'enabled',     label: 'Status',      field: 'enabled',     align: 'center' },
  { name: 'actions',     label: '',            field: 'actions',     align: 'center', style: 'width:80px' },
]
const poolCols = [
  { name: 'name',         label: 'Name',         field: 'name',         align: 'left',  sortable: true },
  { name: 'pooltype',     label: 'Type',         field: 'pooltype',     align: 'left'   },
  { name: 'numvols',      label: 'Volumes',      field: 'numvols',      align: 'right', sortable: true },
  { name: 'maxvols',      label: 'Max Volumes',  field: 'maxvols',      align: 'right'  },
  { name: 'volretention', label: 'Retention',    field: 'volretention', align: 'left'   },
  { name: 'maxvoljobs',   label: 'Max Jobs/Vol', field: 'maxvoljobs',   align: 'right'  },
  { name: 'maxvolbytes',  label: 'Max Bytes/Vol',field: 'maxvolbytes',  align: 'right'  },
]
const volumeCols = [
  { name: 'volumename',  label: 'Volume Name',   field: 'volumename',  align: 'left',  sortable: true },
  { name: 'pool',        label: 'Pool',          field: 'pool',        align: 'left',  sortable: true },
  { name: 'storage',     label: 'Storage',       field: 'storage',     align: 'left'   },
  { name: 'mediatype',   label: 'Media Type',    field: 'mediatype',   align: 'left'   },
  { name: 'lastwritten', label: 'Last Written',  field: 'lastwritten', align: 'left',  sortable: true },
  { name: 'volstatus',   label: 'Status',        field: 'volstatus',   align: 'center', sortable: true },
  { name: 'inchanger',   label: 'In Changer',    field: 'inchanger',   align: 'center' },
  { name: 'retention',   label: 'Retention',     field: 'retention',   align: 'left'   },
  { name: 'maxvolbytes', label: 'Max Bytes',     field: 'maxvolbytes', align: 'right'  },
  { name: 'volbytes',    label: 'Used Bytes',    field: 'volbytes',    align: 'right'  },
]

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative', Purged: 'grey' }[s] || 'info'
}
</script>
