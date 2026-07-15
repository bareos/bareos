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
  <q-card v-if="options.length > 1" flat bordered class="q-mb-md bareos-panel">
    <q-card-section class="row items-center q-gutter-sm q-px-md q-py-sm">
      <span>{{ title }}</span>
      <q-space />
      <q-chip
        dense
        square
        color="white"
        text-color="primary"
        class="q-px-sm"
        :title="selectedDirectorNames.join(', ')"
      >
        <div class="row items-center q-gutter-xs no-wrap">
          <div v-if="selectedDirectorsPreview.length" class="row items-center q-gutter-xs no-wrap">
            <span
              v-for="director in selectedDirectorsPreview"
              :key="director"
              class="director-option-swatch"
              :style="directorSwatchStyle(director)"
            />
            <span v-if="remainingSelectedCount > 0" class="text-caption">
              +{{ remainingSelectedCount }}
            </span>
          </div>
          <span>{{ summaryLabel }}</span>
        </div>
      </q-chip>
      <q-btn-dropdown
        dense
        outline
        no-caps
        color="primary"
        icon="tune"
        :label="t('Directors')"
        :data-testid="dataTestId"
      >
        <div style="min-width: 300px; max-width: 360px">
          <div class="text-caption text-grey-7 q-px-md q-pt-sm q-pb-xs">
            {{ helpText }}
          </div>
          <div class="row justify-end q-px-sm q-pb-sm">
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
                <div class="row items-center q-gutter-sm">
                  <span
                    class="director-option-swatch"
                    :style="directorSwatchStyle(option.value ?? option.label)"
                  />
                  <span>{{ option.label ?? option.value }}</span>
                </div>
              </q-item-section>
            </q-item>
          </q-list>
        </div>
      </q-btn-dropdown>
    </q-card-section>

    <q-card-section v-if="$slots.default || errors.length" class="q-pt-none q-pb-sm q-px-md">
      <slot />

      <q-banner
        v-if="errors.length"
        rounded
        dense
        class="bg-warning text-black"
        :class="{ 'q-mt-sm': !!$slots.default }"
      >
        <template #avatar>
          <q-icon name="warning" />
        </template>
        <div
          v-for="item in errors"
          :key="`${item.director}\u0000${item.message}`"
          class="row items-center q-gutter-xs q-mb-xs"
        >
          <DirectorBadge :director="item.director" size="sm" />
          <span>{{ item.message }}</span>
        </div>
      </q-banner>
    </q-card-section>
  </q-card>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import DirectorBadge from './DirectorBadge.vue'
import { resolveDirectorColors } from '../utils/directorColors.js'

const props = defineProps({
  title: {
    type: String,
    required: true,
  },
  summaryLabel: {
    type: String,
    required: true,
  },
  modelValue: {
    type: Array,
    required: true,
  },
  options: {
    type: Array,
    required: true,
  },
  helpText: {
    type: String,
    required: true,
  },
  dataTestId: {
    type: String,
    default: null,
  },
  errors: {
    type: Array,
    default: () => [],
  },
})

const emit = defineEmits(['update:modelValue'])
const { t } = useI18n()

const optionValues = computed(() =>
  props.options
    .map(option => String(option?.value ?? '').trim())
    .filter(Boolean)
)

const selectedValues = computed(() => {
  const byValue = new Set(
    (props.modelValue ?? [])
      .map(value => String(value ?? '').trim())
      .filter(Boolean)
  )

  return optionValues.value.filter(value => byValue.has(value))
})

const selectedDirectorNames = computed(() => (
  props.options
    .filter(option => selectedValues.value.includes(String(option?.value ?? '').trim()))
    .map(option => String(option?.label ?? option?.value ?? '').trim())
    .filter(Boolean)
))

const selectedDirectorsPreview = computed(() => selectedValues.value.slice(0, 3))
const remainingSelectedCount = computed(() => (
  Math.max(selectedValues.value.length - selectedDirectorsPreview.value.length, 0)
))

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

function directorSwatchStyle(name) {
  const colors = resolveDirectorColors(name, optionValues.value)
  return {
    display: 'inline-block',
    width: '12px',
    height: '12px',
    borderRadius: '999px',
    backgroundColor: colors.background,
    border: `1px solid ${colors.border}`,
    flexShrink: 0,
  }
}
</script>
