<template>
  <q-badge :color="info.color" :label="info.label" />
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  status: { type: String, required: true },
})

const statusMap = {
  T: { label: 'OK',       color: 'positive' },
  W: { label: 'Warning',  color: 'warning'  },
  f: { label: 'Failed',   color: 'negative' },
  A: { label: 'Canceled', color: 'grey'     },
  R: { label: 'Running',  color: 'info'     },
  C: { label: 'Waiting',  color: 'grey'     },
  E: { label: 'Error',    color: 'negative' },
}

const info = computed(() => statusMap[props.status] ?? { label: props.status, color: 'grey' })
</script>
