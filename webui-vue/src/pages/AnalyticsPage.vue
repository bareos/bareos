<template>
  <q-page class="q-pa-md">

    <!-- ── Header row ──────────────────────────────────────────────────────── -->
    <div class="row q-col-gutter-md q-mb-md">

      <!-- Overall totals -->
      <div class="col-6 col-sm-3 col-md-2" v-for="s in overallStats" :key="s.label">
        <q-card flat bordered class="bareos-panel text-center">
          <q-card-section class="q-py-sm">
            <div class="text-caption text-grey-6">{{ s.label }}</div>
            <div class="text-h6 text-weight-bold" :class="'text-' + s.color">{{ s.value }}</div>
          </q-card-section>
        </q-card>
      </div>

    </div>

    <div class="row q-col-gutter-md">

      <!-- ── Left column ─────────────────────────────────────────────────── -->
      <div class="col-12 col-md-8">

        <!-- Treemap: stored bytes per job name -->
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header row items-center">
            <span>Stored Data per Job (treemap)</span>
            <q-space />
            <q-btn-toggle v-model="treemapMode" flat no-caps dense
              :options="[{label:'Bytes',value:'bytes'},{label:'Files',value:'files'}]" />
          </q-card-section>
          <q-card-section class="q-pa-sm">
            <div ref="treemapEl" style="position:relative;width:100%;height:280px;overflow:hidden">
              <div v-for="tile in treemapTiles" :key="tile.name"
                   :style="tile.style"
                   style="position:absolute;overflow:hidden;box-sizing:border-box;border:2px solid white;border-radius:4px;cursor:default;transition:opacity .2s"
                   :title="`${tile.name}\n${fmtBytes(tile.bytes)} · ${tile.files.toLocaleString()} files`">
                <div style="padding:4px 6px;height:100%;display:flex;flex-direction:column;justify-content:center">
                  <div class="text-white text-weight-bold" style="font-size:11px;line-height:1.2;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">
                    {{ tile.name }}
                  </div>
                  <div v-if="tile.h > 36" class="text-white" style="font-size:10px;opacity:.85">
                    {{ treemapMode === 'bytes' ? fmtBytes(tile.bytes) : tile.files.toLocaleString() + ' files' }}
                  </div>
                </div>
              </div>
              <div v-if="!treemapTiles.length" class="flex flex-center text-grey" style="height:100%">
                <span>No data</span>
              </div>
            </div>
          </q-card-section>
        </q-card>

        <!-- Job status breakdown table -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Job Status Breakdown</q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="statusRows" :columns="statusCols" row-key="label"
                     dense flat hide-bottom :pagination="{rowsPerPage:0}">
              <template #body-cell-bar="props">
                <q-td :props="props" style="width:200px">
                  <q-linear-progress :value="props.row.count / totalJobs || 0"
                    :color="props.row.color" track-color="grey-2" size="12px" rounded />
                </q-td>
              </template>
              <template #body-cell-label="props">
                <q-td :props="props">
                  <q-badge :color="props.row.color" :label="props.row.label" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

      </div>

      <!-- ── Right column ────────────────────────────────────────────────── -->
      <div class="col-12 col-md-4">

        <!-- Per-client bytes bar chart -->
        <q-card flat bordered class="bareos-panel q-mb-md">
          <q-card-section class="panel-header">Bytes per Client</q-card-section>
          <q-card-section class="q-pa-sm q-gutter-xs">
            <div v-for="c in clientBytes" :key="c.name" class="q-mb-xs">
              <div class="row items-center q-mb-xs" style="gap:4px">
                <span class="text-caption ellipsis" style="width:110px;min-width:0" :title="c.name">{{ c.name }}</span>
                <q-linear-progress :value="bytesGauge(c.bytes)" color="primary" track-color="grey-3"
                                   size="10px" rounded style="flex:1" />
                <span class="text-caption text-grey-6" style="width:60px;text-align:right">{{ fmtBytes(c.bytes) }}</span>
              </div>
            </div>
            <div v-if="!clientBytes.length" class="text-grey text-caption text-center q-py-md">No data</div>
          </q-card-section>
        </q-card>

        <!-- Job level distribution -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Job Level Distribution</q-card-section>
          <q-card-section class="q-pa-sm">
            <div v-for="l in levelDist" :key="l.label" class="q-mb-sm">
              <div class="row items-center q-mb-xs" style="gap:6px">
                <span class="text-caption" style="width:90px">{{ l.label }}</span>
                <q-linear-progress :value="l.count / totalJobs || 0"
                  :color="l.color" track-color="grey-3" size="10px" rounded style="flex:1" />
                <span class="text-caption text-grey-6" style="width:30px;text-align:right">{{ l.count }}</span>
              </div>
            </div>
          </q-card-section>
        </q-card>

      </div>
    </div>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { formatBytes } from '../mock/index.js'
