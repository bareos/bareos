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
  WidgetPickerDialog
  Shows a searchable grid of all available widget types.
  Emits 'pick' with the chosen widget definition.
-->
<template>
  <q-dialog
    :model-value="modelValue"
    :maximized="isMobile"
    @update:model-value="$emit('update:modelValue', $event)"
  >
    <q-card :style="dialogCardStyle">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Add Widget') }}</span>
        <q-space />
        <q-btn flat round dense icon="close" color="white" @click="$emit('update:modelValue', false)" />
      </q-card-section>

      <q-card-section>
        <q-input
          v-model="search"
          dense outlined
          :placeholder="t('Search widgets…')"
          clearable
          autofocus
        >
          <template #prepend><q-icon name="search" /></template>
        </q-input>
      </q-card-section>

      <q-card-section style="max-height:420px; overflow-y:auto; padding-top:0">
        <div class="row q-col-gutter-sm">
          <div
            v-for="def in filtered"
            :key="def.type"
            class="col-12 col-sm-6"
          >
            <q-card
              flat bordered
              clickable
              class="widget-picker-card q-pa-sm"
              @click="pick(def)"
            >
              <div class="row items-center no-wrap q-gutter-sm">
                <q-icon :name="def.icon" size="28px" color="primary" />
                <div style="min-width:0">
                  <div class="text-weight-medium text-body2 ellipsis">{{ t(def.label) }}</div>
                  <div class="text-caption text-grey-6" style="white-space:normal">
                    {{ t(def.description) }}
                  </div>
                </div>
              </div>
            </q-card>
          </div>
          <div v-if="!filtered.length" class="col-12 text-grey text-caption text-center q-py-md">
            {{ t('No widgets match your search.') }}
          </div>
        </div>
      </q-card-section>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { getAllWidgetDefinitions } from './widgetRegistry.js'

const { t } = useI18n()
const $q = useQuasar()

defineProps({ modelValue: { type: Boolean, default: false } })
const emit = defineEmits(['update:modelValue', 'pick'])

const search = ref('')
const allDefs = getAllWidgetDefinitions()
const isMobile = computed(() => $q.screen.lt.sm)
const dialogCardStyle = computed(() => (isMobile.value
  ? 'width:100%; max-width:100vw; min-width:0; max-height:100vh'
  : 'min-width:520px; max-width:720px; width:100%'))

const filtered = computed(() => {
  const q = search.value.toLowerCase().trim()
  if (!q) return allDefs
  return allDefs.filter(d =>
    d.label.toLowerCase().includes(q) || d.description.toLowerCase().includes(q)
  )
})

function pick(def) {
  emit('pick', def)
  emit('update:modelValue', false)
}
</script>

<style scoped>
.widget-picker-card {
  cursor: pointer;
  transition: box-shadow 0.15s, border-color 0.15s;
  border-radius: 6px;
}
.widget-picker-card:hover {
  border-color: var(--q-primary);
  box-shadow: 0 2px 8px rgba(0, 117, 190, 0.18);
}
</style>
