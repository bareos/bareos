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
<template>
  <div class="restore-options-editor" data-testid="restore-options-editor">
    <q-select
      :model-value="model.restoreclient"
      use-input
      fill-input
      hide-selected
      input-debounce="0"
      :options="restoreClientOptions"
      :label="t('Restore to Client')"
      :hint="t('Choose the File Daemon that receives the restored files.')"
      outlined dense emit-value map-options
      :loading="loadingClients"
      :disable="disabled"
      data-testid="restore-target-client"
      @filter="filterRestoreClientOptions"
      @update:model-value="updateField('restoreclient', $event)"
    />
    <q-select
      :model-value="model.restorejob"
      use-input
      fill-input
      hide-selected
      input-debounce="0"
      :options="restoreJobOptions"
      :label="t('Restore Job')"
      outlined dense emit-value map-options
      :loading="loadingRestoreJobs"
      :disable="disabled"
      data-testid="restore-job"
      @filter="filterRestoreJobOptions"
      @update:model-value="updateField('restorejob', $event)"
    />

    <q-option-group
      :model-value="model.relocationMode || 'where'"
      :options="relocationModeOptions"
      color="primary"
      inline
      dense
      data-testid="restore-relocation-mode"
      @update:model-value="updateField('relocationMode', $event)"
    />

    <q-input
      v-if="effectiveRelocationMode === 'where'"
      :model-value="model.where"
      :label="t('Restore to (Where)')"
      outlined dense
      placeholder="/ or /tmp/bareos-restores"
      :hint="t('Enter the destination path on the selected restore client.')"
      data-testid="restore-where"
      @update:model-value="updateField('where', $event)"
    >
      <template #append>
        <q-btn
          flat dense no-caps
          icon="travel_explore"
          :label="t('Browse')"
          data-testid="restore-browse-destination"
          @click="$emit('browseDestination')"
        />
      </template>
    </q-input>

    <q-card v-else-if="effectiveRelocationMode === 'rules'" flat bordered>
      <q-card-section class="q-gutter-sm">
        <div class="text-subtitle2">{{ t('File relocation rules') }}</div>
        <q-input
          :model-value="model.stripPrefix"
          :label="t('Strip prefix')"
          outlined dense
          placeholder="/home"
          data-testid="restore-strip-prefix"
          @update:model-value="updateField('stripPrefix', $event)"
        />
        <q-input
          :model-value="model.addPrefix"
          :label="t('Add prefix')"
          outlined dense
          placeholder="/restore"
          data-testid="restore-add-prefix"
          @update:model-value="updateField('addPrefix', $event)"
        />
        <q-input
          :model-value="model.addSuffix"
          :label="t('Add suffix')"
          outlined dense
          placeholder=".restored"
          data-testid="restore-add-suffix"
          @update:model-value="updateField('addSuffix', $event)"
        />
      </q-card-section>
    </q-card>

    <q-input
      v-else
      :model-value="model.regexWhere"
      :label="t('Raw regexwhere')"
      outlined dense
      placeholder="!^/home/!/restore/home/!"
      :hint="t('Advanced Bareos regexwhere expression. This replaces Where.')"
      data-testid="restore-regexwhere"
      @update:model-value="updateField('regexWhere', $event)"
    />

    <q-card flat bordered class="bg-grey-1" data-testid="restore-destination-preview">
      <q-card-section class="q-py-sm">
        <div class="text-caption text-grey-7">{{ t('Destination preview') }}</div>
        <div v-if="effectiveRelocationMode === 'where'">
          <code>{{ model.where || t('(no destination set)') }}</code>
        </div>
        <div v-else class="q-gutter-xs">
          <q-input
            :model-value="model.relocationSample"
            :label="t('Sample path')"
            dense outlined
            data-testid="restore-relocation-sample"
            @update:model-value="updateField('relocationSample', $event)"
          />
          <div>
            <span class="text-weight-medium">{{ t('Result') }}:</span>
            <code>{{ relocationPreview || t('(enter a sample path)') }}</code>
          </div>
          <div class="text-caption text-grey-7">
            <span class="text-weight-medium">regexwhere:</span>
            <code>{{ effectiveRegexWhere || t('(empty)') }}</code>
          </div>
        </div>
      </q-card-section>
    </q-card>

    <q-select
      :model-value="model.replace"
      :options="replaceOptions"
      :label="t('Replace Policy')"
      outlined dense emit-value map-options
      data-testid="restore-replace-policy"
      @update:model-value="updateField('replace', $event)"
    >
      <template #option="scope">
        <q-item v-bind="scope.itemProps">
          <q-item-section>
            <q-item-label>{{ scope.opt.label }}</q-item-label>
            <q-item-label caption>{{ scope.opt.description }}</q-item-label>
          </q-item-section>
          <q-item-section side>
            <code>{{ scope.opt.value }}</code>
          </q-item-section>
        </q-item>
      </template>
    </q-select>

    <div v-if="showPluginOptions" data-testid="restore-plugin-options">
      <div class="text-caption text-grey-7 q-mb-xs">{{ pluginOptionsHint }}</div>
      <PluginOptionsEditor
        :model-value="model.pluginoptions"
        :plugin-hints="pluginHints"
        :fileset-definitions="filesetDefinitions"
        @update:model-value="updateField('pluginoptions', $event)"
      />
    </div>

    <q-expansion-item
      dense
      expand-separator
      icon="tune"
      :label="t('Advanced restore options')"
      data-testid="restore-advanced-options"
    >
      <div class="q-gutter-sm q-pt-sm">
        <q-input
          :model-value="model.when"
          :label="t('When')"
          outlined dense
          placeholder="YYYY-MM-DD HH:MM:SS"
          :hint="t('Leave empty to run immediately')"
          data-testid="restore-when"
          @update:model-value="updateField('when', $event)"
        >
          <template #append>
            <q-btn
              flat dense no-caps
              :label="t('Now')"
              @click="updateField('when', '')"
            />
          </template>
        </q-input>
        <q-input
          :model-value="model.priority"
          type="number"
          min="1"
          :label="t('Priority')"
          outlined dense
          :hint="t('Optional positive number; empty keeps the restore job default')"
          data-testid="restore-priority"
          @update:model-value="updatePriority"
        />
      </div>
    </q-expansion-item>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import PluginOptionsEditor from './PluginOptionsEditor.vue'
