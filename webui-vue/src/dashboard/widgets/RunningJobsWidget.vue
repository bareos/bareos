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
    <q-list v-if="!activeJobs.length && !queuedJobs.length" separator>
      <q-item>
        <q-item-section class="text-grey text-caption text-center q-py-md">
          {{ t('No running jobs') }}
        </q-item-section>
      </q-item>
    </q-list>
    <q-virtual-scroll
      v-else
      style="height:100%"
      :items="displayRows"
      virtual-scroll-item-size="96"
      separator
      v-slot="{ item: row }"
    >
      <q-item
        v-if="row.type === 'section'"
        :key="row.key"
        dense
        class="bg-grey-2 text-caption text-weight-medium"
      >
        <q-item-section>
          <span v-if="row.label">{{ row.label }}</span>
        </q-item-section>
        <q-item-section v-if="row.showToggle" side>
          <q-btn
            flat round dense size="sm"
            :icon="row.toggleIcon"
            :title="row.toggleTitle"
            @click="toggleQueuedViewMode"
          />
        </q-item-section>
      </q-item>

      <q-item
        v-else-if="row.type === 'group'"
        :key="row.key"
        clickable
        class="q-py-sm"
        @click="toggleQueuedGroup(row.group.key)"
      >
        <q-item-section avatar>
          <q-icon
            :name="isQueuedGroupExpanded(row.group.key) ? 'expand_more' : 'chevron_right'"
            color="grey-7"
          />
        </q-item-section>
        <q-item-section>
          <q-item-label class="text-weight-medium text-orange-8">
            {{ row.group.key }}
            <q-badge color="orange-7" text-color="white" class="q-ml-sm">
              {{ row.group.count }}
            </q-badge>
          </q-item-label>
          <q-item-label caption>
            {{ t('Queued jobs') }}
          </q-item-label>
        </q-item-section>
        <q-item-section side>
          <q-btn
            flat round dense size="sm"
            icon="cancel"
            color="negative"
            :title="t('Cancel all in group')"
            @click.stop="confirmCancelGroup(row.group)"
          />
        </q-item-section>
      </q-item>

      <div v-else-if="row.type === 'group-job'" :key="row.key" class="q-pl-lg">
        <RunningJobRow
          :job="row.job"
          :duration-secs="elapsedSecs(row.job)"
          :show-director-column="showDirectorColumn"
          @open-details="openJobDetails"
          @cancel="confirmCancel"
        />
      </div>

      <RunningJobRow
        v-else
        :key="row.key"
        :job="row.job"
        :duration-secs="elapsedSecs(row.job)"
        :show-director-column="showDirectorColumn"
        @open-details="openJobDetails"
        @cancel="confirmCancel"
      />
    </q-virtual-scroll>
  </div>
</template>

<script setup>
import { inject, computed, ref, onUnmounted } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { buildJobDetailsQuery, buildCancelJobCommand } from '../../utils/jobs.js'
import { isWaitingStatus, groupRunningJobsByStatus, elapsedRunningSecs } from '../../utils/runningJobsGrouping.js'
import {
  QUEUED_GROUP_THRESHOLD,
  resolveQueuedViewMode,
  extractCancelableGroupJobs,
} from '../../utils/runningJobsWidget.js'
import { useDirectorStore } from '../../stores/director.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import RunningJobRow from './RunningJobRow.vue'

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()
const director = useDirectorStore()

const ctx = inject(DASHBOARD_CONTEXT_KEY)
const runningJobs = computed(() => ctx.aggregate.value.runningJobs)
const directorOptions = computed(() => ctx.directorOptions.value)
const showDirectorColumn = computed(() => directorOptions.value.length > 1)

const queuedViewModeOverride = ref(null)
const expandedQueuedGroupKeys = ref([])

const now = ref(Date.now())
const _clockTimer = setInterval(() => { now.value = Date.now() }, 1000)
onUnmounted(() => clearInterval(_clockTimer))

