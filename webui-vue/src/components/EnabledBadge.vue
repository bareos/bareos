<template>
  <q-badge :color="enabled ? 'positive' : 'negative'" :label="enabled ? 'Enabled' : 'Disabled'" />
</template>

<script setup>
defineProps({
  enabled: { type: Boolean, required: true },
})
</script>
