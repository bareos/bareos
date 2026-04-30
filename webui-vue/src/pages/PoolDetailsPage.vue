<template>
  <q-page class="q-pa-md">
    <!-- Back -->
    <q-btn flat no-caps icon="arrow_back" :label="t('Pools')" class="q-mb-md"
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
            {{ pool.enabled !== '0' ? t('Enabled') : t('Disabled') }}
          </q-badge>
        </div>
      </div>

      <div class="row q-col-gutter-md">
        <!-- Details card -->
        <div class="col-12 col-md-5">
          <q-card flat bordered class="bareos-panel">
            <q-card-section class="panel-header">{{ t('Details') }}</q-card-section>
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
              <span>{{ t('Volumes') }} ({{ volumes.length }})</span>
            </q-card-section>
            <q-card-section class="q-pa-none">
              <q-table :rows="volumes" :columns="volumeCols" row-key="volumename"
                       dense flat :pagination="{ rowsPerPage: 10 }">
                <template #body-cell-volumename="props">
                  <q-td :props="props">
                    <VolumeNameLink :name="props.value" :volume="props.row" />
                  </q-td>
                </template>
                <template #body-cell-volstatus="props">
                  <q-td :props="props">
                    <q-badge :color="statusColor(props.value)" :label="props.value" />
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
                      :value="volMaxBytesGauge(props.value)"
                      color="teal" track-color="grey-3"
                      size="4px" class="q-mt-xs" rounded
                    />
                  </q-td>
                </template>
                <template #body-cell-volretention="props">
                  <q-td :props="props" style="min-width:90px">
                    <div>{{ formatDuration(props.value) }}</div>
                    <q-linear-progress
                      :value="retentionGauge(props.value)"
                      color="orange" track-color="grey-3"
                      size="4px" class="q-mt-xs" rounded
                    />
                  </q-td>
                </template>
                <template #body-cell-inchanger="props">
                  <q-td :props="props" class="text-center">
                    <q-icon :name="props.value ? 'check' : 'remove'"
                            :color="props.value ? 'positive' : 'grey'" size="xs" />
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
import { useI18n } from 'vue-i18n'
import { normaliseVolume } from '../composables/useDirectorFetch.js'
import VolumeNameLink from '../components/VolumeNameLink.vue'
import { useDirectorStore } from '../stores/director.js'
import { formatBytes, formatDuration } from '../mock/index.js'

const route     = useRoute()
const director  = useDirectorStore()
const poolName  = route.params.name
const { t } = useI18n()

const pool    = ref(null)
const volumes = ref([])
const loading = ref(true)
const error   = ref(null)

onMounted(async () => {
  try {
    const [poolRes, volRes] = await Promise.all([
      director.call(`llist pool=${poolName}`),
      director.call(`llist volumes pool=${poolName}`),
    ])
    const pools = poolRes?.pools ?? []
    pool.value    = Array.isArray(pools) ? pools[0] : Object.values(pools)[0]
    const vols    = volRes?.volumes ?? []
    const volumeRows = Array.isArray(vols) ? vols : Object.values(vols).flat()
    volumes.value = volumeRows.map((volume) => {
      const normalized = normaliseVolume(volume)
      return {
        ...volume,
        ...normalized,
        volretention: volume.volretention ?? volume.VolRetention ?? normalized.retention,
      }
    })
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
    { label: t('Pool ID'),           value: p.poolid },
    { label: t('Label Format'),      value: p.labelformat || '—' },
    { label: t('Volumes'),           value: `${p.numvols ?? 0} / ${p.maxvols ?? '∞'}` },
    { label: t('Vol Retention'),     value: formatDuration(p.volretention) },
    { label: t('Vol Use Duration'),  value: p.voluseduration > 0 ? formatDuration(p.voluseduration) : '—' },
    { label: t('Max Jobs/Vol'),      value: p.maxvoljobs > 0 ? p.maxvoljobs : '∞' },
    { label: t('Max Bytes/Vol'),     value: p.maxvolbytes > 0 ? formatBytes(p.maxvolbytes) : '∞' },
    { label: t('Auto Prune'),        value: p.autoprune === '1' ? t('Yes') : t('No') },
    { label: t('Recycle'),           value: p.recycle === '1' ? t('Yes') : t('No') },
    { label: t('Use Once'),          value: p.useonce === '1' ? t('Yes') : t('No') },
    { label: t('Accept Any Volume'), value: p.acceptanyvolume === '1' ? t('Yes') : t('No') },
    { label: t('Scratch Pool ID'),   value: p.scratchpoolid > 0 ? p.scratchpoolid : '—' },
    { label: t('Recycle Pool ID'),   value: p.recyclepoolid > 0 ? p.recyclepoolid : '—' },
  ]
})

const volumeCols = computed(() => [
  { name: 'volumename',  label: t('Volume'),       field: 'volumename',  align: 'left',   sortable: true },
  { name: 'volstatus',   label: t('Status'),       field: 'volstatus',   align: 'center', sortable: true },
  { name: 'volbytes',    label: t('Used'),         field: 'volbytes',    align: 'right',  sortable: true },
  { name: 'maxvolbytes', label: t('Max Bytes'),    field: 'maxvolbytes', align: 'right',  sortable: true },
  { name: 'volretention',label: t('Retention'),    field: 'volretention',align: 'left',   sortable: true },
  { name: 'lastwritten', label: t('Last Written'), field: 'lastwritten', align: 'left',   sortable: true },
  { name: 'inchanger',   label: t('In Changer'),   field: 'inchanger',   align: 'center' },
  { name: 'storage',     label: t('Storage'),      field: 'storage',     align: 'left'   },
])

const maxVolBytes = computed(() =>
  Math.max(1, ...volumes.value.map(v => Number(v.volbytes) || 0))
)
const maxVolMaxBytes = computed(() =>
  Math.max(1, ...volumes.value
    .filter(v => Number(v.maxvolbytes) > 0)
    .map(v => Number(v.maxvolbytes)))
)
const maxVolRetention = computed(() =>
  Math.max(1, ...volumes.value.map(v => Number(v.volretention) || 0))
)
function volBytesGauge(val)    { return (Number(val) || 0) / maxVolBytes.value }
function volMaxBytesGauge(val) { return (Number(val) || 0) / maxVolMaxBytes.value }
function retentionGauge(val)   { return (Number(val) || 0) / maxVolRetention.value }

function statusColor(s) {
  return { Full: 'warning', Append: 'positive', Recycled: 'grey', Error: 'negative', Purged: 'grey', Used: 'info' }[s] || 'info'
}

function poolTypeColor(t) {
  return { Backup: 'primary', Scratch: 'grey', Archive: 'deep-purple', Copy: 'teal', Migration: 'orange' }[t] || 'primary'
}
</script>
