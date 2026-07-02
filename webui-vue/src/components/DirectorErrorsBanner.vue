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
  <q-banner
    v-if="visibleErrors.length"
    rounded
    dense
    class="bg-warning text-black q-mb-md"
  >
    <template #avatar>
      <q-icon name="warning" />
    </template>
    <div
      v-for="item in visibleErrors"
      :key="`${item.director}\u0000${item.message}`"
      class="row items-center q-gutter-xs q-mb-xs"
    >
      <DirectorBadge :director="item.director" size="sm" />
      <span>{{ item.message }}</span>
    </div>
  </q-banner>
</template>

<script setup>
import { computed } from 'vue'
import DirectorBadge from './DirectorBadge.vue'
import { isDirectorLoginRequiredError } from '../utils/directorErrors.js'

const props = defineProps({
  errors: {
    type: Array,
    default: () => [],
  },
})

const visibleErrors = computed(() => (
  (props.errors ?? []).filter(item => !isDirectorLoginRequiredError(item?.message))
))
</script>
