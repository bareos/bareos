<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status"       label="Status"       no-caps />
      <q-tab name="messages"     label="Messages"     no-caps />
      <q-tab name="catalog"      label="Catalog Maintenance" no-caps />
      <q-tab name="subscription" label="Subscription" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- STATUS -->
      <q-tab-panel name="status" class="q-pa-none">
        <div class="row q-col-gutter-md">
          <div class="col-12 col-md-8">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header row items-center">
                <span>Director Status</span>
                <q-space />
                <q-btn flat round dense icon="refresh" color="white" @click="refreshStatus" :loading="statusLoading" />
              </q-card-section>
              <q-card-section>
                <q-inner-loading :showing="statusLoading" />
                <div v-if="statusError" class="text-negative">{{ statusError }}</div>
                <div v-else class="console-output">{{ dirStatusText }}</div>
              </q-card-section>
            </q-card>
          </div>
          <div class="col-12 col-md-4">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header">Version</q-card-section>
              <q-card-section>
                <div class="text-h6">Bareos Director</div>
                <div v-if="versionLine" class="text-caption text-grey">{{ versionLine }}</div>
              </q-card-section>
            </q-card>
          </div>
        </div>
      </q-tab-panel>

      <!-- MESSAGES -->
      <q-tab-panel name="messages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Director Messages</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshMessages" :loading="messagesLoading" />
          </q-card-section>
          <q-card-section>
            <q-inner-loading :showing="messagesLoading" />
            <div v-if="messagesError" class="text-negative">{{ messagesError }}</div>
            <div v-else class="console-output">{{ messagesText || '(no messages)' }}</div>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- CATALOG MAINTENANCE -->
      <q-tab-panel name="catalog" class="q-pa-none">
        <div class="row q-col-gutter-md">

          <!-- Empty Jobs -->
          <div class="col-12">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header row items-center">
                <span>Jobs With No Data</span>
                <q-space />
                <q-btn flat round dense icon="refresh" color="white"
                       @click="loadEmptyJobs" :loading="emptyJobsLoading" />
              </q-card-section>
              <q-card-section class="q-pa-none">
                <q-inner-loading :showing="emptyJobsLoading" />
                <div v-if="emptyJobsError" class="text-negative q-pa-md">{{ emptyJobsError }}</div>
                <template v-else>
                  <div class="q-pa-sm row items-center q-gutter-sm">
                    <span class="text-caption text-grey-6">
                      {{ emptyJobs.length }} job{{ emptyJobs.length !== 1 ? 's' : '' }} with
                      0 files and 0 bytes found
                    </span>
                    <q-space />
                    <q-btn
                      v-if="selectedEmptyJobs.length"
                      color="negative" size="sm" no-caps unelevated
                      :label="`Delete ${selectedEmptyJobs.length} selected`"
                      icon="delete"
                      :loading="deleteLoading"
                      @click="deleteSelected"
                    />
                  </div>
                  <q-table
                    :rows="emptyJobs"
                    :columns="emptyJobCols"
                    row-key="id"
                    dense flat
                    selection="multiple"
                    v-model:selected="selectedEmptyJobs"
                    :pagination="{ rowsPerPage: 20 }"
                    no-data-label="No empty jobs found"
                  >
                    <template #body-cell-id="props">
                      <q-td :props="props">
                        <router-link
                          :to="{ name: 'job-details', params: { id: props.value } }"
                          class="text-primary"
                        >{{ props.value }}</router-link>
                      </q-td>
                    </template>
                    <template #body-cell-status="props">
                      <q-td :props="props">
                        <JobStatusBadge :status="props.value" />
                      </q-td>
                    </template>
                  </q-table>
                  <div v-if="deleteResult" class="q-pa-sm">
                    <q-banner
                      :class="deleteResult.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
                      dense rounded
                    >{{ deleteResult.msg }}</q-banner>
                  </div>
                </template>
              </q-card-section>
            </q-card>
          </div>

          <!-- Prune -->
          <div class="col-12 col-md-6">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header">Prune Expired Records</q-card-section>
              <q-card-section>
                <p class="text-body2 q-mb-md">
                  Remove catalog records that have exceeded their retention period.
                  This does not delete any data from storage volumes.
                </p>
                <div class="row q-gutter-sm">
                  <q-btn
                    v-for="prune in pruneActions" :key="prune.cmd"
                    :label="prune.label" :icon="prune.icon"
                    color="warning" text-color="black"
                    size="sm" no-caps unelevated
                    :loading="pruneLoading[prune.cmd]"
                    @click="runPrune(prune.cmd)"
                  />
                </div>
                <div v-if="pruneResults.length" class="q-mt-md column q-gutter-xs">
                  <q-banner
                    v-for="(r, i) in pruneResults" :key="i"
                    :class="r.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
                    dense rounded
                  >{{ r.msg }}</q-banner>
                </div>
              </q-card-section>
            </q-card>
          </div>

        </div>
      </q-tab-panel>

      <!-- SUBSCRIPTION -->
      <q-tab-panel name="subscription" class="q-pa-none">
        <q-card flat bordered class="bareos-panel" style="max-width:800px">
          <q-card-section class="panel-header row items-center">
            <span>Subscription</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white"
                   @click="refreshSubscription" :loading="subscriptionLoading" />
          </q-card-section>
          <q-card-section>
            <q-inner-loading :showing="subscriptionLoading" />
            <div v-if="subscriptionError" class="text-negative">{{ subscriptionError }}</div>
            <template v-else-if="subscriptionData">
              <!-- Meta info -->
              <q-list dense separator class="q-mb-md">
                <q-item>
                  <q-item-section class="text-weight-medium" style="max-width:160px">Version</q-item-section>
                  <q-item-section>{{ subscriptionData.version }}</q-item-section>
                </q-item>
                <q-item>
                  <q-item-section class="text-weight-medium" style="max-width:160px">OS</q-item-section>
                  <q-item-section>{{ subscriptionData.os }}</q-item-section>
                </q-item>
                <q-item>
                  <q-item-section class="text-weight-medium" style="max-width:160px">Binary Info</q-item-section>
                  <q-item-section>{{ subscriptionData['binary-info'] }}</q-item-section>
                </q-item>
                <q-item>
                  <q-item-section class="text-weight-medium" style="max-width:160px">Report Time</q-item-section>
                  <q-item-section>{{ subscriptionData['report-time'] }}</q-item-section>
                </q-item>
              </q-list>

              <!-- Backup unit totals -->
              <div class="text-subtitle2 q-mb-xs">Backup Unit Totals</div>
              <q-list dense separator class="q-mb-md">
                <q-item v-for="(val, key) in subscriptionData['total-units-required']" :key="key">
                  <q-item-section class="text-weight-medium text-capitalize" style="max-width:160px">{{ key }}</q-item-section>
                  <q-item-section>
                    <q-badge :color="key === 'remaining' && Number(val) < 0 ? 'negative' : 'primary'" :label="val" />
                  </q-item-section>
                </q-item>
              </q-list>

              <!-- Per-client backup unit detail -->
              <div class="text-subtitle2 q-mb-xs">Backup Unit Detail</div>
              <q-table
                flat bordered dense
                :rows="subscriptionData['unit-detail']"
                :columns="subscriptionUnitCols"
                row-key="client"
                hide-pagination
                :rows-per-page-options="[0]"
                class="q-mb-md"
              />

              <!-- Uncategorized clients/filesets -->
              <template v-if="subscriptionData['filesets-not-catogorized'] && subscriptionData['filesets-not-catogorized'].length">
                <div class="text-subtitle2 q-mb-xs">Clients/Filesets Not Yet Categorized</div>
                <q-table
                  flat bordered dense
                  :rows="subscriptionData['filesets-not-catogorized']"
                  row-key="client"
                  hide-pagination
                  :rows-per-page-options="[0]"
                  class="q-mb-md"
                />
              </template>

              <!-- Checksum -->
              <div class="text-caption text-grey q-mb-md">
                Checksum: {{ subscriptionData.checksum }}
              </div>

              <div class="row q-gutter-sm q-mb-md">
                <q-btn color="secondary" label="Download Report" icon="download"
                       no-caps :loading="downloadLoading"
                       @click="downloadSubscription(false)" />
                <q-btn color="secondary" label="Download Anonymized Report" icon="download"
                       no-caps :loading="downloadAnonLoading"
                       @click="downloadSubscription(true)" />
              </div>

              <q-btn color="primary" label="Get Official Support" icon="open_in_new"
                     href="https://www.bareos.com/subscription/" target="_blank" no-caps />
            </template>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useDirectorStore } from '../stores/director.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'

