<template>
  <q-icon :name="value ? 'check' : 'close'" :color="value ? 'positive' : 'grey'" />
</template>

<script setup>
defineProps({
  value: { type: Boolean, required: true },
})
</script>
