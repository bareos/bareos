<template>
  <div class="text-center">
    <div class="stat-number" :class="`text-${color}`">{{ value }}</div>
    <div class="stat-label">{{ label }}</div>
  </div>
</template>

<script setup>
defineProps({
  value:  { type: [String, Number], required: true },
  label:  { type: String,           required: true },
  color:  { type: String,           default: 'dark' },
})
</script>
