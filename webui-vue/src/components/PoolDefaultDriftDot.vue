<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
  <q-icon
    v-if="entries.length"
    name="circle"
    size="8px"
    color="orange-8"
    class="q-ml-xs"
    role="img"
    :aria-label="tooltip"
  >
    <q-tooltip>{{ tooltip }}</q-tooltip>
  </q-icon>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

import { formatPoolDefaultValue } from '../utils/poolDefaults.js'

const props = defineProps({
  // Drift entries as produced by compareVolumeToPoolDefaults().
  entries: { type: Array, default: () => [] },
  poolName: { type: String, default: '' },
})

const { t } = useI18n()

const tooltip = computed(() => {
  const details = props.entries.map(entry => (
    `${t(entry.field.label)}: ${formatPoolDefaultValue(entry.field, entry.poolValue)}`
  )).join(', ')

  return props.poolName
    ? t('Differs from pool {pool}: {details}', { pool: props.poolName, details })
    : t('Differs from the pool defaults: {details}', { details })
})
</script>
