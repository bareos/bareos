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
  <div class="running-jobs-treemap-root">
    <q-btn
      v-if="queuedJobs.length"
      flat round dense size="sm"
      class="running-jobs-treemap-toggle"
      :icon="includeWaiting ? 'hourglass_empty' : 'hourglass_disabled'"
      :color="includeWaiting ? 'primary' : 'grey-7'"
      :title="includeWaiting
        ? t('Showing active and queued jobs (click to show active only)')
        : t('Showing active jobs only (click to include queued jobs)')"
      @click="includeWaiting = !includeWaiting"
    />
    <div v-if="!displayJobs.length" class="running-jobs-treemap-empty text-grey text-caption text-center">
      {{ t('No running jobs') }}
    </div>
    <div v-else ref="treemapEl" class="running-jobs-treemap-container">
      <button
        v-for="tile in tiles"
        :key="tile.key"
        type="button"
        class="running-jobs-treemap-tile"
        :style="tile.style"
        :title="tile.tooltip"
        @click="openJobDetails(tile.job)"
      >
        <span class="running-jobs-treemap-label" :class="{ 'running-jobs-treemap-label--compact': tile.compact }">
          <span class="running-jobs-treemap-name">{{ tile.job.name }}</span>
          <span class="running-jobs-treemap-duration">{{ tile.durationLabel }}</span>
        </span>
      </button>
    </div>
  </div>
</template>

<script setup>
import { computed, inject, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { buildJobDetailsQuery } from '../../utils/jobs.js'
import { isWaitingStatus, elapsedRunningSecs } from '../../utils/runningJobsGrouping.js'
import { squarifyTreemap } from '../../utils/treemapLayout.js'
import { switchActiveDirector } from '../../composables/useDirectorSession.js'
import { formatDuration } from '../../mock/index.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'

const { t } = useI18n()
const $q = useQuasar()
const router = useRouter()

const ctx = inject(DASHBOARD_CONTEXT_KEY)

const now = ref(Date.now())
const treemapEl = ref(null)
const treemapWidth = ref(400)
const treemapHeight = ref(300)

const _clockTimer = setInterval(() => { now.value = Date.now() }, 1000)
onUnmounted(() => clearInterval(_clockTimer))

let _resizeObserver = null
watch(treemapEl, (el) => {
  _resizeObserver?.disconnect()
  _resizeObserver = null
  if (!el) return

  treemapWidth.value = el.clientWidth || 400
  treemapHeight.value = el.clientHeight || 300

  _resizeObserver = new ResizeObserver(([entry]) => {
    if (entry.contentRect.width > 0) treemapWidth.value = entry.contentRect.width
    if (entry.contentRect.height > 0) treemapHeight.value = entry.contentRect.height
  })
  _resizeObserver.observe(el)
}, { flush: 'post' })
onUnmounted(() => _resizeObserver?.disconnect())

const activeJobs = computed(() =>
  (ctx.aggregate.value.runningJobs ?? []).filter(j => !isWaitingStatus(j.runtimeStatus ?? j.status))
)
const queuedJobs = computed(() =>
  (ctx.aggregate.value.runningJobs ?? []).filter(j => isWaitingStatus(j.runtimeStatus ?? j.status))
)
const includeWaiting = ref(false)
const displayJobs = computed(() => (
  includeWaiting.value ? [...activeJobs.value, ...queuedJobs.value] : activeJobs.value
))

function elapsedSecs(job) {
  return elapsedRunningSecs(job, now.value)
}

function tileValue(job) {
  return Math.max(1, elapsedSecs(job))
}

function tileColor(job) {
  const status = String(job.runtimeStatus ?? job.status ?? '').trim().toLowerCase()
  if (!status || status.includes('running')) return '#2E7D32'
  if (status.includes('is waiting')) return '#E65100'
  return '#607D8B'
}

function jobTooltip(job, durationLabel) {
  return [
    job.name,
    job.client,
    job.director,
    durationLabel,
    job.runtimeStatus ?? job.status,
  ].filter(Boolean).join('\n')
}

const tiles = computed(() => {
  if (!displayJobs.value.length) return []

  const rectangles = squarifyTreemap(
    displayJobs.value.map((job, index) => ({
      key: job.scopeKey ?? `${job.director ?? 'unknown'}:${job.id ?? job.jobid ?? index}`,
      job,
      value: tileValue(job),
      color: tileColor(job),
    })),
    {
      width: treemapWidth.value || 400,
      height: treemapHeight.value || 300,
    }
  )

  return rectangles.map((tile) => {
    const durationLabel = formatDuration(elapsedSecs(tile.job))
    const compact = tile.width < 120 || tile.height < 72

    return {
      ...tile,
      durationLabel,
      compact,
      tooltip: jobTooltip(tile.job, durationLabel),
      style: {
        left: `${tile.x}px`,
        top: `${tile.y}px`,
        width: `${tile.width}px`,
        height: `${tile.height}px`,
        backgroundColor: tile.color,
      },
    }
  })
})

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
</script>

<style scoped>
.running-jobs-treemap-root {
  position: relative;
  height: 100%;
  overflow: hidden;
  padding: 8px;
  box-sizing: border-box;
}

.running-jobs-treemap-toggle {
  position: absolute;
  top: 6px;
  right: 6px;
  z-index: 2;
  border-radius: 999px;
  background: rgba(255, 255, 255, 0.82);
}

.running-jobs-treemap-empty {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 100%;
}

.running-jobs-treemap-container {
  position: relative;
  width: 100%;
  height: 100%;
  min-height: 0;
}

.running-jobs-treemap-tile {
  position: absolute;
  display: block;
  overflow: hidden;
  box-sizing: border-box;
  border: 2px solid white;
  border-radius: 6px;
  padding: 0;
  color: white;
  cursor: pointer;
  text-align: left;
  transition: opacity 0.2s ease;
}

.running-jobs-treemap-tile:hover,
.running-jobs-treemap-tile:focus-visible {
  opacity: 0.92;
}

.running-jobs-treemap-label {
  display: flex;
  flex-direction: column;
  justify-content: center;
  gap: 2px;
  width: 100%;
  height: 100%;
  padding: 6px 8px;
  box-sizing: border-box;
}

.running-jobs-treemap-label--compact {
  padding: 4px 6px;
}

.running-jobs-treemap-name,
.running-jobs-treemap-duration {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.running-jobs-treemap-name {
  font-size: 12px;
  font-weight: 600;
  line-height: 1.2;
}

.running-jobs-treemap-duration {
  font-size: 11px;
  line-height: 1.15;
  opacity: 0.92;
}

.running-jobs-treemap-label--compact .running-jobs-treemap-name {
  font-size: 11px;
}

.running-jobs-treemap-label--compact .running-jobs-treemap-duration {
  font-size: 10px;
}
</style>