import {
  previewRelocatedPath,
  resolveRestoreRegexWhere,
} from '../utils/restore.js'

const props = defineProps({
  modelValue: {
    type: Object,
    required: true,
  },
  restoreClientOptions: {
    type: Array,
    default: () => [],
  },
  restoreJobOptions: {
    type: Array,
    default: () => [],
  },
  loadingClients: {
    type: Boolean,
    default: false,
  },
  loadingRestoreJobs: {
    type: Boolean,
    default: false,
  },
  disabled: {
    type: Boolean,
    default: false,
  },
  showPluginOptions: {
    type: Boolean,
    default: false,
  },
  pluginOptionsHint: {
    type: String,
    default: '',
  },
  pluginHints: {
    type: Object,
    default: () => ({}),
  },
  filesetDefinitions: {
    type: Array,
    default: () => [],
  },
  filterRestoreClientOptions: {
    type: Function,
    default: () => {},
  },
  filterRestoreJobOptions: {
    type: Function,
    default: () => {},
  },
})

const emit = defineEmits(['update:modelValue', 'browseDestination'])

const { t } = useI18n()

const model = computed(() => props.modelValue ?? {})
const effectiveRelocationMode = computed(() => model.value.relocationMode || 'where')
const effectiveRegexWhere = computed(() => resolveRestoreRegexWhere(model.value))
const relocationPreview = computed(() => previewRelocatedPath(model.value))

const relocationModeOptions = computed(() => [
  { label: t('Single destination'), value: 'where' },
  { label: t('Relocation rules'), value: 'rules' },
  { label: t('Raw regexwhere'), value: 'regex' },
])

const replaceOptions = computed(() => [
  {
    label: t('Always overwrite'),
    value: 'Always',
    description: t('Restore files even when a file already exists on the target client.'),
  },
  {
    label: t('Only overwrite older files'),
    value: 'IfNewer',
    description: t('Restore only when the backed-up file is newer than the target file.'),
  },
  {
    label: t('Only overwrite newer files'),
    value: 'IfOlder',
    description: t('Restore only when the backed-up file is older than the target file.'),
  },
  {
    label: t('Never overwrite existing files'),
    value: 'Never',
    description: t('Skip files that already exist on the target client.'),
  },
])

function updateField(field, value) {
  emit('update:modelValue', {
    ...model.value,
    [field]: value,
  })
}

function updatePriority(value) {
  const priority = value === null || value === '' ? null : Number(value)
  updateField('priority', Number.isInteger(priority) && priority > 0 ? priority : null)
}
</script>

<style scoped>
.restore-options-editor {
  display: grid;
  gap: 12px;
}
</style>
