<template>
  <div class="row items-center no-wrap q-gutter-xs">
    <router-link
      v-if="link"
      :to="{ name: 'volume-details', params: { name } }"
      class="text-primary"
    >
      {{ name }}
    </router-link>
    <span v-else>{{ name }}</span>

    <q-icon
      v-if="hasEncryptionKey"
      name="vpn_key"
      size="xs"
      color="amber-8"
    >
      <q-tooltip>Encryption key stored in catalog</q-tooltip>
    </q-icon>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { volumeHasEncryptionKey } from '../utils/volumes.js'

const props = defineProps({
  name: {
    type: String,
    required: true,
  },
  volume: {
    type: Object,
    default: null,
  },
  link: {
    type: Boolean,
    default: true,
  },
})

const hasEncryptionKey = computed(() => volumeHasEncryptionKey(props.volume))
</script>
