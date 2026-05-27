<template>
  <q-avatar
    :color="info.color"
    text-color="white"
    size="24px"
    font-size="12px"
    style="font-weight:700; user-select:none"
  >
    {{ info.badge }}
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-avatar>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { resolveJobLevelInfo } from '../utils/jobLevels.js'

const props = defineProps({
  level: { type: String, required: true },
})
const { t } = useI18n()

const info = computed(() => {
  const resolved = resolveJobLevelInfo(props.level)
  return {
    ...resolved,
    label: resolved.labelKey ? t(resolved.labelKey) : props.level,
  }
})
</script>
