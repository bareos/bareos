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
  WidgetConfigDialog
  Allows the user to:
  - Edit the widget instance title
  - Fill in required and optional property fields
  Required props use live director data (fetched once the dialog opens).
  Emits 'save' with { title, props }.
-->
<template>
  <q-dialog :model-value="modelValue" @update:model-value="$emit('update:modelValue', $event)">
    <q-card style="min-width:400px; max-width:560px; width:100%">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Configure Widget') }}</span>
        <q-space />
        <q-btn flat round dense icon="close" color="white" @click="cancel" />
      </q-card-section>

      <q-card-section class="q-pt-md">
        <!-- Title -->
        <q-input
          v-model="localTitle"
          dense outlined
          :label="t('Widget title')"
          class="q-mb-md"
        />

        <!-- Required props -->
        <template v-if="shownProps.length">
          <div class="text-caption text-grey-6 q-mb-xs">{{ t('Properties') }}</div>

          <div v-for="prop in shownProps" :key="prop.key" class="q-mb-sm">
            <!-- select with live options -->
            <q-select
              v-if="prop.type === 'select'"
              v-model="localProps[prop.key]"
              dense outlined
              :label="prop.label"
              :options="propOptions[prop.key] ?? []"
              :loading="optionsLoading[prop.key]"
              emit-value
              map-options
              option-value="value"
              option-label="label"
            />
            <!-- free text -->
            <q-input
              v-else
              v-model="localProps[prop.key]"
              dense outlined
              :label="prop.label"
            />
          </div>
        </template>
      </q-card-section>

      <q-card-actions align="right" class="q-pb-md q-pr-md">
        <q-btn flat :label="t('Cancel')" @click="cancel" />
        <q-btn color="primary" :label="t('Save')" :disable="!isValid" @click="save" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth.js'
import { getWidgetDefinition } from './widgetRegistry.js'

const { t } = useI18n()
const auth = useAuthStore()

const props = defineProps({
  modelValue: { type: Boolean, default: false },
  /** The widget instance being configured. */
  widget: { type: Object, required: true },
  /** If true, only show required props (first-time config after adding). */
  requiredOnly: { type: Boolean, default: false },
  /** Active director names, used to fetch live option lists. */
  activeDirectors: { type: Array, default: () => [] },
})

const emit = defineEmits(['update:modelValue', 'save'])

const localTitle = ref('')
const localProps = ref({})
const propOptions = ref({})
const optionsLoading = ref({})

const def = computed(() => getWidgetDefinition(props.widget?.type))

const shownProps = computed(() => {
  if (!def.value) return []
  const all = [
    ...(def.value.requiredProps ?? []),
    ...(props.requiredOnly ? [] : (def.value.optionalProps ?? [])),
  ]
  return all
})

const isValid = computed(() => {
  const required = def.value?.requiredProps ?? []
  return required.every(p => {
    const v = localProps.value[p.key]
    return v !== undefined && v !== null && v !== ''
  })
})

async function loadOptions() {
  const credentials = auth.getCredentials()
  if (!credentials) return
  for (const prop of shownProps.value) {
    if (prop.type === 'select' && typeof prop.options === 'function') {
      optionsLoading.value[prop.key] = true
      try {
        propOptions.value[prop.key] = await prop.options(credentials, props.activeDirectors)
      } catch {
        propOptions.value[prop.key] = []
      } finally {
        optionsLoading.value[prop.key] = false
      }
    }
  }
}

// Initialise local state when dialog opens or widget changes.
watch(
  () => [props.modelValue, props.widget],
  ([open]) => {
    if (!open) return
    localTitle.value = props.widget?.title ?? ''
    localProps.value = { ...(props.widget?.props ?? {}) }
    propOptions.value = {}
    optionsLoading.value = {}
    loadOptions()
  },
  { immediate: true },
)

function save() {
  emit('save', {
    title: localTitle.value.trim(),
    props: { ...localProps.value },
  })
  emit('update:modelValue', false)
}

function cancel() {
  emit('update:modelValue', false)
}
</script>
