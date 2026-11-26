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
  <q-btn flat round dense icon="view_column" :title="t('Choose columns')" :aria-label="t('Choose columns')">
    <q-menu anchor="bottom right" self="top right">
      <q-list dense style="min-width:200px">
        <q-item-label header>{{ t('Show columns') }}</q-item-label>
        <q-item
          v-for="column in columns"
          :key="column.name"
          clickable
          @click="$emit('toggle', column.name)"
        >
          <q-item-section side>
            <q-checkbox
              dense
              :aria-label="column.label"
              :model-value="column.visible"
              @update:model-value="$emit('toggle', column.name)"
              @click.stop
            />
          </q-item-section>
          <q-item-section>{{ column.label }}</q-item-section>
        </q-item>
      </q-list>
    </q-menu>
  </q-btn>
</template>

<script setup>
import { useI18n } from 'vue-i18n'

defineProps({
  columns: {
    type: Array,
    required: true,
    // Each entry: { name, label, visible }
  },
})
defineEmits(['toggle'])

const { t } = useI18n()
</script>
