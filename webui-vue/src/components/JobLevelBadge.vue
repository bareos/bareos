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

const props = defineProps({
  level: { type: String, required: true },
})

const levelMap = {
  F: { color: 'primary', label: 'Full'         },
  I: { color: 'teal',    label: 'Incremental'  },
  D: { color: 'orange',  label: 'Differential' },
  V: { color: 'purple',  label: 'Virtual Full' },
  B: { color: 'grey',    label: 'Base'         },
}

const info = computed(() =>
  levelMap[props.level] ?? { color: 'grey', label: props.level }
)
</script>
