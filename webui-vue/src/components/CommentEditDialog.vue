<template>
  <q-dialog :model-value="modelValue" @update:model-value="value => emit('update:modelValue', value)" @show="reset">
    <q-card style="min-width:min(520px, 90vw)" data-testid="comment-edit-dialog">
      <q-card-section class="panel-header row items-center">
        <q-icon name="comment" class="q-mr-sm" size="sm" />
        <span class="text-subtitle1">{{ title }}</span>
      </q-card-section>
      <q-card-section>
        <q-input
          v-model="text"
          autofocus
          dense outlined counter
          :label="t('Comment')"
          :hint="t('Leave empty to clear the comment.')"
          :maxlength="MAX_COMMENT_LENGTH"
          :disable="saving"
          data-testid="comment-edit-input"
          @keyup.enter="submit"
        />
        <div v-if="error" class="text-negative q-mt-sm" data-testid="comment-edit-error">{{ error }}</div>
      </q-card-section>
      <q-card-actions align="right">
        <q-btn flat no-caps :label="t('Cancel')" :disable="saving" v-close-popup />
        <q-btn
          unelevated no-caps color="primary"
          :label="t('Save')" :loading="saving"
          data-testid="comment-edit-save"
          @click="submit"
        />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'
import { MAX_COMMENT_LENGTH, sanitizeComment } from '../utils/volumeBulk.js'

const props = defineProps({
  modelValue: { type: Boolean, default: false },
  title: { type: String, default: '' },
  comment: { type: String, default: '' },
  // async (comment) => void; a rejection keeps the dialog open.
  save: { type: Function, required: true },
})
const emit = defineEmits(['update:modelValue'])
const { t } = useI18n()

const text = ref('')
const saving = ref(false)
const error = ref('')

function reset() {
  text.value = props.comment ?? ''
  error.value = ''
  saving.value = false
}

async function submit() {
  if (saving.value) return
  saving.value = true
  error.value = ''
  try {
    await props.save(sanitizeComment(text.value))
    emit('update:modelValue', false)
  } catch (e) {
    error.value = e?.message ?? String(e)
  } finally {
    saving.value = false
  }
}
</script>
