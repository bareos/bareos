<template>
  <q-badge :color="info.color" :label="info.label" />
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'

const props = defineProps({
  status: { type: String, required: true },
})
const { t } = useI18n()

const info = computed(() => ({
  T:    { label: 'OK', color: 'positive' },
  OK:   { label: 'OK', color: 'positive' },
  W:    { label: t('Warning'), color: 'warning' },
  f:    { label: t('Failed'), color: 'negative' },
  A:    { label: t('Canceled'), color: 'grey' },
  R:    { label: t('Running'), color: 'info' },
  C:    { label: t('Waiting'), color: 'grey' },
  E:    { label: t('Error'), color: 'negative' },
}[props.status] ?? { label: props.status, color: 'grey' }))
</script>
