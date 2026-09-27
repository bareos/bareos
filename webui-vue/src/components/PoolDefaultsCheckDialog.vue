<!--
  bareos-webui - Bareos Web-Frontend

  @link      https://github.com/bareos/bareos
  @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG
  @license   GNU Affero General Public License (http://www.gnu.org/licenses/)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->

<template>
  <q-dialog
    :model-value="modelValue"
    persistent
    @update:model-value="value => emit('update:modelValue', value)"
  >
    <q-card class="pool-defaults-dialog" data-testid="pool-defaults-dialog">
      <q-card-section class="panel-header row items-center">
        <q-icon name="rule" class="q-mr-sm" size="sm" />
        <span class="text-subtitle1">{{ t('Check volume settings against pool defaults') }}</span>
        <q-space />
        <q-btn
          flat round dense icon="close" color="white"
          :disable="running"
          :title="t('Close')" :aria-label="t('Close')"
          @click="close"
        />
      </q-card-section>

      <template v-if="phase === 'confirm'">
        <q-card-section v-if="!drift.length" data-testid="pool-defaults-clean">
          <q-banner dense rounded class="bg-green-1">
            <template #avatar><q-icon name="check_circle" color="positive" /></template>
            {{ t('All {count} volumes match the settings of their pool.', { count: volumes.length }) }}
          </q-banner>
        </q-card-section>

        <template v-else>
          <q-card-section class="q-pb-none">
            <q-banner dense rounded class="bg-orange-1">
              <template #avatar><q-icon name="rule" color="orange-9" /></template>
              {{ t('{deviating} of {total} volumes deviate from their pool.', {
                deviating: drift.length,
                total: volumes.length,
              }) }}
            </q-banner>
          </q-card-section>

          <q-card-section class="q-pa-none">
            <q-table
              v-model:selected="selected"
              :rows="drift"
              :columns="columns"
              row-key="key"
              selection="multiple"
              dense flat
              :pagination="{ rowsPerPage: 0 }"
              hide-bottom
              class="pool-defaults-table"
            >
              <template #body-cell-differences="props">
                <q-td :props="props">
                  <div
                    v-for="entry in props.row.differences"
                    :key="entry.field.id"
                    class="pool-defaults-difference"
                  >
                    <span class="text-grey-7">{{ t(entry.field.label) }}:</span>
                    <span class="text-negative q-ml-xs">{{ formatValue(entry.field, entry.volumeValue) }}</span>
                    <q-icon name="arrow_right_alt" size="16px" class="q-mx-xs" />
                    <span class="text-positive">{{ formatValue(entry.field, entry.poolValue) }}</span>
                  </div>
                </q-td>
              </template>
            </q-table>
          </q-card-section>

          <q-card-section class="q-pt-sm">
            <div class="text-caption text-grey-7">{{ t('Commands to run') }}</div>
            <div class="console-output pool-defaults-commands" data-testid="pool-defaults-commands">
              <div v-for="entry in selected" :key="entry.key">
                {{ buildApplyPoolDefaultsCommand(entry.volume.volumename) }}
              </div>
            </div>
          </q-card-section>
        </template>

        <q-card-actions align="right">
          <q-btn flat no-caps :label="t('Close')" @click="close" />
          <q-btn
            v-if="drift.length"
            unelevated no-caps color="primary" icon="play_arrow"
            :label="t('Apply pool settings')"
            :disable="!selected.length || running"
            data-testid="pool-defaults-apply"
            @click="run"
          />
        </q-card-actions>
      </template>

      <template v-else>
        <q-card-section>
          <q-linear-progress
            :value="results.length / Math.max(1, selected.length)"
            :color="failedCount ? 'negative' : 'primary'"
            size="8px" rounded class="q-mb-sm"
          />
          <div class="text-body2 q-mb-sm" data-testid="pool-defaults-summary">
            {{ t('{done} of {total} done, {failed} failed', {
              done: results.length,
              total: selected.length,
              failed: failedCount,
            }) }}
          </div>
          <q-list dense bordered separator class="pool-defaults-results">
            <q-item v-for="result in results" :key="result.volume.volumename">
              <q-item-section avatar>
                <q-icon :name="result.ok ? 'check_circle' : 'error'" :color="result.ok ? 'positive' : 'negative'" />
              </q-item-section>
              <q-item-section>
                <q-item-label>{{ result.volume.volumename }}</q-item-label>
                <q-item-label v-if="!result.ok" caption class="text-negative">{{ result.message }}</q-item-label>
              </q-item-section>
            </q-item>
          </q-list>
        </q-card-section>
        <q-card-actions align="right">
          <q-btn
            unelevated no-caps color="primary" :label="t('Close')"
            :disable="running"
            data-testid="pool-defaults-close"
            @click="close"
          />
        </q-card-actions>
      </template>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import {
  buildApplyPoolDefaultsCommand,
  collectPoolDefaultDrift,
  formatPoolDefaultValue,
} from '../utils/poolDefaults.js'

const props = defineProps({
  modelValue: { type: Boolean, default: false },
  volumes: { type: Array, default: () => [] },
  pools: { type: Array, default: () => [] },
  showDirector: { type: Boolean, default: false },
  runCommand: { type: Function, required: true },
})
const emit = defineEmits(['update:modelValue', 'done'])
const { t } = useI18n()

const phase = ref('confirm')
const selected = ref([])
const running = ref(false)
const results = ref([])

const drift = computed(() => (
  collectPoolDefaultDrift(props.volumes, props.pools).map(entry => ({
    ...entry,
    key: `${entry.volume.director ?? ''}\u0000${entry.volume.volumename}`,
  }))
))

const failedCount = computed(() => results.value.filter(result => !result.ok).length)

const columns = computed(() => [
  ...(props.showDirector ? [{
    name: 'director', label: t('Director'), align: 'left',
    field: row => row.volume.director,
  }] : []),
  {
    name: 'volumename', label: t('Volume Name'), align: 'left',
    field: row => row.volume.volumename,
  },
  { name: 'pool', label: t('Pool'), align: 'left', field: row => row.pool.name },
  {
    name: 'differences', label: t('Volume value → pool value'), align: 'left',
    field: 'differences',
  },
])

function formatValue(field, value) {
  return formatPoolDefaultValue(field, value, t)
}

watch(() => props.modelValue, (open) => {
  if (open) {
    phase.value = 'confirm'
    running.value = false
    results.value = []
    selected.value = [...drift.value]
  }
})

async function run() {
  if (running.value || !selected.value.length) {
    return
  }

  const entries = [...selected.value]
  phase.value = 'results'
  running.value = true
  results.value = []
  try {
    for (const entry of entries) {
      try {
        await props.runCommand(
          entry.volume,
          buildApplyPoolDefaultsCommand(entry.volume.volumename)
        )
        results.value.push({ volume: entry.volume, ok: true, message: '' })
      } catch (reason) {
        results.value.push({
          volume: entry.volume,
          ok: false,
          message: reason?.message ?? String(reason),
        })
      }
    }
  } finally {
    running.value = false
    emit('done', results.value)
  }
}

function close() {
  if (running.value) {
    return
  }
  emit('update:modelValue', false)
}
</script>

<style scoped>
.pool-defaults-dialog {
  width: 860px;
  max-width: 95vw;
}

.pool-defaults-table {
  max-height: 360px;
}

.pool-defaults-difference {
  white-space: nowrap;
}

.pool-defaults-commands {
  max-height: 160px;
  overflow: auto;
  white-space: pre;
}

.pool-defaults-results {
  max-height: 300px;
  overflow: auto;
}
</style>
