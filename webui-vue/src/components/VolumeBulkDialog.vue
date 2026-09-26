<template>
  <q-dialog :model-value="modelValue" persistent @update:model-value="value => emit('update:modelValue', value)">
    <q-card class="volume-bulk-dialog" data-testid="volume-bulk-dialog">
      <q-card-section
        class="row items-center"
        :class="destructive ? 'bg-negative text-white' : 'panel-header'"
      >
        <q-icon :name="destructive ? 'warning' : (action?.icon ?? 'playlist_play')" class="q-mr-sm" size="sm" />
        <span class="text-subtitle1">
          {{ action ? t(action.label) : '' }}: {{ t('{count} volumes', { count: volumes.length }) }}
        </span>
        <q-space />
        <q-btn
          flat round dense icon="close" color="white"
          :disable="running"
          :title="t('Close')" :aria-label="t('Close')"
          @click="close"
        />
      </q-card-section>

      <!-- Confirmation -->
      <template v-if="phase === 'confirm'">
        <q-card-section v-if="destructive" class="q-pb-none">
          <q-banner dense rounded class="bg-negative text-white" data-testid="volume-bulk-destructive-warning">
            <template #avatar><q-icon name="report" /></template>
            <div class="text-weight-bold">{{ t('Warning: this operation is destructive and cannot be undone.') }}</div>
            <div>{{ destructiveHint }}</div>
          </q-banner>
        </q-card-section>

        <q-card-section v-if="action?.param === 'pool'" class="q-pb-none">
          <q-select
            v-model="params.pool"
            :options="poolOptions"
            :label="t('Target pool')"
            dense outlined
            data-testid="volume-bulk-pool"
          />
        </q-card-section>
        <q-card-section v-if="action?.param === 'comment'" class="q-pb-none">
          <q-input
            v-model="params.comment"
            :label="t('Comment')"
            :hint="t('Leave empty to clear the comment.')"
            :maxlength="MAX_COMMENT_LENGTH"
            dense outlined counter
            data-testid="volume-bulk-comment"
          />
        </q-card-section>

        <q-card-section>
          <div class="text-caption text-grey-7 q-mb-xs">{{ t('Commands to run') }}</div>
          <div class="console-output volume-bulk-commands" data-testid="volume-bulk-commands">
            <div v-for="entry in plan" :key="entry.volume.scopeKey">
              <template v-if="entry.command">{{ entry.command }}</template>
              <span v-else class="text-grey-6">
                # {{ entry.volume.volumename }}: {{ t('skipped') }} ({{ entry.skipReason }})
              </span>
            </div>
          </div>
          <div v-if="planError" class="text-negative q-mt-sm">{{ planError }}</div>
        </q-card-section>

        <q-card-section v-if="destructive" class="q-pt-none">
          <q-input
            v-model="confirmText"
            dense outlined
            color="negative"
            :label="t('Type {count} to confirm', { count: volumes.length })"
            data-testid="volume-bulk-confirm-input"
          />
        </q-card-section>

        <q-card-actions align="right">
          <q-btn flat no-caps :label="t('Cancel')" @click="close" />
          <q-btn
            unelevated no-caps
            :color="destructive ? 'negative' : 'primary'"
            :icon="destructive ? 'warning' : 'play_arrow'"
            :label="t('Run')"
            :disable="!canRun"
            data-testid="volume-bulk-run"
            @click="run"
          />
        </q-card-actions>
      </template>

      <!-- Progress / results -->
      <template v-else>
        <q-card-section>
          <q-linear-progress
            :value="results.length / Math.max(1, runnable.length)"
            :color="failedCount ? 'negative' : 'primary'"
            size="8px" rounded class="q-mb-sm"
          />
          <div class="text-body2 q-mb-sm" data-testid="volume-bulk-summary">
            {{ t('{done} of {total} done, {failed} failed', { done: results.length, total: runnable.length, failed: failedCount }) }}
            <span v-if="skippedCount">, {{ t('{count} skipped', { count: skippedCount }) }}</span>
          </div>
          <q-list dense bordered separator class="volume-bulk-results">
            <q-item v-for="result in results" :key="result.volume.scopeKey">
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
            data-testid="volume-bulk-close"
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
  MAX_COMMENT_LENGTH,
  buildVolumeBulkPlan,
  findVolumeBulkAction,
} from '../utils/volumeBulk.js'

const props = defineProps({
  modelValue: { type: Boolean, default: false },
  actionId: { type: String, default: '' },
  volumes: { type: Array, default: () => [] },
  poolOptions: { type: Array, default: () => [] },
  runCommand: { type: Function, required: true },
})
const emit = defineEmits(['update:modelValue', 'done'])
const { t } = useI18n()

const phase = ref('confirm')
const params = ref({ pool: null, comment: '' })
const confirmText = ref('')
const running = ref(false)
const results = ref([])

const action = computed(() => findVolumeBulkAction(props.actionId))
const destructive = computed(() => !!action.value?.destructive)

const destructiveHint = computed(() => {
  switch (props.actionId) {
    case 'purge':
      return t('All jobs and files on these volumes are removed from the catalog, regardless of retention.')
    case 'truncate':
      return t('The data on these purged volumes is erased on the storage.')
    case 'delete':
      return t('These volumes are removed from the catalog together with their job records.')
    default:
      return ''
  }
})

const planState = computed(() => {
  if (!action.value) {
    return { entries: [], error: '' }
  }
  if (action.value.param === 'pool' && !params.value.pool) {
    return {
      entries: props.volumes.map(volume => ({ volume, command: null, skipReason: t('no target pool selected') })),
      error: '',
    }
  }
  try {
    const entries = buildVolumeBulkPlan(props.actionId, props.volumes, params.value)
      .map(entry => ({
        ...entry,
        skipReason: entry.command ? '' : t('status is not Purged'),
      }))
    return { entries, error: '' }
  } catch (reason) {
    return { entries: [], error: reason?.message ?? String(reason) }
  }
})
const plan = computed(() => planState.value.entries)
const planError = computed(() => planState.value.error)
const runnable = computed(() => plan.value.filter(entry => entry.command))
const skippedCount = computed(() => plan.value.length - runnable.value.length)
const failedCount = computed(() => results.value.filter(result => !result.ok).length)

const canRun = computed(() => {
  if (running.value || !runnable.value.length || planError.value) {
    return false
  }
  if (destructive.value) {
    return confirmText.value.trim() === String(props.volumes.length)
  }
  return true
})

watch(() => props.modelValue, (open) => {
  if (open) {
    phase.value = 'confirm'
    params.value = { pool: null, comment: '' }
    confirmText.value = ''
    results.value = []
  }
})

async function run() {
  if (!canRun.value) {
    return
  }
  const entries = runnable.value
  phase.value = 'results'
  running.value = true
  results.value = []
  try {
    for (const entry of entries) {
      try {
        await props.runCommand(entry.volume, entry.command)
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
.volume-bulk-dialog {
  width: 720px;
  max-width: 95vw;
}

.volume-bulk-commands {
  max-height: 240px;
  overflow: auto;
  white-space: pre;
}

.volume-bulk-results {
  max-height: 300px;
  overflow: auto;
}
</style>