const activeJobs = computed(() =>
  runningJobs.value.filter(job => !isWaitingStatus(jobStatus(job)))
)
const queuedJobs = computed(() =>
  runningJobs.value.filter(job => isWaitingStatus(jobStatus(job)))
)
const showSectionLabels = computed(() =>
  activeJobs.value.length > 0 && queuedJobs.value.length > 0
)
const showQueuedViewToggle = computed(() => queuedJobs.value.length > 0)
const queuedJobGroups = computed(() => groupRunningJobsByStatus(queuedJobs.value))
const queuedViewMode = computed(() =>
  resolveQueuedViewMode(
    queuedJobs.value.length,
    queuedViewModeOverride.value,
    QUEUED_GROUP_THRESHOLD
  )
)
const displayRows = computed(() => {
  const rows = []

  if (showSectionLabels.value && activeJobs.value.length) {
    rows.push({
      type: 'section',
      key: 'section-active',
      label: t('Active'),
      showToggle: false,
    })
  }

  for (const job of activeJobs.value) {
    rows.push({ type: 'job', key: `active:${job.scopeKey}`, job })
  }

  if (queuedJobs.value.length) {
    if (showSectionLabels.value || showQueuedViewToggle.value) {
      rows.push({
        type: 'section',
        key: 'section-queued',
        label: showSectionLabels.value ? t('Queued') : '',
        showToggle: showQueuedViewToggle.value,
        toggleIcon: queuedViewMode.value === 'grouped' ? 'view_list' : 'account_tree',
        toggleTitle: queuedViewMode.value === 'grouped'
          ? t('Switch to flat queued view')
          : t('Switch to grouped queued view'),
      })
    }

    if (queuedViewMode.value === 'grouped') {
      for (const group of queuedJobGroups.value) {
        rows.push({ type: 'group', key: `group:${group.key}`, group })
        if (isQueuedGroupExpanded(group.key)) {
          for (const job of group.jobs) {
            rows.push({
              type: 'group-job',
              key: `group-job:${group.key}:${job.scopeKey}`,
              job,
            })
          }
        }
      }
    } else {
      for (const job of queuedJobs.value) {
        rows.push({ type: 'job', key: `queued:${job.scopeKey}`, job })
      }
    }
  }

  return rows
})

function elapsedSecs(job) {
  return elapsedRunningSecs(job, now.value)
}

function jobStatus(row) { return row.runtimeStatus ?? row.status ?? '?' }

function isQueuedGroupExpanded(groupKey) {
  return expandedQueuedGroupKeys.value.includes(groupKey)
}

function toggleQueuedGroup(groupKey) {
  expandedQueuedGroupKeys.value = isQueuedGroupExpanded(groupKey)
    ? expandedQueuedGroupKeys.value.filter(key => key !== groupKey)
    : [...expandedQueuedGroupKeys.value, groupKey]
}

function toggleQueuedViewMode() {
  queuedViewModeOverride.value = queuedViewMode.value === 'grouped'
    ? 'flat'
    : 'grouped'
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

function confirmCancelGroup(group) {
  const jobsToCancel = extractCancelableGroupJobs(group)
  const count = jobsToCancel.length

  $q.dialog({
    title: t('Cancel all in group'),
    message: t('Cancel {count} queued job(s) in "{reason}"?', {
      count,
      reason: group.key,
    }),
    ok:     { label: t('Cancel all in group'), color: 'negative', flat: true },
    cancel: { label: t('Keep Running'), flat: true },
  }).onOk(() => doCancelGroup(group))
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

async function doCancelGroup(group) {
  const jobsToCancel = extractCancelableGroupJobs(group)
  let completed = 0
  let failed = 0

  for (const [index, { job, id }] of jobsToCancel.entries()) {
    $q.notify({
      type: 'info',
      message: t('Cancelling {current} of {total}...', {
        current: index + 1,
        total: jobsToCancel.length,
      }),
      timeout: 900,
    })

    try {
      await switchActiveDirector(job.director)
      await director.call(buildCancelJobCommand(id))
      completed += 1
    } catch (e) {
      failed += 1
    }
  }

  if (failed) {
    $q.notify({
      type: 'warning',
      message: t('Cancelled {completed} of {total} queued job(s) ({failed} failed).', {
        completed,
        total: jobsToCancel.length,
        failed,
      }),
    })
  } else {
    $q.notify({
      type: 'positive',
      message: t('Cancelled {count} queued job(s).', { count: completed }),
    })
  }

  ctx.refresh()
}
</script>
