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
  <q-chip
    :dense="dense"
    :square="square"
    :size="size"
    :icon="icon || undefined"
    :clickable="clickable"
    :removable="removable"
    :style="chipStyle"
    @remove="$emit('remove')"
  >
    <slot>{{ director }}</slot>
  </q-chip>
</template>

<script setup>
import { computed } from 'vue'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildDirectorOptions } from '../utils/director.js'
import { resolveDirectorColors } from '../utils/directorColors.js'

const props = defineProps({
  director: {
    type: String,
    required: true,
  },
  icon: {
    type: String,
    default: null,
  },
  dense: {
    type: Boolean,
    default: true,
  },
  square: {
    type: Boolean,
    default: true,
  },
  clickable: {
    type: Boolean,
    default: false,
  },
  removable: {
    type: Boolean,
    default: false,
  },
  size: {
    type: String,
    default: null,
  },
})

defineEmits(['remove'])

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()

const directorNames = computed(() => buildDirectorOptions({
  availableDirectors: director.availableDirectors,
  selectedDirectors: settings.selectedDirectors,
  currentDirector: auth.user?.director,
  fallbackDirector: settings.directorName,
}).map(option => option.value))

const chipStyle = computed(() => {
  const colors = resolveDirectorColors(props.director, directorNames.value)
  return {
    backgroundColor: colors.background,
    color: colors.text,
    border: `1px solid ${colors.border}`,
  }
})
</script>
