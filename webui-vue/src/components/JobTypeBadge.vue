<template>
  <q-avatar
    :color="info.color"
    text-color="white"
    size="24px"
    font-size="12px"
    style="font-weight:700; cursor:default"
  >
    {{ info.badge }}
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-avatar>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { resolveJobTypeInfo } from '../utils/jobTypes.js'

const props = defineProps({
  type: { type: String, required: true },
})
const { t } = useI18n()

const info = computed(() => {
  const resolved = resolveJobTypeInfo(props.type)
  return {
    ...resolved,
    label: resolved.labelKey ? t(resolved.labelKey) : props.type,
  }
})
</script>