const tab = ref('status')
const director = useDirectorStore()

// ── Status ───────────────────────────────────────────────────────────────────
const statusLoading = ref(false)
const statusError   = ref(null)
const rawStatus     = ref(null)

async function refreshStatus() {
  statusLoading.value = true
  statusError.value = null
  try {
    rawStatus.value = await director.call('status director')
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

// The director returns a dict; format it for display.
const dirStatusText = computed(() => {
  const d = rawStatus.value
  if (!d) return ''
  if (typeof d === 'string') return d
  return JSON.stringify(d, null, 2)
})

const versionLine = computed(() => {
  const text = dirStatusText.value
  const m = text.match(/Version:\s+(.+)/)
  return m ? m[1] : ''
})

// ── Messages ─────────────────────────────────────────────────────────────────
const messagesLoading = ref(false)
const messagesError   = ref(null)
const rawMessages     = ref(null)

async function refreshMessages() {
  messagesLoading.value = true
  messagesError.value = null
  try {
    rawMessages.value = await director.call('messages')
  } catch (e) {
    messagesError.value = e.message
  } finally {
    messagesLoading.value = false
  }
}

const messagesText = computed(() => {
  const d = rawMessages.value
  if (!d) return ''
  if (typeof d === 'string') return d
  return JSON.stringify(d, null, 2)
})

refreshStatus()
refreshMessages()
watch(tab, (t) => {
  if (t === 'catalog') loadEmptyJobs()
  if (t === 'subscription') refreshSubscription()
})

// ── Subscription ──────────────────────────────────────────────────────────────
const subscriptionLoading = ref(false)
const subscriptionError   = ref(null)
const subscriptionData    = ref(null)

const subscriptionUnitCols = [
  { name: 'client',       label: 'Client',        field: 'client',       align: 'left' },
  { name: 'fileset',      label: 'Fileset',        field: 'fileset',      align: 'left' },
  { name: 'normal_units', label: 'Normal Units',   field: 'normal_units', align: 'right' },
  { name: 'db_units',     label: 'DB Units',       field: 'db_units',     align: 'right' },
  { name: 'vm_units',     label: 'VM Units',       field: 'vm_units',     align: 'right' },
  { name: 'filer_units',  label: 'Filer Units',    field: 'filer_units',  align: 'right' },
]

async function refreshSubscription() {
  subscriptionLoading.value = true
  subscriptionError.value   = null
  try {
    subscriptionData.value = await director.call('status subscriptions all')
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    subscriptionLoading.value = false
  }
}

const downloadLoading     = ref(false)
const downloadAnonLoading = ref(false)

async function downloadSubscription(anonymize) {
  const loadingRef = anonymize ? downloadAnonLoading : downloadLoading
  loadingRef.value = true
  try {
    const cmd  = anonymize ? 'status subscriptions all anonymize' : 'status subscriptions all'
    const data = await director.call(cmd)
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href     = url
    a.download = anonymize ? 'bareos-subscription-anonymized.json' : 'bareos-subscription.json'
    a.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    loadingRef.value = false
  }
}

// ── Catalog Maintenance ───────────────────────────────────────────────────────
const emptyJobsLoading  = ref(false)
const emptyJobsError    = ref(null)
const emptyJobs         = ref([])
const selectedEmptyJobs = ref([])
const deleteLoading     = ref(false)
const deleteResult      = ref(null)

const emptyJobCols = [
  { name: 'id',        label: 'ID',       field: 'id',
    align: 'right', sortable: true },
  { name: 'name',      label: 'Job Name', field: 'name',
    align: 'left',  sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',
    align: 'left',  sortable: true },
  { name: 'type',      label: 'Type',     field: 'type',
    align: 'center' },
  { name: 'starttime', label: 'Start',    field: 'starttime',
    align: 'left',  sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',
    align: 'center' },
]

async function loadEmptyJobs() {
  emptyJobsLoading.value  = true
  emptyJobsError.value    = null
  deleteResult.value      = null
  selectedEmptyJobs.value = []
  try {
    const res  = await director.call('list jobs')
    const jobs = (res?.jobs ?? []).map(normaliseJob)
    emptyJobs.value = jobs.filter(j =>
      !Number(j.bytes) && !Number(j.files) && j.status !== 'R'
    )
  } catch (e) {
    emptyJobsError.value = e.message
  } finally {
    emptyJobsLoading.value = false
  }
}

async function deleteSelected() {
  if (!selectedEmptyJobs.value.length) return
  deleteLoading.value = true
  deleteResult.value  = null
  const ids = selectedEmptyJobs.value.map(j => j.id).join(',')
  try {
    await director.call(`delete jobid=${ids}`)
    deleteResult.value = { ok: true, msg: `Deleted job(s) ${ids}.` }
    const deleted = new Set(selectedEmptyJobs.value.map(j => j.id))
    emptyJobs.value         = emptyJobs.value.filter(j => !deleted.has(j.id))
    selectedEmptyJobs.value = []
  } catch (e) {
    deleteResult.value = { ok: false, msg: e.message }
  } finally {
    deleteLoading.value = false
  }
}

// ── Prune ─────────────────────────────────────────────────────────────────────
const pruneActions = [
  { cmd: 'prune jobs yes',   label: 'Prune Jobs',
    icon: 'work_off' },
  { cmd: 'prune files yes',  label: 'Prune File Records',
    icon: 'folder_off' },
  { cmd: 'prune stats yes',  label: 'Prune Statistics',
    icon: 'bar_chart_off' },
]
const pruneLoading = ref(
  Object.fromEntries(pruneActions.map(p => [p.cmd, false]))
)
const pruneResults = ref([])

async function runPrune(cmd) {
  pruneLoading.value[cmd] = true
  try {
    await director.call(cmd)
    pruneResults.value.push({ ok: true,
      msg: `✓ "${cmd}" completed.` })
  } catch (e) {
    pruneResults.value.push({ ok: false,
      msg: `✗ "${cmd}": ${e.message}` })
  } finally {
    pruneLoading.value[cmd] = false
  }
}
</script>