import { useDirectorFetch, normaliseJob } from '../composables/useDirectorFetch.js'

const fmtBytes = formatBytes
const treemapMode = ref('bytes')
const treemapEl   = ref(null)
const treemapW    = ref(600)
const treemapH    = ref(280)

// ── data ─────────────────────────────────────────────────────────────────────
const { data: rawJobs }    = useDirectorFetch('list jobs', 'jobs')
const { data: rawClients } = useDirectorFetch('list clients', 'clients')

const jobs = computed(() => (rawJobs.value ?? []).map(normaliseJob))
const totalJobs = computed(() => jobs.value.length || 1)

// ── overall stats ─────────────────────────────────────────────────────────────
const overallStats = computed(() => {
  const j = jobs.value
  return [
    { label: 'Total Jobs',  value: j.length,                                                  color: 'primary'  },
    { label: 'Successful',  value: j.filter(x => x.status === 'T').length,                    color: 'positive' },
    { label: 'Warning',     value: j.filter(x => x.status === 'W').length,                    color: 'warning'  },
    { label: 'Failed',      value: j.filter(x => x.status === 'f' || x.status === 'E').length,color: 'negative' },
    { label: 'Total Bytes', value: fmtBytes(j.reduce((a, x) => a + x.bytes, 0)),              color: 'blue-7'   },
    { label: 'Total Files', value: j.reduce((a, x) => a + x.files, 0).toLocaleString(),       color: 'teal-7'   },
  ]
})

// ── status breakdown ──────────────────────────────────────────────────────────
const statusRows = computed(() => {
  const j = jobs.value
  const count = (code) => j.filter(x => x.status === code).length
  return [
    { label: 'Successful', color: 'positive', count: count('T') },
    { label: 'Warning',    color: 'warning',  count: count('W') },
    { label: 'Failed',     color: 'negative', count: count('f') + count('E') },
    { label: 'Canceled',   color: 'grey',     count: count('A') },
    { label: 'Running',    color: 'info',     count: count('R') },
  ]
})
const statusCols = [
  { name: 'label', label: 'Status', field: 'label', align: 'left',  style: 'width:100px' },
  { name: 'bar',   label: '',       field: 'bar',   align: 'left'  },
  { name: 'count', label: '#',      field: 'count', align: 'right', style: 'width:50px'  },
]

// ── per-client bytes ──────────────────────────────────────────────────────────
const clientBytes = computed(() => {
  const map = {}
  for (const j of jobs.value) {
    if (!j.client) continue
    map[j.client] = (map[j.client] ?? 0) + j.bytes
  }
  return Object.entries(map)
    .map(([name, bytes]) => ({ name, bytes }))
    .sort((a, b) => b.bytes - a.bytes)
    .slice(0, 12)
})

const maxBytesLog = computed(() =>
  Math.log(Math.max(1, ...clientBytes.value.map(c => c.bytes)) + 1)
)
function bytesGauge(val) { return Math.log(val + 1) / maxBytesLog.value }

// ── level distribution ────────────────────────────────────────────────────────
const levelDist = computed(() => {
  const j = jobs.value
  const count = (code) => j.filter(x => x.level === code).length
  return [
    { label: 'Full',          color: 'primary',  count: count('F') },
    { label: 'Incremental',   color: 'teal',     count: count('I') },
    { label: 'Differential',  color: 'purple',   count: count('D') },
  ]
})

// ── treemap ───────────────────────────────────────────────────────────────────
// Group jobs by name, sum bytes and files
const jobGroups = computed(() => {
  const map = {}
  for (const j of jobs.value) {
    if (!map[j.name]) map[j.name] = { name: j.name, bytes: 0, files: 0 }
    map[j.name].bytes += j.bytes
    map[j.name].files += j.files
  }
  return Object.values(map).sort((a, b) => b.bytes - a.bytes)
})

