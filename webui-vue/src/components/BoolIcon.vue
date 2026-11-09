<template>
  <q-icon
    :name="value ? 'check' : 'close'"
    :color="value ? 'positive' : 'grey'"
    role="img"
    :aria-label="value ? t('Yes') : t('No')"
  />
</template>

<script setup>
import { useI18n } from 'vue-i18n'

defineProps({
  value: { type: Boolean, required: true },
})
const { t } = useI18n()
</script>
