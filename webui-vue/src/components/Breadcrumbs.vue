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
  <q-breadcrumbs
    class="bareos-breadcrumbs text-caption q-mb-sm"
    active-color="grey-8"
  >
    <template #separator>
      <q-icon name="chevron_right" size="16px" color="grey-6" />
    </template>
    <q-breadcrumbs-el
      v-for="(item, index) in items"
      :key="`${item.label}-${index}`"
      :label="item.label"
      :icon="item.icon"
      :to="item.to"
      :data-testid="item.testId"
    />
  </q-breadcrumbs>
</template>

<script setup>
// items: [{ label, icon?, to?, testId? }]
// The last entry is treated as the current page: omit its `to` so
// q-breadcrumbs-el renders it as plain (non-navigable) text.
defineProps({
  items: {
    type: Array,
    required: true,
  },
})
</script>

<style scoped>
.bareos-breadcrumbs :deep(.q-breadcrumbs__el-icon) {
  font-size: 16px;
}
</style>
