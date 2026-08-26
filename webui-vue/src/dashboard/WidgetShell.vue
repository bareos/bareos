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

<!--
  WidgetShell wraps every dashboard widget with:
  • A blue panel-header title bar (always visible)
  • Gear (configure) and Remove buttons shown only in edit mode
  • A drag handle covering the whole header when in edit mode
-->
<template>
  <q-card flat bordered class="bareos-panel column" style="height:100%; overflow:hidden">
    <!-- Header -->
    <q-card-section
      class="panel-header row items-center no-wrap"
      :class="{ 'widget-drag-handle': editMode }"
      style="flex-shrink:0; cursor: default"
      :style="editMode ? 'cursor: grab' : ''"
    >
      <q-icon v-if="icon" :name="icon" size="sm" class="q-mr-sm" />
      <span class="ellipsis" style="flex:1; min-width:0">{{ title }}</span>
      <q-space />

      <!-- Edit-mode controls -->
      <template v-if="editMode">
        <q-btn
          flat round dense
          icon="settings"
          color="white"
          size="sm"
          :title="t('Configure widget')"
          @click.stop="$emit('configure')"
        />
        <q-btn
          flat round dense
          icon="close"
          color="white"
          size="sm"
          :title="t('Remove widget')"
          @click.stop="$emit('remove')"
        />
      </template>
    </q-card-section>

    <!-- Content: bounded container, vertical scroll only.
         Horizontal scroll is handled by q-table__middle internally. -->
    <div style="position:relative; flex:1; min-height:0; min-width:0; overflow:hidden">
      <div style="position:absolute; inset:0; overflow-y:auto; overflow-x:hidden; padding:4px">
        <slot />
      </div>
    </div>
  </q-card>
</template>

<script setup>
import { useI18n } from 'vue-i18n'

const { t } = useI18n()

defineProps({
  title:    { type: String,  default: '' },
  icon:     { type: String,  default: '' },
  editMode: { type: Boolean, default: false },
})

defineEmits(['configure', 'remove'])
</script>

<style scoped>
.widget-drag-handle {
  user-select: none;
}
</style>
