<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
  <div class="row items-center q-gutter-sm q-mb-md text-caption">
    <span :class="{ 'text-negative': snapshot?.stale || age?.stale }"
          :title="snapshot?.calculated_at || undefined">
      {{ t('Accounting snapshot age') }}:
      {{ age ? t('{hours}h {minutes}m', duration) : t('Unavailable') }}
    </span>
    <slot />
  </div>
</template>

<script setup>
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { subscriptionSnapshotAge } from '../utils/subscription.js'

const props = defineProps({
  snapshot: { type: Object, default: null },
  reportTime: { type: String, default: '' },
})
const { t } = useI18n()
const receivedAt = ref(Date.now())
const now = ref(Date.now())
let timer
watch(() => [props.snapshot, props.reportTime], () => {
  receivedAt.value = Date.now()
  now.value = receivedAt.value
})
onMounted(() => { timer = setInterval(() => { now.value = Date.now() }, 60000) })
onUnmounted(() => clearInterval(timer))
const age = computed(() => subscriptionSnapshotAge(
  props.snapshot, props.reportTime, now.value - receivedAt.value,
))
const duration = computed(() => {
  const minutes = Math.floor((age.value?.milliseconds ?? 0) / 60000)
  return { hours: Math.floor(minutes / 60), minutes: minutes % 60 }
})
</script>
