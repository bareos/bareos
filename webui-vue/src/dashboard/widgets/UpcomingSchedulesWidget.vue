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
  <div class="upcoming-schedules-widget" style="height:100%; width:100%; overflow:auto">
    <div v-if="!scheduledJobs.length" class="row items-center justify-center full-height q-pa-md">
      <div class="text-grey text-caption text-center">
        <q-icon name="mdi-calendar-blank-outline" size="32px" class="q-mb-xs block text-grey-5" />
        {{ t('No upcoming schedules') }}
      </div>
    </div>
    <q-table
      v-else
      :rows="scheduledJobs"
      :columns="columns"
      row-key="scopeKey"
      dense flat
      style="height:100%; width:100%"
      virtual-scroll
      :rows-per-page-options="[0]"
      hide-bottom
    >
      <template #body-cell-scheduled="props">
        <q-td :props="props" style="white-space:nowrap">
          <div class="row items-center no-wrap q-gutter-x-xs">
            <q-icon name="mdi-clock-outline" size="14px" color="primary" />
            <span class="text-weight-medium" :title="formatScheduleTooltip(props.row.scheduled)">
              {{ formatScheduleDisplay(props.row.scheduled) }}
            </span>
          </div>
        </q-td>
      </template>

      <template #body-cell-director="props">
        <q-td :props="props">
          <div class="row items-center q-gutter-xs no-wrap">
            <span :style="directorSwatchStyle(props.row.director || props.value || '', directorOptions)" />
            <span>{{ props.row.director || props.value || '—' }}</span>
          </div>
        </q-td>
      </template>

      <template #body-cell-name="props">
        <q-td :props="props">
          <router-link
            :to="{ name: 'schedules' }"
            class="text-primary text-weight-medium text-decoration-none"
            :title="t('View in Schedules')"
          >
            {{ props.value }}
          </router-link>
        </q-td>
      </template>

      <template #body-cell-level="props">
        <q-td :props="props" class="text-center">
          <JobLevelBadge v-if="props.value" :level="props.value" />
          <span v-else class="text-grey-5">—</span>
        </q-td>
      </template>

      <template #body-cell-pool="props">
        <q-td :props="props">
          <q-badge v-if="props.value" outline color="grey-7" class="q-mr-xs">
            {{ props.value }}
          </q-badge>
          <span v-else class="text-grey-5">—</span>
        </q-td>
      </template>

      <template #body-cell-volume="props">
        <q-td :props="props">
          <span v-if="props.value" class="text-caption text-grey-8">{{ props.value }}</span>
          <span v-else class="text-grey-5">—</span>
        </q-td>
      </template>

      <template #body-cell-storage="props">
        <q-td :props="props">
          <span v-if="props.value" class="text-caption text-grey-8">{{ props.value }}</span>
          <span v-else class="text-grey-5">—</span>
        </q-td>
      </template>
    </q-table>
  </div>
</template>

<script setup>
import { inject, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import { resolveDirectorColors } from '../../utils/directorColors.js'
import {
  formatRelativeDate,
  formatLocalDateTime,
  parseDirectorDate,
} from '../../utils/locales.js'
import { useSettingsStore } from '../../stores/settings.js'
import JobLevelBadge from '../../components/JobLevelBadge.vue'

const { t } = useI18n()
const settings = useSettingsStore()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const scheduledJobs = computed(() => (
  ctx?.aggregate?.value?.scheduledJobs ?? []
))

const directorOptions = computed(() => ctx?.directorOptions?.value ?? [])
const activeDirectors = computed(() => ctx?.activeDirectors?.value ?? [])
const showDirectorColumn = computed(() => activeDirectors.value.length > 1)

function parseScheduledDate(value) {
  if (!value) return null
  if (value instanceof Date && !Number.isNaN(value.getTime())) return value
  const str = String(value).trim()

  const direct = new Date(str.replace(' ', 'T'))
  if (!Number.isNaN(direct.getTime())) return direct
  return parseDirectorDate(str)
}

function formatScheduleDisplay(value) {
  if (!value) return '—'
  const date = parseScheduledDate(value)
  if (!date) return String(value)

  if (settings.relativeTime) {
    return formatRelativeDate(date, settings.locale)
  }
  return formatLocalDateTime(date, settings.locale, {
    dateStyle: 'short',
    timeStyle: 'short',
  })
}

function formatScheduleTooltip(value) {
  if (!value) return ''
  const date = parseScheduledDate(value)
  if (!date) return String(value)

  return formatLocalDateTime(date, settings.locale, {
    dateStyle: 'full',
    timeStyle: 'medium',
  })
}

function directorSwatchStyle(name, knownDirectors) {
  const { background, border } = resolveDirectorColors(name, knownDirectors)
  return {
    display: 'inline-block',
    width: '8px',
    height: '8px',
    borderRadius: '50%',
    backgroundColor: background,
    border: `1px solid ${border}`,
  }
}

const columns = computed(() => {
  const cols = [
    {
      name: 'scheduled',
      label: t('Scheduled Time'),
      field: 'scheduled',
      align: 'left',
      sortable: true,
    },
  ]

  if (showDirectorColumn.value) {
    cols.push({
      name: 'director',
      label: t('Director'),
      field: 'director',
      align: 'left',
      sortable: true,
    })
  }

  cols.push(
    {
      name: 'name',
      label: t('Job'),
      field: 'name',
      align: 'left',
      sortable: true,
    },
    {
      name: 'level',
      label: t('Level'),
      field: 'level',
      align: 'center',
      sortable: true,
    },
    {
      name: 'pool',
      label: t('Pool'),
      field: 'pool',
      align: 'left',
      sortable: true,
    },
    {
      name: 'volume',
      label: t('Volume'),
      field: 'volume',
      align: 'left',
      sortable: true,
    },
    {
      name: 'storage',
      label: t('Storage'),
      field: 'storage',
      align: 'left',
      sortable: true,
    },
  )

  return cols
})
</script>

<style scoped>
.upcoming-schedules-widget {
  font-size: 13px;
}
</style>
