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
  <div style="height:100%; overflow:auto">
    <q-list v-if="!runningJobs.length" separator>
      <q-item>
        <q-item-section class="text-grey text-caption text-center q-py-md">
          {{ t('No running jobs') }}
        </q-item-section>
      </q-item>
    </q-list>
    <q-virtual-scroll
      v-else
      style="height:100%"
      :items="runningJobs"
      virtual-scroll-item-size="96"
      separator
      v-slot="{ item: job }"
    >
      <q-item :key="job.scopeKey" class="q-py-sm">
        <q-item-section>
          <q-item-label>
            <a href="#" class="text-primary text-weight-medium" @click.prevent="openJobDetails(job)">
              {{ job.name }}
            </a>
            <DirectorBadge
              v-if="showDirectorColumn"
              :director="job.director"
              size="sm"
              class="q-ml-xs"
            />
            <span class="text-grey-6 text-caption q-ml-xs">({{ job.client }})</span>
          </q-item-label>
          <q-item-label caption>
            {{ formatNumber(job.files ?? 0, settings.locale) }} {{ t('Files') }}
            &middot; {{ formatBytes(job.bytes ?? 0) }}
            &middot; {{ formatDuration(elapsedSecs(job)) }}
          </q-item-label>
          <q-item-label
            v-if="jobStatus(job)"
            caption
            :class="isWaitingStatus(jobStatus(job)) ? 'text-orange-7' : 'text-grey-6'"
          >
            <q-icon
              v-if="isWaitingStatus(jobStatus(job))"
              name="hourglass_empty"
              size="14px"
              class="q-mr-xs"
            />
            {{ jobStatus(job) }}
          </q-item-label>
          <q-linear-progress indeterminate color="positive" class="q-mt-xs"
            style="height:6px; border-radius:3px" />
        </q-item-section>
        <q-item-section side>
          <q-btn
            flat round dense
            icon="cancel" color="negative" size="sm"
            :title="t('Cancel Job')"
            @click="confirmCancel(job)"
          />
        </q-item-section>
      </q-item>
    </q-virtual-scroll>
  </div>
</template>

<script setup>
import { inject, computed, ref, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { formatBytes, formatDuration } from '../../mock/index.js'
import { buildJobDetailsQuery, buildCancelJobCommand } from '../../utils/jobs.js'
import { formatNumber } from '../../utils/locales.js'
import { useSettingsStore } from '../../stores/settings.js'
import { useDirectorStore } from '../../stores/director.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import DirectorBadge from '../../components/DirectorBadge.vue'

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()
const settings = useSettingsStore()
const director = useDirectorStore()

const ctx = inject(DASHBOARD_CONTEXT_KEY)
const runningJobs = computed(() => ctx.aggregate.value.runningJobs)
const directorOptions = computed(() => ctx.directorOptions.value)
const showDirectorColumn = computed(() => directorOptions.value.length > 1)

const now = ref(Date.now())
const _clockTimer = setInterval(() => { now.value = Date.now() }, 1000)
onUnmounted(() => clearInterval(_clockTimer))

function elapsedSecs(job) {
  if (!job.starttime) return 0
  const start = new Date(job.starttime.replace(' ', 'T')).getTime()
  if (isNaN(start)) return 0
  return Math.max(0, Math.floor((now.value - start) / 1000))
}

function jobStatus(row) { return row.runtimeStatus ?? row.status ?? '?' }
function isWaitingStatus(status) {
  return typeof status === 'string' && status.toLowerCase().includes('is waiting')
}

async function openJobDetails(row) {
  try {
    await switchActiveDirector(row.director)
    await router.push({
      name: 'job-details',
      params: { id: row.id ?? row.jobid },
      query: buildJobDetailsQuery({ director: row.director, dashboardOrigin: true }),
    })
  } catch (error) {
    $q.notify({ type: 'negative', message: error.message })
  }
}

function confirmCancel(job) {
  $q.dialog({
    title: t('Cancel Job'),
    message: t('Cancel job {name} (ID {id})?', { name: job.name, id: job.id ?? job.jobid }),
    ok:     { label: t('Cancel Job'), color: 'negative', flat: true },
    cancel: { label: t('Keep Running'), flat: true },
  }).onOk(() => doCancel(job))
}

async function doCancel(job) {
  try {
    await switchActiveDirector(job.director)
    await director.call(buildCancelJobCommand(job.id ?? job.jobid))
    $q.notify({ type: 'positive', message: t('Job {id} cancelled.', { id: job.id ?? job.jobid }) })
    ctx.refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: t('Cancel failed: {message}', { message: e.message }) })
  }
}
</script>
