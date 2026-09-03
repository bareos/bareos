<!--
  BAREOS® - Backup Archiving REcovery Open Sourced

  Copyright (C) 2026 Bareos GmbH & Co. KG

  This program is Free Software; you can redistribute it and/or
  modify it under the terms of version three of the GNU Affero General Public
  License as published by the Free Software Foundation and included
  in the file LICENSE.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
-->
<template>
  <div class="trouble-view-widget column no-wrap" style="height:100%; width:100%">
    <div class="row items-center justify-end q-pa-xs">
      <q-btn
        dense flat round
        icon="refresh"
        :loading="loading"
        :title="t('Refresh')"
        @click="loadTroubleLines"
      />
    </div>
    <q-banner v-if="error" dense class="bg-negative text-white">
      {{ error }}
    </q-banner>
    <q-banner v-if="truncated" dense class="bg-warning text-white">
      {{ t('Showing logs for the first {limit} of {total} troubled jobs from the last 24 hours.', { limit: MAX_JOBS, total: totalTroubleJobs }) }}
    </q-banner>
    <q-scroll-area style="flex:1 1 auto">
      <div v-if="loading && !lines.length" class="text-center text-grey q-pa-md">
        {{ t('Loading…') }}
      </div>
      <div v-else-if="!lines.length" class="text-center text-grey q-pa-md">
        <q-icon name="mdi-check-circle-outline" color="positive" size="24px" class="q-mb-xs" />
        <div>{{ t('No errors or warnings in the last 24 hours.') }}</div>
      </div>
      <q-list v-else dense separator>
        <q-item
          v-for="(line, idx) in lines"
          :key="idx"
          clickable
          @click="openLine(line)"
        >
          <q-item-section avatar top>
            <q-icon
              :name="line.type === 'error' ? 'mdi-alert-circle' : 'mdi-alert'"
              :color="line.type === 'error' ? 'negative' : 'warning'"
              size="20px"
            />
          </q-item-section>
          <q-item-section>
            <q-item-label class="text-caption text-grey">
              {{ line.jobName }} (#{{ line.jobId }}) · {{ line.director }}
              <span v-if="line.time">
                · <span :title="settings.relativeTime ? line.time : timeAgo(line.time, settings.locale)">
                  {{ settings.relativeTime ? timeAgo(line.time, settings.locale) : line.time }}
                </span>
              </span>
            </q-item-label>
            <q-item-label class="log-line-text">{{ line.logtext }}</q-item-label>
          </q-item-section>
        </q-item>
      </q-list>
    </q-scroll-area>
  </div>
</template>

<script setup>
import { inject, ref, watch, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { useSettingsStore } from '../../stores/settings.js'
import { useAuthStore } from '../../stores/auth.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { createDirectorCommandClient } from '../../composables/directorAggregate.js'
import { fetchAggregatedTroubleJobs } from '../../composables/jobsAggregate.js'
import { buildJobDetailsQuery, buildListTroubleLogCommand, classifyLogLine } from '../../utils/jobs.js'
import { timeAgo } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'

// Safety cap on the number of jobs whose logs are fetched, to avoid an
// unbounded number of `list joblog` calls if an unusually large number of
// jobs ran in the last 24 hours. The number of *lines* shown is not capped.
const MAX_JOBS = 200

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()
const auth = useAuthStore()
const settings = useSettingsStore()

const ctx = inject(DASHBOARD_CONTEXT_KEY)

const loading = ref(false)
const lines = ref([])
const error = ref('')
const truncated = ref(false)
const totalTroubleJobs = ref(0)

let isUnmounted = false
onUnmounted(() => { isUnmounted = true })

// Guards against overlapping/out-of-order fetch runs (e.g. the aggregate
// firing the watcher again before a previous run finished): only the
// results of the most recently started run are applied.
let latestRequestId = 0

async function loadTroubleLines() {
  const requestId = ++latestRequestId
  const credentials = auth.getCredentials()
  const directors = ctx.activeDirectors.value

  if (!credentials || directors.length === 0) {
    if (requestId === latestRequestId && !isUnmounted) {
      lines.value = []
      error.value = ''
      truncated.value = false
      totalTroubleJobs.value = 0
    }
    return
  }

  if (requestId === latestRequestId && !isUnmounted) loading.value = true

  const troubleJobs = await fetchAggregatedTroubleJobs(credentials, directors, {
    days: 1,
    limit: MAX_JOBS,
  })
  if (requestId !== latestRequestId || isUnmounted) return

  const jobs = troubleJobs.jobs
  totalTroubleJobs.value = troubleJobs.totalJobs
  truncated.value = troubleJobs.truncated
  error.value = troubleJobs.directorErrors.map(entry => entry.message).join(' ')

  if (!jobs.length) {
    if (requestId === latestRequestId && !isUnmounted) {
      lines.value = []
      loading.value = false
    }
    return
  }

  // Filled in as each job's log resolves, indexed by position in `jobs` so
  // the final result stays in the widget's jobid-descending order regardless
  // of which job's request happens to complete first.
  const buckets = new Array(jobs.length).fill(null)
  const indexByJobId = new Map(jobs.map((job, index) => [String(job.id ?? job.jobid), index]))
  const jobById = new Map(jobs.map(job => [String(job.id ?? job.jobid), job]))

  // Group jobs by director so a single batched `list joblog jobids=...` call
  // per director can replace one `list joblog jobid=` call per job.
  const jobIdsByDirector = new Map()
  for (const job of jobs) {
    if (!jobIdsByDirector.has(job.director)) jobIdsByDirector.set(job.director, [])
    jobIdsByDirector.get(job.director).push(job.id ?? job.jobid)
  }

  // Fetch job logs using dedicated, short-lived director connections
  // (rather than the shared director store used for navigation), so this
  // widget's per-job director switching cannot race with, or disrupt, the
  // console/other widgets' use of the single shared director connection.
  const clientsByDirector = new Map()
  async function getClient(directorName) {
    if (!clientsByDirector.has(directorName)) {
      clientsByDirector.set(
        directorName,
        createDirectorCommandClient(auth.getCredentials(directorName)),
      )
    }
    return clientsByDirector.get(directorName)
  }

  try {
    // One batched request per director instead of one request per job: the
    // director now fans the request out to every requested JobId itself in
    // a single SQL query (`list joblog jobids=...`), rather than the widget
    // issuing (and the director serializing) up to MAX_JOBS separate
    // `list joblog jobid=` round-trips -- which previously took 15-20+
    // seconds once the catalog grew large, since a single director
    // connection processes console commands strictly serially regardless
    // of how "concurrently" the client fires them.
    await Promise.allSettled([...jobIdsByDirector.entries()].map(async ([director, jobIds]) => {
      const command = buildListTroubleLogCommand(jobIds)
      if (!command) return
      try {
        const client = await getClient(director)
        const res = await client.call(command)
        const rows = Array.isArray(res?.joblog) ? res.joblog : []
        for (const row of rows) {
          const jobId = String(row.jobid ?? '')
          const index = indexByJobId.get(jobId)
          const job = jobById.get(jobId)
          if (index === undefined || !job) continue
          const time = (row.time ?? '').trim()
          const logtext = (row.logtext ?? '').trimEnd()
          const type = classifyLogLine(`${time} ${logtext}`.trim())
          if (type === 'error' || type === 'warning') {
            if (!buckets[index]) buckets[index] = []
            buckets[index].push({
              time,
              logtext,
              type,
              jobId: job.id ?? job.jobid,
              jobName: job.name,
              director: job.director,
            })
          }
        }
      } catch {
        // Skip the director's jobs whose logs could not be fetched (e.g.
        // director offline); other directors' results are unaffected.
      }
    }))
  } finally {
    for (const clientPromise of clientsByDirector.values()) {
      clientPromise.then(client => client.disconnect()).catch(() => {})
    }
    if (requestId === latestRequestId && !isUnmounted) {
      lines.value = buckets.flatMap(bucket => bucket ?? [])
      loading.value = false
    }
  }
}

async function openLine(line) {
  try {
    await switchActiveDirector(line.director)
    await router.push({
      name: 'job-details',
      params: { id: line.jobId },
      query: buildJobDetailsQuery({
        director: line.director,
        dashboardOrigin: true,
        logFocus: line.type,
      }),
    })
  } catch (error) {
    $q.notify({ type: 'negative', message: error.message })
  }
}

watch(
  () => [ctx.refreshToken.value, ctx.activeDirectors.value.join('\0')],
  loadTroubleLines,
  { immediate: true }
)
</script>

<style scoped>
.log-line-text {
  font-family: monospace;
  font-size: 0.8rem;
  white-space: normal;
  word-break: break-word;
}
</style>
