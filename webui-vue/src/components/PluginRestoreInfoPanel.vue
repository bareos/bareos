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
  <q-banner
    v-if="pluginRestoreInfo"
    dense
    rounded
    class="bg-blue-1 text-grey-9"
    data-testid="restore-plugin-info"
  >
    <div class="text-weight-medium">{{ t('Plugin restore detected') }}</div>
    <div class="text-caption q-mt-xs">
      {{
        pluginRestoreInfo.usesMergedJobs
          ? t('One of the related backup jobs in this restore uses a plugin.')
          : t('The selected backup job uses a plugin.')
      }}
    </div>
    <div class="q-mt-sm text-caption">
      <div>
        <span class="text-grey-7">{{ t('Plugin') }}:</span>
        <span class="plugin-restore-details__value">
          {{ pluginRestoreInfo.pluginNames.join(', ') }}
        </span>
      </div>
      <div>
        <span class="text-grey-7">{{ t('Fileset') }}:</span>
        <span class="plugin-restore-details__value">
          {{ pluginRestoreInfo.filesetNames.join(', ') }}
        </span>
      </div>
      <div v-if="pluginRestoreInfo.optionKeys.length">
        <span class="text-grey-7">{{ t('Options seen in backup definition') }}:</span>
        <span class="plugin-restore-details__value">
          {{ pluginRestoreInfo.optionKeys.join(', ') }}
        </span>
      </div>
    </div>
    <div class="text-caption q-mt-sm">
      {{ t('Bareos does not expose which restore plugin options are required or optional. Use the detected plugin and backup definition below as a guide and consult the plugin documentation if needed.') }}
    </div>
    <div class="q-mt-sm">
      <q-btn
        flat
        dense
        no-caps
        color="primary"
        icon="menu_book"
        :label="t('Show all known plugin hints')"
        data-testid="restore-all-plugin-hints"
        @click="allPluginHintsDialog = true"
      />
    </div>
    <div
      v-for="pluginHint in pluginHints"
      :key="pluginHint.id"
      class="q-mt-sm"
    >
      <div class="text-caption text-weight-medium">
        {{ t('Known options for') }} {{ pluginHint.displayName }}
      </div>
      <div
        class="text-caption q-mt-xs"
        :class="pluginHint.supportLevel === 'contrib' ? 'text-orange-8' : 'text-grey-7'"
      >
        {{ pluginSupportLabel(pluginHint) }}
      </div>
      <div
        v-if="pluginHint.supportLevel === 'contrib'"
        class="text-caption text-orange-8 q-mt-xs"
      >
        {{ t('Contrib plugins are community contributions and are not supported.') }}
      </div>
      <div
        v-if="pluginHint.manualUrl"
        class="text-caption q-mt-xs"
      >
        <a
          :href="pluginHint.manualUrl"
          target="_blank"
          rel="noopener noreferrer"
          class="text-primary"
        >
          {{ t('Open manual page') }}
        </a>
      </div>
      <div
        v-if="pluginHint.note"
        class="text-caption text-grey-8 q-mt-xs"
      >
        {{ pluginHint.note }}
      </div>
      <div class="text-caption text-grey-7 q-mb-xs">
        {{ t('Best-effort hints derived from Bareos manuals, shipped examples and plugin README files.') }}
      </div>
      <q-markup-table
        dense
        flat
        bordered
        class="plugin-restore-details__table"
      >
        <thead>
          <tr>
            <th>{{ t('Option') }}</th>
            <th>{{ t('Status') }}</th>
            <th>{{ t('Source') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr
            v-for="option in pluginHint.options"
            :key="`${pluginHint.id}:${option.name}`"
          >
            <td>
              <div class="text-mono">{{ option.name }}</div>
              <div
                v-if="option.description"
                class="text-caption text-grey-7 plugin-restore-details__description"
              >
                {{ option.description }}
              </div>
            </td>
            <td>{{ pluginOptionStatusLabel(option.status) }}</td>
            <td>{{ pluginOptionSourceLabel(option.source) }}</td>
          </tr>
        </tbody>
      </q-markup-table>
    </div>
    <div class="q-mt-sm q-gutter-xs">
      <div
        v-for="definition in pluginRestoreInfo.definitions"
        :key="`${definition.filesetName}:${definition.raw}`"
        class="plugin-restore-details__definition"
      >
        <div class="text-caption text-grey-7">
          {{ t('Backup plugin definition') }} — {{ definition.filesetName }}
        </div>
        <code class="plugin-restore-details__code">{{ definition.raw }}</code>
      </div>
    </div>
  </q-banner>

  <q-dialog
    v-model="allPluginHintsDialog"
    :maximized="$q.screen.lt.md"
    transition-show="slide-up"
    transition-hide="slide-down"
  >
    <q-card class="plugin-hints-dialog">
      <q-card-section class="panel-header row items-center">
        <span class="plugin-hints-dialog__title">
          {{ t('All known plugin hints') }}
        </span>
        <q-space />
        <q-btn flat round dense icon="close" color="white" v-close-popup />
      </q-card-section>
      <q-card-section class="q-gutter-md plugin-hints-dialog__body">
        <div class="text-caption">
          {{ t('These hints are derived from Bareos manuals, shipped examples and plugin README files. They are frontend guidance only and may be incomplete.') }}
        </div>
        <div class="text-caption text-orange-8">
          {{ t('Entries marked as contrib plugins are community contributions and are not supported.') }}
        </div>
        <div
          v-for="pluginHint in allPluginHints"
          :key="pluginHint.id"
          class="plugin-hints-dialog__section"
        >
          <div class="text-subtitle2">{{ pluginHint.displayName }}</div>
          <div
            class="text-caption q-mt-xs"
            :class="pluginHint.supportLevel === 'contrib' ? 'text-orange-8' : 'text-grey-7'"
          >
            {{ pluginSupportLabel(pluginHint) }}
          </div>
          <div
            v-if="pluginHint.supportLevel === 'contrib'"
            class="text-caption text-orange-8 q-mt-xs"
          >
            {{ t('Contrib plugins are community contributions and are not supported.') }}
          </div>
          <div
            v-if="pluginHint.manualUrl"
            class="text-caption q-mt-xs"
          >
            <a
              :href="pluginHint.manualUrl"
              target="_blank"
              rel="noopener noreferrer"
              class="text-primary"
            >
              {{ t('Open manual page') }}
            </a>
          </div>
          <div
            v-if="pluginHint.note"
            class="text-caption text-grey-8 q-mt-xs"
          >
            {{ pluginHint.note }}
          </div>
          <div v-if="pluginHint.example" class="text-caption text-grey-7 q-mt-xs">
            {{ t('Example') }}: <code>{{ pluginHint.example }}</code>
          </div>
          <q-markup-table
            dense
            flat
            bordered
            class="plugin-restore-details__table q-mt-sm"
          >
            <thead>
              <tr>
                <th>{{ t('Option') }}</th>
                <th>{{ t('Status') }}</th>
                <th>{{ t('Source') }}</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="option in pluginHint.options"
                :key="`${pluginHint.id}:${option.name}`"
              >
                <td>
                  <div class="text-mono">{{ option.name }}</div>
                  <div
                    v-if="option.description"
                    class="text-caption text-grey-7 plugin-restore-details__description"
                  >
                    {{ option.description }}
                  </div>
                </td>
                <td>{{ pluginOptionStatusLabel(option.status) }}</td>
                <td>{{ pluginOptionSourceLabel(option.source) }}</td>
              </tr>
            </tbody>
          </q-markup-table>
        </div>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat :label="t('Close')" v-close-popup />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { computed, ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { getAllRestorePluginHints } from '../utils/restore.js'

defineProps({
  pluginRestoreInfo: {
    type: Object,
    default: null,
  },
  pluginHints: {
    type: Array,
    default: () => [],
  },
})

const { t } = useI18n()
const allPluginHintsDialog = ref(false)
const allPluginHints = computed(() => getAllRestorePluginHints())

function pluginOptionStatusLabel(status) {
  if (status === 'required') {
    return t('Required')
  }

  if (status === 'optional') {
    return t('Optional')
  }

  return t('Known key')
}

function pluginSupportLabel(pluginHint) {
  if (pluginHint?.supportLevel === 'contrib') {
    return t('Contrib plugin')
  }

  return t('Bareos plugin')
}

function pluginOptionSourceLabel(source) {
  if (source === 'plugin-doc') {
    return t('Bareos manual')
  }

  if (source === 'plugin-readme') {
    return t('Plugin README')
  }

  if (source === 'plugin-source') {
    return t('Plugin source')
  }

  return t('Shipped example')
}
</script>

<style scoped>
.plugin-hints-dialog {
  width: min(960px, 96vw);
  max-width: 960px;
}

.plugin-hints-dialog__title {
  min-width: 0;
  overflow-wrap: anywhere;
}

.plugin-hints-dialog__body {
  max-height: min(75vh, 900px);
  overflow: auto;
}

.plugin-hints-dialog__section {
  padding-bottom: 0.25rem;
}

.plugin-restore-details__value {
  margin-left: 0.25rem;
}

.plugin-restore-details__definition {
  padding: 0.5rem 0.75rem;
  border-radius: 4px;
  background: rgba(0, 0, 0, 0.04);
}

.plugin-restore-details__description {
  margin-top: 0.15rem;
  max-width: 34rem;
}

.plugin-restore-details__table {
  background: rgba(255, 255, 255, 0.85);
}

.plugin-restore-details__table :deep(th),
.plugin-restore-details__table :deep(td) {
  padding: 4px 8px;
}

.plugin-restore-details__code {
  display: block;
  margin-top: 0.25rem;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}
</style>
