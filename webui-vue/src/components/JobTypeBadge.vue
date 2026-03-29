<template>
  <q-avatar
    :color="info.color"
    text-color="white"
    size="24px"
    font-size="12px"
    style="font-weight:700; cursor:default"
  >
    {{ props.type }}
    <q-tooltip>{{ info.label }}</q-tooltip>
  </q-avatar>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  type: { type: String, required: true },
})

const typeMap = {
  B: { color: 'primary',  label: 'Backup'     },
  R: { color: 'teal',     label: 'Restore'    },
  V: { color: 'purple',   label: 'Verify'     },
  A: { color: 'grey',     label: 'Admin'      },
  D: { color: 'orange',   label: 'Diagnostic' },
  C: { color: 'cyan-8',   label: 'Copy'       },
  M: { color: 'deep-purple', label: 'Migration' },
}

const info = computed(() =>
  typeMap[props.type] ?? { color: 'grey', label: props.type }
)
</script>
