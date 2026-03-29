<template>
  <q-icon :name="info.icon" :color="info.color" size="18px">
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-icon>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  level: { type: String, required: true },
})

const levelMap = {
  F: { icon: 'save',       color: 'primary', label: 'Full'         },
  I: { icon: 'add',        color: 'teal',    label: 'Incremental'  },
  D: { icon: 'difference', color: 'orange',  label: 'Differential' },
  V: { icon: 'sync',       color: 'purple',  label: 'Virtual Full' },
  B: { icon: 'hub',        color: 'grey',    label: 'Base'         },
}

const info = computed(() =>
  levelMap[props.level] ?? { icon: 'help_outline', color: 'grey', label: props.level }
)
</script>
