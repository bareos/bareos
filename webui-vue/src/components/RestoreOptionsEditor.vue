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

    <q-input
      v-if="advancedMode"
      :model-value="model.where"
      :label="t('Restore to (Where)')"
      outlined dense
      placeholder="/ or /tmp/bareos-restores"
      :hint="effectiveRelocationMode === 'none'
        ? t('Enter the destination path on the selected restore client.')
        : t('File Relocation is active and replaces this Where destination.')"
      :disable="effectiveRelocationMode !== 'none'"
      data-testid="restore-where"
      @update:model-value="updateField('where', $event)"
    >
      <template #append>
        <q-btn
          flat
          dense
          no-caps
          icon="travel_explore"
          :label="t('Browse')"
          :disable="effectiveRelocationMode !== 'none'"
          data-testid="restore-browse-destination"
          @click="$emit('browseDestination')"
        />
      </template>
    </q-input>

    <q-card v-if="advancedMode" flat bordered class="relocation-settings-card">
      <q-card-section class="q-gutter-sm">
        <div>
          <div class="text-subtitle2">{{ t('File Relocation') }}</div>
          <div class="text-caption text-grey-7">
            {{ t('For simple restore roots, use') }}
            <code>Where</code>.
            {{ t('File Relocation rewrites stored paths with') }}
            <code>RegexWhere</code>.
          </div>
        </div>
      </q-card-section>

      <q-tabs
        :model-value="effectiveRelocationMode"
        class="relocation-mode-tabs"
        active-color="primary"
        indicator-color="primary"
        align="left"
        dense
        mobile-arrows
        narrow-indicator
        outside-arrows
        data-testid="restore-relocation-mode"
        @update:model-value="selectRelocationMode"
      >
        <q-tab
          v-for="option in relocationModeOptions"
          :key="option.value"
          :name="option.value"
          :label="option.label"
          no-caps
        />
      </q-tabs>

      <q-separator />
      <q-card-section
        class="q-gutter-sm relocation-mode-settings"
      >
        <div>
          <div class="text-subtitle2">{{ selectedRelocationModeLabel }}</div>
          <div class="text-caption text-grey-7">
            {{ selectedRelocationModeDescription }}
          </div>
          <code class="relocation-mode-example">
            {{ selectedRelocationModeExample }}
          </code>
        </div>

        <template v-if="effectiveRelocationMode !== 'none'">
          <template v-if="effectiveRelocationMode === 'windows-drive'">
            <div class="row q-col-gutter-sm">
              <div class="col-12 col-sm-6">
                <q-input
                  :model-value="model.sourceDrive"
                  :label="t('Source drive')"
                  outlined dense
                  placeholder="C:"
                  data-testid="restore-source-drive"
                  @update:model-value="updateField('sourceDrive', $event)"
                />
              </div>
              <div class="col-12 col-sm-6">
                <q-input
                  :model-value="model.targetDrive"
                  :label="t('Target drive')"
                  outlined dense
                  placeholder="D:"
                  data-testid="restore-target-drive"
                  @update:model-value="updateField('targetDrive', $event)"
                />
              </div>
            </div>
          </template>
          <template v-else-if="effectiveRelocationMode === 'replace-prefix'">
            <q-input
              :model-value="model.sourcePrefix"
              :label="t('Source prefix')"
              outlined dense
              placeholder="C:/Users"
              data-testid="restore-source-prefix"
              @update:model-value="updateField('sourcePrefix', $event)"
            />
            <q-input
              :model-value="model.targetPrefix"
              :label="t('Target prefix')"
              outlined dense
              placeholder="D:/Users"
              data-testid="restore-target-prefix"
              @update:model-value="updateField('targetPrefix', $event)"
            />
          </template>
          <template v-else-if="effectiveRelocationMode === 'strip-prefix'">
            <q-input
              :model-value="model.stripPrefix"
              :label="t('Strip prefix')"
              outlined dense
              placeholder="/home"
              data-testid="restore-strip-prefix"
              @update:model-value="updateField('stripPrefix', $event)"
            />
          </template>
          <template v-else-if="effectiveRelocationMode === 'add-suffix'">
            <q-input
              :model-value="model.addSuffix"
              :label="t('Add suffix')"
              outlined dense
              placeholder=".restored"
              data-testid="restore-add-suffix"
              @update:model-value="updateField('addSuffix', $event)"
            />
          </template>
          <template v-else-if="effectiveRelocationMode === 'custom-regex'">
            <q-input
              :model-value="model.regexWhere"
              :label="t('Custom RegexWhere')"
              outlined dense
              placeholder="!^C:/Users/!D:/Users/!i"
              :hint="t('Advanced Bareos regexwhere expression. This replaces Where.')"
              data-testid="restore-regexwhere"
              @update:model-value="updateField('regexWhere', $event)"
            />
          </template>
          <q-input
            :model-value="model.relocationSample"
            :label="t('Example path')"
            outlined dense
            :placeholder="DEFAULT_RELOCATION_SAMPLE_PATH"
            data-testid="restore-relocation-sample"
            @update:model-value="updateField('relocationSample', $event)"
          />
          <div class="bg-grey-1 rounded-borders q-pa-sm" data-testid="restore-destination-preview">
            <div class="text-caption text-grey-7">{{ t('Preview') }}</div>
            <div class="relocation-preview-line">
              <span class="text-weight-medium">{{ t('Result') }}:</span>
              <code>{{ relocationPreview || t('(enter an example path)') }}</code>
            </div>
            <div class="text-caption text-grey-7 relocation-preview-line">
              <span class="text-weight-medium">RegexWhere:</span>
              <code>{{ effectiveRegexWhere || t('(empty)') }}</code>
            </div>
          </div>
        </template>
      </q-card-section>
    </q-card>

    <q-select
      v-if="advancedMode"
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

    <div v-if="advancedMode && showPluginOptions" data-testid="restore-plugin-options">
      <div class="text-caption text-grey-7 q-mb-xs">{{ pluginOptionsHint }}</div>
      <PluginOptionsEditor
        :model-value="model.pluginoptions"
        :plugin-hints="pluginHints"
        :fileset-definitions="filesetDefinitions"
        @update:model-value="updateField('pluginoptions', $event)"
      />
    </div>

    <q-expansion-item
      v-if="advancedMode"
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
          :hint="t('Leave empty to run immediately, or use the calendar and clock picker.')"
          data-testid="restore-when"
          @update:model-value="updateField('when', $event)"
        >
          <template #append>
            <q-icon name="event" class="cursor-pointer">
              <q-popup-proxy @before-show="prepareWhenPicker">
                <div class="q-pa-md column q-gutter-md">
                  <q-date
                    v-model="whenPickerValue"
                    mask="YYYY-MM-DD HH:mm:ss"
                  />
                  <q-time
                    v-model="whenPickerValue"
                    mask="YYYY-MM-DD HH:mm:ss"
                    format24h
                    with-seconds
                  />
                  <div class="row justify-end q-gutter-sm">
                    <q-btn
                      flat
                      no-caps
                      :label="t('Clear')"
                      v-close-popup
                      @click="clearWhen"
                    />
                    <q-btn flat no-caps :label="t('Cancel')" v-close-popup />
                    <q-btn
                      color="primary"
                      no-caps
                      :label="t('OK')"
                      v-close-popup
                      @click="applyWhenPicker"
                    />
                  </div>
                </div>
              </q-popup-proxy>
            </q-icon>
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
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import PluginOptionsEditor from './PluginOptionsEditor.vue'
import {
  formatRunWhenPickerDate,
  resolveRunWhenPickerValue,
} from '../utils/jobs.js'
import {
  DEFAULT_RELOCATION_SAMPLE_PATH,
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
  advancedMode: {
    type: Boolean,
    default: true,
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
const whenPickerValue = ref(formatRunWhenPickerDate(new Date()))
const effectiveRelocationMode = computed(() => {
  const mode = model.value.relocationMode || 'none'
  return mode === 'where' ? 'none' : mode
})
const effectiveRegexWhere = computed(() => resolveRestoreRegexWhere(model.value))
const relocationPreview = computed(() => previewRelocatedPath(model.value))

const relocationModeOptions = computed(() => [
  {
    label: t('No relocation'),
    value: 'none',
    description: t('Use the Where destination without rewriting stored paths.'),
    example: t('Example: restore below /tmp/bareos-restores'),
  },
  {
    label: t('Windows drive remap'),
    value: 'windows-drive',
    description: t('Move paths from one Windows drive letter to another.'),
    example: 'C:/Users -> D:/Users',
  },
  {
    label: t('Replace prefix'),
    value: 'replace-prefix',
    description: t('Replace one leading path prefix with another prefix.'),
    example: 'C:/Users -> D:/Users',
  },
  {
    label: t('Strip prefix'),
    value: 'strip-prefix',
    description: t('Remove a leading path prefix from restored files.'),
    example: '/home/alice -> /alice',
  },
  {
    label: t('Add suffix'),
    value: 'add-suffix',
    description: t('Append a suffix to every restored file path.'),
    example: 'report.txt -> report.txt.restored',
  },
  {
    label: t('Custom RegexWhere'),
    value: 'custom-regex',
    description: t('Enter an advanced Bareos RegexWhere expression.'),
    example: '!^C:/Users/!D:/Users/!i',
  },
])
const selectedRelocationModeLabel = computed(() => (
  relocationModeOptions.value.find(option => option.value === effectiveRelocationMode.value)?.label
    ?? t('File Relocation')
))
const selectedRelocationModeDescription = computed(() => (
  relocationModeOptions.value.find(option => option.value === effectiveRelocationMode.value)?.description
    ?? ''
))
const selectedRelocationModeExample = computed(() => (
  relocationModeOptions.value.find(option => option.value === effectiveRelocationMode.value)?.example
    ?? ''
))

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

function selectRelocationMode(value) {
  updateField('relocationMode', value)
}

function prepareWhenPicker() {
  whenPickerValue.value = resolveRunWhenPickerValue(model.value.when)
}

function applyWhenPicker() {
  updateField('when', whenPickerValue.value)
}

function clearWhen() {
  updateField('when', '')
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

.relocation-mode-tabs {
  background: rgba(0, 0, 0, 0.03);
}

.relocation-mode-example {
  display: inline-block;
  margin-top: 4px;
  white-space: normal;
}

.relocation-preview-line {
  align-items: baseline;
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
</style>
