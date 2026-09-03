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
  <div class="row q-col-gutter-md text-center q-pa-sm">
    <div class="col" v-for="s in summaryStats" :key="s.label">
      <router-link
        :to="{ name: 'jobs', query: withJobsStatusFilterQuery({}, s.status) }"
        class="text-decoration-none"
      >
        <StatNumber :value="s.count" :label="s.label" :color="s.color" />
      </router-link>
    </div>
  </div>
</template>

<script setup>
import { inject, computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { withJobsStatusFilterQuery } from '../../utils/jobs.js'
import { DASHBOARD_CONTEXT_KEY } from '../dashboardContext.js'
import StatNumber from '../../components/StatNumber.vue'

const { t } = useI18n()
const ctx = inject(DASHBOARD_CONTEXT_KEY)
const past24hStatusCounts = computed(() => (
  ctx.aggregate.value.jobsPast24hStatusCounts ?? {}
))

const summaryStats = computed(() => {
  const countByStatus = (code) => Number(past24hStatusCounts.value[code] ?? 0)
  return [
    { label: t('Running'),    status: 'R', color: 'info',     count: countByStatus('R') },
    { label: t('Waiting'),    status: 'C', color: 'grey',     count: countByStatus('C') },
    { label: t('Successful'), status: 'T', color: 'positive', count: countByStatus('T') },
    { label: t('Warning'),    status: 'W', color: 'warning',  count: countByStatus('W') },
    { label: t('Failed'),     status: 'f', color: 'negative', count: countByStatus('f') },
  ]
})
</script>
