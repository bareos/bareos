<template>
  <q-page class="q-pa-md">
    <!-- Back -->
    <q-btn flat no-caps icon="arrow_back" label="Pools" class="q-mb-md"
           :to="{ name: 'storages', query: { tab: 'pools' } }" />

    <!-- Loading / error -->
    <q-spinner v-if="loading" size="40px" class="block q-mx-auto q-mt-xl" />
    <q-banner v-else-if="error" class="bg-negative text-white q-mb-md">{{ error }}</q-banner>

    <template v-else-if="pool">
      <!-- Header -->
      <div class="row items-center q-mb-md q-gutter-sm">
        <q-icon name="mdi-database" size="2rem" color="primary" />
        <div>
          <div class="text-h5">{{ pool.name }}</div>
          <q-badge :color="poolTypeColor(pool.pooltype)" class="q-mt-xs">{{ pool.pooltype }}</q-badge>
          <q-badge :color="pool.enabled !== '0' ? 'positive' : 'grey'" class="q-mt-xs q-ml-xs">
            {{ pool.enabled !== '0' ? 'Enabled' : 'Disabled' }}
          </q-badge>
        </div>
      </div>

      <div class="row q-col-gutter-md">
        <!-- Details card -->
        <div class="col-12 col-md-5">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">Details</q-card-section>
            <q-card-section class="q-pa-none">
              <q-list dense separator>
                <q-item v-for="row in detailRows" :key="row.label">
                  <q-item-section class="text-grey-6" style="max-width:160px">{{ row.label }}</q-item-section>
                  <q-item-section>{{ row.value }}</q-item-section>
                </q-item>
              </q-list>
            </q-card-section>
          </q-card>
        </div>

        <!-- Volumes card -->
        <div class="col-12 col-md-7">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header row items-center">
              <span>Volumes ({{ volumes.length }})</span>
            </q-card-section>
            <q-card-section class="q-pa-none">
              <q-table :rows="volumes" :columns="volumeCols" row-key="volumename"
                       dense flat :pagination="{ rowsPerPage: 10 }">
                <template #body-cell-volstatus="props">
                  <q-td :props="props">
                    <q-badge :color="statusColor(props.value)" :label="props.value" />
                  </q-td>
                </template>
                <template #body-cell-volbytes="props">
                  <q-td :props="props" class="text-right">{{ formatBytes(props.value) }}</q-td>
                </template>
                <template #body-cell-inchanger="props">
                  <q-td :props="props" class="text-center">
                    <q-icon :name="props.value === '1' ? 'check' : 'remove'"
                            :color="props.value === '1' ? 'positive' : 'grey'" size="xs" />
                  </q-td>
                </template>
              </q-table>
            </q-card-section>
          </q-card>
        </div>
      </div>
    </template>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useRoute } from 'vue-router'
import { useDirectorStore } from '../stores/director.js'
import { formatBytes, formatDuration } from '../mock/index.js'

const route     = useRoute()
const director  = useDirectorStore()
const poolName  = route.params.name

const pool    = ref(null)
const volumes = ref([])
const loading = ref(true)
const error   = ref(null)

onMounted(async () => {
  try {
    const [poolRes, volRes] = await Promise.all([
      director.call(`llist pool=${poolName}`),
      director.call(`list volumes pool=${poolName}`),
    ])
    const pools = poolRes?.pools ?? []
    pool.value    = Array.isArray(pools) ? pools[0] : Object.values(pools)[0]
    const vols    = volRes?.volumes ?? []
    volumes.value = Array.isArray(vols) ? vols : Object.values(vols).flat()
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
})

const detailRows = computed(() => {
  if (!pool.value) return []
  const p = pool.value
  return [
    { label: 'Pool ID',           value: p.poolid },
    { label: 'Label Format',      value: p.labelformat || '—' },
    { label: 'Volumes',           value: `${p.numvols ?? 0} / ${p.maxvols ?? '∞'}` },
    { label: 'Vol Retention',     value: formatDuration(p.volretention) },
    { label: 'Vol Use Duration',  value: p.voluseduration > 0 ? formatDuration(p.voluseduration) : '—' },
    { label: 'Max Jobs/Vol',      value: p.maxvoljobs > 0 ? p.maxvoljobs : '∞' },
    { label: 'Max Bytes/Vol',     value: p.maxvolbytes > 0 ? formatBytes(p.maxvolbytes) : '∞' },
    { label: 'Auto Prune',        value: p.autoprune === '1' ? 'Yes' : 'No' },
    { label: 'Recycle',           value: p.recycle === '1' ? 'Yes' : 'No' },
    { label: 'Use Once',          value: p.useonce === '1' ? 'Yes' : 'No' },
    { label: 'Accept Any Volume', value: p.acceptanyvolume === '1' ? 'Yes' : 'No' },
    { label: 'Scratch Pool ID',   value: p.scratchpoolid > 0 ? p.scratchpoolid : '—' },
    { label: 'Recycle Pool ID',   value: p.recyclepoolid > 0 ? p.recyclepoolid : '—' },
  ]
})

const volumeCols = [
  { name: 'volumename', label: 'Volume',       field: 'volumename', align: 'left',   sortable: true },
  { name: 'volstatus',  label: 'Status',       field: 'volstatus',  align: 'center', sortable: true },
  { name: 'volbytes',   label: 'Used',         field: 'volbytes',   align: 'right',  sortable: true },
  { name: 'lastwritten',label: 'Last Written', field: 'lastwritten',align: 'left',   sortable: true },
  { name: 'inchanger',  label: 'In Changer',   field: 'inchanger',  align: 'center' },
  { name: 'storage',    label: 'Storage',      field: 'storage',    align: 'left'   },
]

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative', Purged: 'grey', Used: 'info' }[s] || 'info'
}

function poolTypeColor(t) {
  return { Backup: 'primary', Scratch: 'grey', Archive: 'deep-purple', Copy: 'teal', Migration: 'orange' }[t] || 'primary'
}
</script>
