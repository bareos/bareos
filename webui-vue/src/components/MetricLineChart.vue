<template>
  <div
    v-if="chartPoints.length"
    class="metric-chart"
    :style="{ height: `${height}px` }"
  >
    <svg :viewBox="`0 0 ${width} ${height}`" preserveAspectRatio="none">
      <polyline
        fill="none"
        :stroke="color"
        stroke-width="3"
        stroke-linecap="round"
        stroke-linejoin="round"
        :points="polylinePoints"
      />
    </svg>
  </div>
  <div v-else class="flex flex-center text-grey text-caption" :style="{ height: `${height}px` }">
    {{ emptyLabel }}
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({
  points: {
    type: Array,
    default: () => [],
  },
  color: {
    type: String,
    default: '#1976D2',
  },
  height: {
    type: Number,
    default: 180,
  },
  emptyLabel: {
    type: String,
    default: 'No data',
  },
})

const width = 640

const chartPoints = computed(() => props.points
  .filter(point => Number.isFinite(point?.value)))

const polylinePoints = computed(() => {
  if (!chartPoints.value.length) {
    return ''
  }

  const minValue = Math.min(...chartPoints.value.map(point => point.value))
  const maxValue = Math.max(...chartPoints.value.map(point => point.value))
  const valueSpan = Math.max(maxValue - minValue, 1)
  const lastIndex = Math.max(chartPoints.value.length - 1, 1)

  return chartPoints.value.map((point, index) => {
    const x = (index / lastIndex) * width
    const y = props.height - (((point.value - minValue) / valueSpan) * (props.height - 8)) - 4
    return `${x},${y}`
  }).join(' ')
})
</script>

<style scoped>
.metric-chart {
  width: 100%;
}

.metric-chart svg {
  width: 100%;
  height: 100%;
}
</style>
