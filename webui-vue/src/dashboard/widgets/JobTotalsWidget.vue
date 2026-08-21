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
  <div class="row q-gutter-sm q-pa-sm wrap">
    <div v-for="stat in totalStats" :key="stat.label" class="col-auto">
      <div class="text-caption text-grey-6" style="white-space:nowrap">{{ stat.label }}</div>
      <div class="text-weight-bold" style="font-size:1rem; line-height:1.2">{{ stat.value }}</div>
    </div>
  </div>
</template>

<script setup>
import { inject, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { formatBytes } from '../../mock/index.js'
import { formatNumber } from '../../utils/locales.js'
import { useSettingsStore } from '../../stores/settings.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'

const { t } = useI18n()
const settings = useSettingsStore()
const ctx = inject(DASHBOARD_CONTEXT_KEY)

const totals = computed(() => ctx.aggregate.value.jobTotals)
const clientCount = computed(() => ctx.aggregate.value.clientCount)
const storageCount = computed(() => ctx.aggregate.value.storageCount)

const totalStats = computed(() => [
  { label: t('Total Jobs'),  value: totals.value.jobs },
  { label: t('Total Files'), value: formatNumber(totals.value.files ?? 0, settings.locale) },
  { label: t('Total Bytes'), value: formatBytes(totals.value.bytes ?? 0) },
  { label: t('Clients'),     value: clientCount.value },
  { label: t('Storages'),    value: storageCount.value },
])
</script>
