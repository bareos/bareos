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
import { inject, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { useDirectorStore } from '../../stores/director.js'
import { useSettingsStore } from '../../stores/settings.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { buildJobDetailsQuery, classifyLogLine } from '../../utils/jobs.js'
import { timeAgo } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'

// Safety cap on the number of jobs whose logs are fetched, to avoid an
// unbounded number of `list joblog` calls if an unusually large number of
// jobs ran in the last 24 hours. The number of *lines* shown is not capped.
const TROUBLE_STATUSES = new Set(['W', 'E', 'f', 'A'])
const MAX_JOBS = 200

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()
const director = useDirectorStore()
const settings = useSettingsStore()

const ctx = inject(DASHBOARD_CONTEXT_KEY)

const loading = ref(false)
const lines = ref([])

async function loadTroubleLines() {
  const jobs = (ctx.aggregate.value.jobsPast24h ?? [])
    .filter(j => TROUBLE_STATUSES.has(j.status))
    .slice(0, MAX_JOBS)

  if (!jobs.length) {
    lines.value = []
    return
  }

  loading.value = true
  const collected = []
  try {
    for (const job of jobs) {
      try {
        await switchActiveDirector(job.director)
        const res = await director.call(`list joblog jobid=${job.id ?? job.jobid}`)
        const rows = Array.isArray(res?.joblog) ? res.joblog : []
        for (const row of rows) {
          const time = (row.time ?? '').trim()
          const logtext = (row.logtext ?? '').trimEnd()
          const type = classifyLogLine(`${time} ${logtext}`.trim())
          if (type === 'error' || type === 'warning') {
            collected.push({
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
        // Skip jobs whose log could not be fetched (e.g. director offline).
      }
    }
  } finally {
    lines.value = collected
    loading.value = false
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

watch(() => ctx.aggregate.value.jobsPast24h, loadTroubleLines, { immediate: true })
</script>

<style scoped>
.log-line-text {
  font-family: monospace;
  font-size: 0.8rem;
  white-space: normal;
  word-break: break-word;
}
</style>
