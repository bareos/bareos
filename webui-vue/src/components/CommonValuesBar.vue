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
  <div v-if="entries.length" class="common-values-bar row items-center q-gutter-xs q-px-sm q-py-xs">
    <q-icon name="filter_alt_off" size="xs" color="grey-6" />
    <span class="text-caption text-grey-7">{{ summary }}</span>
    <q-chip
      v-for="entry in entries"
      :key="entry.name"
      dense clickable square size="sm" color="grey-3" text-color="grey-9"
      :data-testid="`${testid}-${entry.name}`"
      :title="t('Show this column in the table again')"
      @click="$emit('pin', entry.name)"
    >
      <span class="text-grey-7 q-mr-xs">{{ entry.label }}:</span>
      <span class="text-weight-medium">{{ entry.display }}</span>
      <q-icon name="push_pin" size="12px" class="q-ml-xs" />
    </q-chip>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  entries: { type: Array, default: () => [] },
  rowCount: { type: Number, default: 0 },
  rowLabel: { type: String, default: '' },
  testid: { type: String, default: 'common-value' },
})

defineEmits(['pin'])

const { t } = useI18n()

const summary = computed(() => t('All {count} {items} share:', {
  count: props.rowCount,
  items: props.rowLabel || t('rows'),
}))
</script>

<style scoped>
.common-values-bar {
  border-bottom: 1px solid rgba(0, 0, 0, 0.08);
}
</style>
