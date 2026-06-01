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
  <div :data-testid="dataTestId">
    <div class="row justify-end q-px-sm q-py-sm">
      <q-btn
        flat
        dense
        no-caps
        size="sm"
        color="primary"
        :label="t('Select all')"
        @click="selectAll"
      />
    </div>

    <q-separator />

    <q-list dense>
      <q-item
        v-for="option in options"
        :key="option.value"
        clickable
        v-ripple
        @click="toggleDirector(option.value)"
      >
        <q-item-section avatar>
          <q-checkbox
            dense
            :model-value="isSelected(option.value)"
            @update:model-value="toggleDirector(option.value)"
            @click.stop
          />
        </q-item-section>
        <q-item-section>
          <q-item-label>{{ option.label ?? option.value }}</q-item-label>
          <q-item-label v-if="option.caption" caption>
            {{ option.caption }}
          </q-item-label>
        </q-item-section>
      </q-item>
    </q-list>

    <template v-if="consoleOptions.length">
      <q-separator />
      <q-item-label header>{{ t('Console') }}</q-item-label>
      <q-list dense>
        <q-item
          v-for="option in consoleOptions"
          :key="`console:${option.value}`"
          clickable
          v-ripple
          v-close-popup="closeOnConsole"
          @click="emit('openConsole', option.value)"
        >
          <q-item-section avatar>
            <q-icon name="terminal" />
          </q-item-section>
          <q-item-section>{{ t('Console') }}</q-item-section>
          <q-item-section side class="text-caption">
            {{ option.label ?? option.value }}
          </q-item-section>
        </q-item>
      </q-list>
    </template>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  modelValue: {
    type: Array,
    required: true,
  },
  options: {
    type: Array,
    required: true,
  },
  consoleDirectors: {
    type: Array,
    default: () => [],
  },
  closeOnConsole: {
    type: Boolean,
    default: false,
  },
  dataTestId: {
    type: String,
    default: null,
  },
})

const emit = defineEmits(['update:modelValue', 'openConsole'])
const { t } = useI18n()

const optionValues = computed(() => (
  props.options
    .map(option => String(option?.value ?? '').trim())
    .filter(Boolean)
))

const selectedValues = computed(() => {
  const selected = new Set(
    (props.modelValue ?? [])
      .map(value => String(value ?? '').trim())
      .filter(Boolean)
  )

  return optionValues.value.filter(value => selected.has(value))
})

const consoleOptions = computed(() => {
  const allowed = new Set(
    (props.consoleDirectors ?? [])
      .map(value => String(value ?? '').trim())
      .filter(Boolean)
  )

  return props.options.filter(option => allowed.has(String(option?.value ?? '').trim()))
})

function emitSelected(values) {
  emit('update:modelValue', optionValues.value.filter(value => values.includes(value)))
}

function isSelected(value) {
  return selectedValues.value.includes(String(value ?? '').trim())
}

function toggleDirector(value) {
  const normalizedValue = String(value ?? '').trim()
  if (!normalizedValue) {
    return
  }

  if (isSelected(normalizedValue)) {
    if (selectedValues.value.length <= 1) {
      return
    }

    emitSelected(selectedValues.value.filter(entry => entry !== normalizedValue))
    return
  }

  emitSelected([...selectedValues.value, normalizedValue])
}

function selectAll() {
  emitSelected(optionValues.value)
}
</script>
