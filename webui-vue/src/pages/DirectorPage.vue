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
        <q-inner-loading :showing="statusLoading" />
        <div v-if="statusError" class="text-negative q-pa-md">{{ statusError }}</div>
        <template v-else-if="rawStatus">
          <div class="row q-col-gutter-md">

            <!-- Director Info – single summary line -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header row items-center">
                  <span>Director Info</span>
                  <q-space />
                  <span class="text-white text-caption q-mr-sm" style="opacity:0.7">↻ {{ statusCountdown }}s</span>
                  <q-btn flat round dense color="white" class="q-mr-xs"
                         :icon="relativeTime ? 'calendar_today' : 'schedule'"
                         :title="relativeTime ? 'Show absolute times' : 'Show relative times'"
                         @click="relativeTime = !relativeTime" />
                  <q-btn flat round dense icon="refresh" color="white"
                         @click="manualRefreshStatus" :loading="statusLoading" />
                </q-card-section>
                <q-card-section class="q-py-sm">
                  <div class="row items-center q-gutter-sm text-body2 flex-wrap">
                    <q-chip v-if="statusHeader.director"
                            dense square color="primary" text-color="white" icon="dns" class="q-ma-none">
                      {{ statusHeader.director }}
                      <q-tooltip>Director: {{ statusHeader.director }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.version"
                            dense square color="blue-7" text-color="white" icon="info" class="q-ma-none">
                      {{ statusHeader.version }}
                      <q-tooltip>Version: {{ statusHeader.version }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.release_date"
                            dense square color="blue-grey-6" text-color="white" icon="event" class="q-ma-none">
                      {{ statusHeader.release_date }}
                      <q-tooltip>Released: {{ statusHeader.release_date }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.binary_info"
                            dense square color="blue-grey-7" text-color="white" icon="build" class="q-ma-none">
                      {{ statusHeader.binary_info }}
                      <q-tooltip>Build: {{ statusHeader.binary_info }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.os"
                            dense square :color="directorOsIcon.color" text-color="white"
                            :icon="directorOsIcon.icon" class="q-ma-none">
                      {{ statusHeader.os }}
                      <q-tooltip>OS: {{ statusHeader.os }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.daemon_started"
                            dense square color="teal-7" text-color="white" icon="schedule" class="q-ma-none">
                      {{ relativeTime ? dirTimeAgo(statusHeader.daemon_started) : statusHeader.daemon_started }}
                      <q-tooltip>Started: {{ statusHeader.daemon_started }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.jobs_run != null"
                            dense square color="purple-7" text-color="white" icon="check" class="q-ma-none">
                      {{ statusHeader.jobs_run }}
                      <q-tooltip>Jobs Run: {{ statusHeader.jobs_run }}</q-tooltip>
                    </q-chip>
                    <q-chip v-if="statusHeader.jobs_running != null"
                            dense square color="orange-7" text-color="white" icon="play_arrow" class="q-ma-none">
                      {{ statusHeader.jobs_running }}
                      <q-tooltip>Running: {{ statusHeader.jobs_running }}</q-tooltip>
                    </q-chip>
                    <span v-if="statusHeader.config_warnings != null"
                          class="row items-center no-wrap q-gutter-x-xs">
                      <q-icon
                        :name="statusHeader.config_warnings ? 'error' : 'check_circle'"
                        :color="statusHeader.config_warnings ? 'negative' : 'positive'"
                        size="20px"
                      >
                        <q-tooltip>Config warnings: {{ statusHeader.config_warnings ? 'Yes' : 'None' }}</q-tooltip>
                      </q-icon>
                      <span :class="statusHeader.config_warnings ? 'text-negative' : 'text-positive'">
                        {{ statusHeader.config_warnings ? 'Configuration warnings' : 'Config OK' }}
                      </span>
                    </span>
                  </div>
                </q-card-section>
              </q-card>
            </div>

            <!-- Scheduled Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">Scheduled Jobs</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!scheduledJobs.length" class="q-pa-md text-grey">
                    No scheduled jobs.
                  </div>
                  <q-table v-else flat dense
                    :rows="scheduledJobs"
                    :columns="scheduledJobCols"
                    row-key="name"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-name="props">
                      <td>
                        <router-link :to="{ name: 'jobs', query: { name: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-scheduled="props">
                      <td>
                        <span :title="relativeTime ? props.value : undefined">
                          {{ relativeTime ? dirTimeAgo(props.value) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-volume="props">
                      <td>
                        <router-link v-if="props.value"
                                     :to="{ name: 'volume-details', params: { name: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                        <span v-else>—</span>
                      </td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

            <!-- Running Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">Running Jobs</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!runningJobs.length" class="q-pa-md text-grey">
                    No jobs running.
                  </div>
                  <q-table v-else flat dense
                    :rows="runningJobs"
                    :columns="runningJobCols"
                    row-key="jobid"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-jobid="props">
                      <td>
                        <router-link :to="{ name: 'job-details', params: { id: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                      </td>
                    </template>
                    <template #body-cell-name="props">
                      <td>
                        <router-link :to="{ name: 'jobs', query: { name: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-start_time="props">
                      <td>
                        <span :title="relativeTime ? props.value : undefined">
                          {{ relativeTime ? dirTimeAgo(props.value) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-files="props">
                      <td class="text-right" style="min-width:80px">
                        <div>{{ (props.value ?? 0).toLocaleString() }}</div>
                        <q-linear-progress v-if="!isWaiting(props.row.status)" indeterminate
                          color="teal" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-bytes="props">
                      <td class="text-right" style="min-width:90px">
                        <div>{{ formatBytes(Number(props.value) || 0) }}</div>
                        <q-linear-progress v-if="!isWaiting(props.row.status)" indeterminate
                          color="primary" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-status="props">
                      <td>
                        <span v-if="isWaiting(props.value)"
                              class="row items-center no-wrap q-gutter-x-xs">
                          <q-icon name="hourglass_empty" color="orange-7" size="16px"
                                  class="animated-spin" />
                          <span class="text-orange-7 text-caption">{{ props.value }}</span>
                        </span>
                        <JobStatusBadge v-else :status="props.value" />
                      </td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

            <!-- Terminated Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">Terminated Jobs</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!terminatedJobs.length" class="q-pa-md text-grey">
                    No terminated jobs.
                  </div>
                  <q-table v-else flat dense
                    :rows="terminatedJobs"
                    :columns="terminatedJobCols"
                    row-key="jobid"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-jobid="props">
                      <td>
                        <router-link :to="{ name: 'job-details', params: { id: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                      </td>
                    </template>
                    <template #body-cell-name="props">
                      <td>
                        <router-link :to="{ name: 'jobs', query: { name: props.value } }"
                                     class="text-primary">
                          {{ props.value }}
                        </router-link>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-finished="props">
                      <td>
                        <span :title="relativeTime ? props.value : undefined">
                          {{ relativeTime ? dirTimeAgo(props.value) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-files="props">
                      <td class="text-right" style="min-width:80px">
                        <div>{{ (props.value ?? 0).toLocaleString() }}</div>
                        <q-linear-progress
                          :value="(Number(props.value) || 0) / maxTermFiles"
                          color="teal" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-bytes="props">
                      <td class="text-right" style="min-width:90px">
                        <div>{{ formatBytes(Number(props.value) || 0) }}</div>
                        <q-linear-progress
                          :value="(Number(props.value) || 0) / maxTermBytes"
                          color="primary" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-status="props">
                      <td><JobStatusBadge :status="props.value" /></td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

          </div>
        </template>
      </q-tab-panel>

      <!-- MESSAGES -->
      <q-tab-panel name="messages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Director Messages</span>
            <q-space />
            <q-select v-model="messagesLimit" :options="[50,100,250,500]" dense outlined dark
                      style="width:80px" class="q-mr-sm" />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshMessages" :loading="messagesLoading" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-inner-loading :showing="messagesLoading" />
            <div v-if="messagesError" class="q-pa-md text-negative">{{ messagesError }}</div>
            <div v-else-if="!logEntries.length && !messagesLoading" class="q-pa-md text-grey">(no messages)</div>
            <div v-else class="terminal-output">
              <template v-for="item in logEntries" :key="item.logid">
                <span class="terminal-line">{{ item.time }} {{ item.logtext }}</span>
              </template>
            </div>
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
              <SubscriptionReport :data="subscriptionData" />

              <!-- Download buttons: 2×2 grid (normal / anonymized) × (PDF / JSON) -->
              <div class="row q-gutter-sm q-mb-md q-mt-md">
                <q-btn color="primary" label="Download PDF" icon="picture_as_pdf"
                       no-caps :loading="pdfLoading"
                       @click="printSubscription(false)" />
                <q-btn color="secondary" label="Download JSON" icon="download"
                       no-caps :loading="downloadLoading"
                       @click="downloadSubscription(false)" />
                <q-btn color="primary" label="Download Anonymized PDF"
                       icon="picture_as_pdf"
                       no-caps :loading="pdfAnonLoading"
                       @click="printSubscription(true)" />
                <q-btn color="secondary" label="Download Anonymized JSON"
                       icon="download"
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

  <!-- Hidden print root — rendered into DOM, visible only during window.print() -->
  <teleport to="body">
    <div id="subscription-print-root" style="display:none">
      <SubscriptionReport v-if="printData" :data="printData" :anonymized="printAnonymized" />
    </div>
  </teleport>
</template>

<script setup>
import { ref, computed, watch, nextTick, onMounted, onUnmounted } from 'vue'
import { useDirectorStore } from '../stores/director.js'
import { normaliseJob } from '../composables/useDirectorFetch.js'
import { formatBytes } from '../mock/index.js'
import { resolveOsIcon } from '../utils/osIcon.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge  from '../components/JobLevelBadge.vue'
import JobTypeBadge   from '../components/JobTypeBadge.vue'
import SubscriptionReport from '../components/SubscriptionReport.vue'

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

const STATUS_INTERVAL = 10
const statusCountdown = ref(STATUS_INTERVAL)
let _statusTimer = null

function startStatusAutoRefresh() {
  clearInterval(_statusTimer)
  statusCountdown.value = STATUS_INTERVAL
  _statusTimer = setInterval(() => {
    statusCountdown.value -= 1
    if (statusCountdown.value <= 0) {
      refreshStatus()
      statusCountdown.value = STATUS_INTERVAL
    }
  }, 1000)
}

function manualRefreshStatus() {
  refreshStatus()
  statusCountdown.value = STATUS_INTERVAL
}

// The director now returns a structured JSON object with header, scheduled,
// running and terminated arrays.  Fall back gracefully for older directors.
const statusHeader = computed(() => {
  const d = rawStatus.value
  if (!d || typeof d !== 'object') return {}
  return d.header || {}
})
const scheduledJobs  = computed(() => rawStatus.value?.scheduled  ?? [])
const runningJobs    = computed(() => rawStatus.value?.running    ?? [])
const terminatedJobs = computed(() => rawStatus.value?.terminated ?? [])

const maxTermBytes = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.bytes) || 0)))
const maxTermFiles = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.files) || 0)))

// Relative-time toggle
const relativeTime = ref(false)

// OS icon: pass the full os string as osInfo so distro detection works
// (e.g. "Fedora Linux 43" → mdi-fedora)
const directorOsIcon = computed(() => {
  const full = statusHeader.value.os ?? ''
  const lower = full.toLowerCase()
  if (lower.includes('windows') || lower.includes('win'))
    return resolveOsIcon({ os: 'windows', osInfo: full })
  if (lower.includes('darwin') || lower.includes('apple') || lower.includes('macos'))
    return resolveOsIcon({ os: 'macos', osInfo: full })
  return resolveOsIcon({ os: 'linux', osInfo: full })
})

// Map full-word level/type strings emitted by the director to single-letter
// codes expected by JobLevelBadge / JobTypeBadge.
// The director truncates levels differently per section:
//   scheduled: full string ("Incremental", "Differential")
//   running:   level[7]=0  ("Incremen", "Differen")
//   terminated: level[4]=0 ("Incr", "Diff", "VFul")
const LEVEL_CODE = {
  Full: 'F', Incremental: 'I', Differential: 'D', 'Virtual Full': 'V', Base: 'B',
  Incremen: 'I', Differen: 'D',
  Incr: 'I', Diff: 'D', VFul: 'V',
}
const TYPE_CODE  = { Backup: 'B', Restore: 'R', Verify: 'V', Admin: 'A', Diagnostic: 'D', Copy: 'C', Migration: 'M' }
function levelCode(v) { return LEVEL_CODE[v] ?? null }
function typeCode(v)  { return TYPE_CODE[v]  ?? null }
function isWaiting(status) { return typeof status === 'string' && status.includes('is waiting') }

// Parse the director's "DD-Mon-YY HH:MM" date format into a JS Date.
const _MON = { Jan:0, Feb:1, Mar:2, Apr:3, May:4, Jun:5, Jul:6, Aug:7, Sep:8, Oct:9, Nov:10, Dec:11 }
function parseDirectorDate(str) {
  if (!str) return null
  const m = str.match(/^(\d{1,2})-([A-Za-z]{3})-(\d{2})\s+(\d{2}):(\d{2})/)
  if (!m) return null
  const month = _MON[m[2]]
  if (month === undefined) return null
  return new Date(2000 + parseInt(m[3]), month, parseInt(m[1]), parseInt(m[4]), parseInt(m[5]))
}

// Relative time for past and future Bareos director timestamps.
function dirTimeAgo(str) {
  if (!str) return '—'
  const d = parseDirectorDate(str)
  if (!d) return str
  const ms = Date.now() - d.getTime()
  if (ms < 0) {
    // Future (scheduled jobs)
    const abs = -ms
    const m = Math.floor(abs / 60000)
    if (m < 60) return `in ${m}m`
    const h = Math.floor(m / 60)
    if (h < 24) return `in ${h}h`
    return `in ${Math.floor(h / 24)}d`
  }
  const s = Math.floor(ms / 1000)
  if (s < 60)  return `${s}s ago`
  const m = Math.floor(s / 60)
  if (m < 60)  return `${m}m ago`
  const h = Math.floor(m / 60)
  if (h < 24)  return `${h}h ago`
  const dy = Math.floor(h / 24)
  if (dy < 30) return `${dy}d ago`
  const mo = Math.floor(dy / 30)
  if (mo < 12) return `${mo}mo ago`
  return `${Math.floor(mo / 12)}y ago`
}

const scheduledJobCols = [
  { name: 'name',      label: 'Job',       field: 'name',      align: 'left' },
  { name: 'level',     label: 'Level',     field: 'level',     align: 'left' },
  { name: 'type',      label: 'Type',      field: 'type',      align: 'left' },
  { name: 'priority',  label: 'Pri',       field: 'priority',  align: 'right' },
  { name: 'scheduled', label: 'Scheduled', field: 'scheduled', align: 'left' },
  { name: 'volume',    label: 'Volume',    field: 'volume',    align: 'left' },
  { name: 'pool',      label: 'Pool',      field: 'pool',      align: 'left' },
  { name: 'storage',   label: 'Storage',   field: 'storage',   align: 'left' },
]
const runningJobCols = [
  { name: 'jobid',      label: 'JobId',      field: 'jobid',      align: 'right' },
  { name: 'name',       label: 'Job',        field: 'name',       align: 'left' },
  { name: 'level',      label: 'Level',      field: 'level',      align: 'left' },
  { name: 'type',       label: 'Type',       field: 'type',       align: 'left' },
  { name: 'start_time', label: 'Started',    field: 'start_time', align: 'left' },
  { name: 'files',      label: 'Files',      field: 'files',      align: 'right' },
  { name: 'bytes',      label: 'Bytes',      field: 'bytes',      align: 'right' },
  { name: 'status',     label: 'Status',     field: 'status',     align: 'left' },
]
const terminatedJobCols = [
  { name: 'jobid',    label: 'JobId',    field: 'jobid',    align: 'right' },
  { name: 'name',     label: 'Job',      field: 'name',     align: 'left' },
  { name: 'level',    label: 'Level',    field: 'level',    align: 'left' },
  { name: 'type',     label: 'Type',     field: 'type',     align: 'left' },
  { name: 'finished', label: 'Finished', field: 'finished', align: 'left' },
  { name: 'files',    label: 'Files',    field: 'files',    align: 'right' },
  { name: 'bytes',    label: 'Bytes',    field: 'bytes',    align: 'right' },
  { name: 'status',   label: 'Status',   field: 'status',   align: 'left' },
]

// ── Messages ─────────────────────────────────────────────────────────────────
const messagesLoading = ref(false)
const messagesError   = ref(null)
const messagesLimit   = ref(100)
const logEntries      = ref([])

async function refreshMessages() {
  messagesLoading.value = true
  messagesError.value = null
  try {
    const data = await director.call(`list log limit=${messagesLimit.value}`)
    logEntries.value = data?.log ?? []
  } catch (e) {
    messagesError.value = e.message
  } finally {
    messagesLoading.value = false
  }
}

watch(messagesLimit, () => refreshMessages())

refreshStatus()
refreshMessages()
watch(tab, (t) => {
  if (t === 'messages') refreshMessages()
  if (t === 'catalog') loadEmptyJobs()
  if (t === 'subscription') refreshSubscription()
})

onMounted(() => { startStatusAutoRefresh() })
onUnmounted(() => { clearInterval(_statusTimer) })

// ── Subscription ──────────────────────────────────────────────────────────────
const subscriptionLoading = ref(false)
const subscriptionError   = ref(null)
const subscriptionData    = ref(null)

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
const pdfLoading          = ref(false)
const pdfAnonLoading      = ref(false)

// Print state: when set, the hidden #subscription-print-root becomes populated
// and window.print() renders it to PDF via @media print CSS.
const printData       = ref(null)
const printAnonymized = ref(false)

async function printSubscription(anonymize) {
  const loadingRef = anonymize ? pdfAnonLoading : pdfLoading
  loadingRef.value = true
  try {
    const cmd = anonymize
      ? 'status subscriptions all anonymize'
      : 'status subscriptions all'
    printData.value       = await director.call(cmd)
    printAnonymized.value = anonymize
    await nextTick()
    window.print()
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    printData.value  = null
    loadingRef.value = false
  }
}

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

<style scoped>
.terminal-output {
  background: #0d1117;
  color: #c9d1d9;
  font-family: 'Fira Mono', 'Cascadia Code', 'Consolas', 'DejaVu Sans Mono', monospace;
  font-size: 0.8rem;
  line-height: 1.5;
  padding: 12px 16px;
  max-height: 600px;
  overflow-y: auto;
  white-space: pre-wrap;
  word-break: break-all;
  display: flex;
  flex-direction: column;
}
.terminal-line {
  display: block;
  border-bottom: 1px solid #21262d;
  padding: 1px 0;
}
.terminal-line:last-child {
  border-bottom: none;
}
.animated-spin {
  animation: hourglass-spin 1.4s ease-in-out infinite;
}
@keyframes hourglass-spin {
  0%   { transform: rotate(0deg);   }
  45%  { transform: rotate(0deg);   }
  55%  { transform: rotate(180deg); }
  100% { transform: rotate(180deg); }
}
</style>
