<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
-->

<template>
  <div v-if="segments.length" class="volume-status-bar" :data-testid="testid">
    <div class="row no-wrap items-stretch status-track">
      <div
        v-for="segment in segments"
        :key="segment.status"
        class="status-segment cursor-pointer"
        :class="[`bg-${segment.color}`, { 'status-segment-active': segment.active }]"
        :style="{ flexGrow: segment.count }"
        :data-testid="`${testid}-${segment.status}`"
        @click="$emit('select', segment.status)"
      >
        <span v-if="segment.share >= 0.08" class="status-segment-label">
          {{ segment.count }}
        </span>
        <q-tooltip>
          {{ segment.status }}: {{ segment.count }} ({{ Math.round(segment.share * 100) }}%)
        </q-tooltip>
      </div>
    </div>
    <div class="row items-center q-gutter-xs q-mt-xs">
      <div
        v-for="segment in segments"
        :key="`legend-${segment.status}`"
        class="row items-center no-wrap status-legend cursor-pointer"
        @click="$emit('select', segment.status)"
      >
        <span class="status-swatch" :class="`bg-${segment.color}`" />
        <span class="text-caption q-ml-xs" :class="segment.active ? 'text-weight-bold' : 'text-grey-7'">
          {{ segment.status }} {{ segment.count }}
        </span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  volumes: { type: Array, default: () => [] },
  active: { type: Array, default: () => [] },
  testid: { type: String, default: 'volume-status-bar' },
})

defineEmits(['select'])

const STATUS_COLORS = {
  Append: 'positive',
  Full: 'warning',
  Used: 'orange',
  Purged: 'grey-5',
  Recycle: 'blue-grey-4',
  Recycled: 'blue-grey-4',
  Error: 'negative',
  Cleaning: 'teal',
  'Read-Only': 'blue-grey',
  Archive: 'indigo-4',
  Disabled: 'grey-7',
}

const segments = computed(() => {
  const counts = new Map()
  for (const volume of props.volumes) {
    const status = String(volume?.volstatus ?? '').trim() || '—'
    counts.set(status, (counts.get(status) ?? 0) + 1)
  }

  const total = props.volumes.length
  if (!total) {
    return []
  }

  const activeSet = new Set(props.active)
  return [...counts.entries()]
    .sort((a, b) => b[1] - a[1])
    .map(([status, count]) => ({
      status,
      count,
      share: count / total,
      color: STATUS_COLORS[status] ?? 'info',
      active: activeSet.has(status),
    }))
})
</script>

<style scoped>
.status-track {
  height: 14px;
  border-radius: 7px;
  overflow: hidden;
}

.status-segment {
  min-width: 4px;
  display: flex;
  align-items: center;
  justify-content: center;
  transition: filter 0.15s ease;
}

.status-segment:hover,
.status-segment-active {
  filter: brightness(1.15);
  box-shadow: inset 0 0 0 2px rgba(0, 0, 0, 0.35);
}

.status-segment-label {
  font-size: 10px;
  line-height: 1;
  color: #fff;
  font-weight: 600;
}

.status-swatch {
  width: 10px;
  height: 10px;
  border-radius: 2px;
  display: inline-block;
}
</style>
