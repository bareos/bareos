<template>
  <q-badge :color="enabled ? 'positive' : 'negative'" :label="enabled ? t('Enabled') : t('Disabled')" />
</template>

<script setup>
import { useI18n } from 'vue-i18n'

defineProps({
  enabled: { type: Boolean, required: true },
})

const { t } = useI18n()
</script>
