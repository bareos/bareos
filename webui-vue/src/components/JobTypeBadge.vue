<template>
  <q-avatar
    :color="info.color"
    text-color="white"
    size="24px"
    font-size="12px"
    style="font-weight:700; cursor:default"
  >
    {{ props.type }}
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-avatar>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  type: { type: String, required: true },
})
const { t } = useI18n()

const info = computed(() =>
  ({
    B: { color: 'primary', label: t('Backup') },
    R: { color: 'teal', label: t('Restore') },
    V: { color: 'purple', label: t('Verify') },
    A: { color: 'grey', label: t('Admin') },
    D: { color: 'orange', label: t('Diagnostic') },
    C: { color: 'cyan-8', label: t('Copy') },
    M: { color: 'deep-purple', label: t('Migration') },
  }[props.type] ?? { color: 'grey', label: props.type })
)
</script>
