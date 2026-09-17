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
  <div class="plugin-options-editor" data-testid="plugin-options-editor">
    <div class="row items-center q-gutter-xs">
      <q-tabs
        v-if="blocks.length > 1"
        v-model="activeIndex"
        dense
        inline-label
        outside-arrows
        class="col plugin-options-editor__tabs"
        data-testid="plugin-options-editor-tabs"
      >
        <q-tab
          v-for="(block, index) in blocks"
          :key="index"
          :name="index"
          :label="block.pluginName || t('(unnamed plugin)')"
        />
      </q-tabs>
      <q-space v-else />
      <q-btn
        flat dense no-caps
        icon="add"
        color="primary"
        :label="t('Add plugin')"
        data-testid="plugin-options-editor-add-block"
        @click="addBlock"
      />
      <q-btn
        v-if="blocks.length > 1"
        flat dense no-caps
        icon="remove_circle_outline"
        color="negative"
        :label="t('Remove plugin')"
        data-testid="plugin-options-editor-remove-block"
        @click="removeBlock(activeIndex)"
      />
    </div>

    <div v-if="activeBlock" class="q-mt-sm">
      <q-input
        v-model="activeBlock.pluginName"
        dense outlined
        :label="t('Plugin name')"
        data-testid="plugin-options-editor-plugin-name"
        @update:model-value="emitModelValue"
      />

      <div
        v-for="(option, rowIndex) in activeBlock.options"
        :key="rowIndex"
        class="row items-start q-gutter-xs q-mt-xs"
        data-testid="plugin-options-editor-row"
      >
        <q-select
          v-model="option.key"
          dense outlined
          use-input
          hide-selected
          fill-input
          new-value-mode="add-unique"
          class="col-4"
          :label="t('Option')"
          :options="filteredKeyOptions(option.key)"
          :option-label="opt => (typeof opt === 'string' ? opt : opt.name)"
          :option-value="opt => (typeof opt === 'string' ? opt : opt.name)"
          emit-value
          map-options
          data-testid="plugin-options-editor-row-key"
          @filter="(val, update) => filterKeyOptions(val, update)"
          @update:model-value="emitModelValue"
        >
          <template #option="scope">
            <q-item v-bind="scope.itemProps">
              <q-item-section>
                <q-item-label>{{ scope.opt.name ?? scope.opt }}</q-item-label>
                <q-item-label v-if="scope.opt.description" caption>
                  {{ scope.opt.description }}
                </q-item-label>
              </q-item-section>
              <q-item-section v-if="scope.opt.status" side>
                <q-badge
                  :color="scope.opt.status === 'required' ? 'orange' : 'grey'"
                  :label="scope.opt.status === 'required' ? t('Required') : t('Optional')"
                />
              </q-item-section>
            </q-item>
          </template>
        </q-select>
        <q-input
          v-model="option.value"
          dense outlined
          class="col"
          :label="t('Value')"
          data-testid="plugin-options-editor-row-value"
          @update:model-value="emitModelValue"
        />
        <q-btn
          flat dense round
          icon="delete"
          color="negative"
          :aria-label="t('Remove option')"
          data-testid="plugin-options-editor-row-delete"
          @click="removeRow(rowIndex)"
        />
      </div>

      <div class="q-mt-xs">
        <q-btn
          flat dense no-caps
          icon="add"
          color="primary"
          :label="t('Add option')"
          data-testid="plugin-options-editor-add-row"
          @click="addRow"
        />
      </div>
    </div>

    <div class="text-caption text-grey-7 q-mt-sm plugin-options-editor__preview">
      <span class="text-weight-medium">{{ t('Resulting pluginoptions string') }}:</span>
      <code>{{ previewDocument || t('(empty)') }}</code>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  buildPluginOptionsDocument,
  findRestorePluginHintIdForBlockName,
  parsePluginOptionsDocument,
} from '../utils/restore.js'

const props = defineProps({
  modelValue: {
    type: String,
    default: '',
  },
  pluginHints: {
    type: Object,
    default: () => ({}),
  },
})

const emit = defineEmits(['update:modelValue'])

const { t } = useI18n()

function parsedOrEmpty(value) {
  const parsed = parsePluginOptionsDocument(value)
  return parsed.length > 0 ? parsed : [{ pluginName: '', options: [] }]
}

const blocks = ref(parsedOrEmpty(props.modelValue))
const activeIndex = ref(0)
let lastEmitted = buildPluginOptionsDocument(blocks.value)

watch(() => props.modelValue, (value) => {
  if (value === lastEmitted) {
    return
  }
  blocks.value = parsedOrEmpty(value)
  activeIndex.value = 0
})

const activeBlock = computed(() => blocks.value[activeIndex.value] ?? null)

const previewDocument = computed(() => buildPluginOptionsDocument(blocks.value))

function emitModelValue() {
  const document = buildPluginOptionsDocument(blocks.value)
  lastEmitted = document
  emit('update:modelValue', document)
}

function addBlock() {
  blocks.value.push({ pluginName: '', options: [] })
  activeIndex.value = blocks.value.length - 1
  emitModelValue()
}

function removeBlock(index) {
  if (blocks.value.length <= 1) {
    return
  }
  blocks.value.splice(index, 1)
  activeIndex.value = Math.max(0, Math.min(activeIndex.value, blocks.value.length - 1))
  emitModelValue()
}

function addRow() {
  if (!activeBlock.value) {
    return
  }
  activeBlock.value.options.push({ key: '', value: '' })
  emitModelValue()
}

function removeRow(rowIndex) {
  if (!activeBlock.value) {
    return
  }
  activeBlock.value.options.splice(rowIndex, 1)
  emitModelValue()
}

function currentHint() {
  const hintId = findRestorePluginHintIdForBlockName(activeBlock.value?.pluginName, props.pluginHints)
  return hintId ? props.pluginHints?.[hintId] : null
}

function filteredKeyOptions(currentValue) {
  const hint = currentHint()
  const usedKeys = new Set(
    (activeBlock.value?.options ?? [])
      .map(option => option.key)
      .filter(key => key && key !== currentValue)
  )
  return (hint?.options ?? []).filter(option => !usedKeys.has(option.name))
}

function filterKeyOptions(val, update) {
  update(() => {
    // Options are already computed reactively via :options binding; this
    // handler only needs to exist so q-select's use-input mode keeps the
    // typed text instead of resetting it.
  })
}
</script>

<style scoped>
.plugin-options-editor__preview code {
  display: block;
  margin-top: 0.15rem;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
</style>
