<template>
  <q-avatar
    :color="info.color"
    text-color="white"
    size="24px"
    font-size="12px"
    style="font-weight:700; cursor:default"
  >
    {{ props.level }}
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-avatar>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  level: { type: String, required: true },
})
const { t } = useI18n()

const info = computed(() =>
  ({
    F: { color: 'primary', label: t('Full') },
    I: { color: 'teal', label: t('Incremental') },
    D: { color: 'orange', label: t('Differential') },
    V: { color: 'purple', label: t('Virtual Full') },
    B: { color: 'grey', label: t('Base') },
  }[props.level] ?? { color: 'grey', label: props.level })
)
</script>
