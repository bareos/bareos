<template>
  <q-avatar :color="color" text-color="white" size="24px"
            style="font-size:11px; font-weight:700; border-radius:50%">
    {{ letter }}
  </q-avatar>
</template>

<script setup>
const props = defineProps({ type: { type: String, default: '' } })

const MAP = {
  Backup:    { letter: 'B', color: 'primary'     },
  Scratch:   { letter: 'S', color: 'grey-6'      },
  Archive:   { letter: 'A', color: 'deep-purple' },
  Copy:      { letter: 'C', color: 'teal'        },
  Migration: { letter: 'M', color: 'orange'      },
}

const entry  = MAP[props.type] ?? { letter: props.type?.[0]?.toUpperCase() ?? '?', color: 'grey-6' }
const letter = entry.letter
const color  = entry.color
</script>