// Squarified treemap layout (simple slice-and-dice for brevity)
const PALETTE = ['#1976D2','#388E3C','#F57C00','#7B1FA2','#C62828',
                 '#00838F','#558B2F','#6D4C41','#455A64','#E91E63',
                 '#0277BD','#2E7D32','#EF6C00','#6A1B9A','#AD1457']

function sliceDice(items, x0, y0, x1, y1, vertical) {
  const total = items.reduce((s, i) => s + i.value, 0)
  if (!total) return []
  const tiles = []
  let pos = vertical ? y0 : x0
  for (const item of items) {
    const frac = item.value / total
    const end = vertical ? y0 + (y1 - y0) * (tiles.reduce((s, t) => s + t._frac, 0) + frac)
                         : x0 + (x1 - x0) * (tiles.reduce((s, t) => s + t._frac, 0) + frac)
    const tile = vertical
      ? { ...item, x: x0, y: pos, w: x1 - x0, h: end - pos, _frac: frac }
      : { ...item, x: pos, y: y0, w: end - pos, h: y1 - y0, _frac: frac }
    tiles.push(tile)
    pos = end
  }
  return tiles
}

// Simple squarified: split into two halves recursively
function squarify(items, x0, y0, x1, y1, depth = 0) {
  if (!items.length) return []
  if (items.length === 1) {
    return [{ ...items[0], x: x0, y: y0, w: x1 - x0, h: y1 - y0 }]
  }
  const W = x1 - x0, H = y1 - y0
  const total = items.reduce((s, i) => s + i.value, 0)
  // Find split point that best squares the first partition
  let bestAspect = Infinity, bestSplit = 1
  let cumFrac = 0
  for (let i = 0; i < items.length - 1; i++) {
    cumFrac += items[i].value / total
    const aW = W > H ? cumFrac * W : W
    const aH = W > H ? H : cumFrac * H
    const bW = W > H ? (1 - cumFrac) * W : W
    const bH = W > H ? H : (1 - cumFrac) * H
    // avg aspect of smallest tile in each partition
    const a1 = items[i].value / total
    const tileW = W > H ? aW : aW * (items[i].value / (cumFrac * total))
    const tileH = W > H ? aH * (items[i].value / (cumFrac * total)) : aH
    const aspect = Math.max(tileW / Math.max(tileH, 0.001), Math.max(tileH, 0.001) / tileW)
    if (aspect < bestAspect) { bestAspect = aspect; bestSplit = i + 1 }
  }
  const leftItems  = items.slice(0, bestSplit)
  const rightItems = items.slice(bestSplit)
  const leftFrac   = leftItems.reduce((s, i) => s + i.value, 0) / total
  if (W >= H) {
    return [
      ...squarify(leftItems,  x0,              y0, x0 + W * leftFrac,  y1, depth + 1),
      ...squarify(rightItems, x0 + W * leftFrac, y0, x1,               y1, depth + 1),
    ]
  } else {
    return [
      ...squarify(leftItems,  x0, y0,              x1, y0 + H * leftFrac,  depth + 1),
      ...squarify(rightItems, x0, y0 + H * leftFrac, x1, y1,               depth + 1),
    ]
  }
}

const treemapTiles = computed(() => {
  const W = treemapW.value, H = treemapH.value
  if (!W || !H) return []
  const groups = jobGroups.value.filter(g => g[treemapMode.value] > 0)
  if (!groups.length) return []
  const items = groups.map((g, i) => ({
    ...g,
    value: g[treemapMode.value],
    color: PALETTE[i % PALETTE.length],
  }))
  const tiles = squarify(items, 0, 0, W, H)
  return tiles.map(t => ({
    name:  t.name,
    bytes: t.bytes,
    files: t.files,
    h:     t.h,
    style: {
      left:            t.x + 'px',
      top:             t.y + 'px',
      width:           t.w + 'px',
      height:          t.h + 'px',
      backgroundColor: t.color,
    },
  }))
})

// Observe container width for responsive treemap
onMounted(() => {
  if (!treemapEl.value) return
  const ro = new ResizeObserver(entries => {
    for (const e of entries) {
      treemapW.value = e.contentRect.width
      treemapH.value = e.contentRect.height
    }
  })
  ro.observe(treemapEl.value)
})
</script>

