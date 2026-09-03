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
  <q-item class="q-py-sm">
    <q-item-section>
      <q-item-label>
        <a
          href="#"
          class="text-primary text-weight-medium"
          @click.prevent="$emit('open-details', job)"
        >
          {{ job.name }}
        </a>
        <DirectorBadge
          v-if="showDirectorColumn"
          :director="job.director"
          size="sm"
          class="q-ml-xs"
        />
        <span class="text-grey-6 text-caption q-ml-xs">({{ job.client }})</span>
      </q-item-label>
      <q-item-label caption>
        {{ formatNumber(job.files ?? 0, settings.locale) }} {{ t('Files') }}
        &middot; {{ formatBytes(job.bytes ?? 0) }}
        &middot; {{ formatDuration(durationSecs) }}
      </q-item-label>
      <q-item-label
        v-if="statusText"
        caption
        :class="isWaitingStatus(statusText) ? 'text-orange-7' : 'text-grey-6'"
      >
        <q-icon
          v-if="isWaitingStatus(statusText)"
          name="hourglass_empty"
          size="14px"
          class="q-mr-xs"
        />
        {{ statusText }}
      </q-item-label>
      <q-linear-progress
        indeterminate
        color="positive"
        class="q-mt-xs"
        style="height:6px; border-radius:3px"
      />
    </q-item-section>
    <q-item-section side>
      <q-btn
        flat round dense
        icon="cancel" color="negative" size="sm"
        :title="t('Cancel Job')"
        @click="$emit('cancel', job)"
      />
    </q-item-section>
  </q-item>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { formatBytes, formatDuration } from '../../mock/index.js'
import { formatNumber } from '../../utils/locales.js'
import { isWaitingStatus } from '../../utils/runningJobsGrouping.js'
import { useSettingsStore } from '../../stores/settings.js'
import DirectorBadge from '../../components/DirectorBadge.vue'

const props = defineProps({
  job: {
    type: Object,
    required: true,
  },
  durationSecs: {
    type: Number,
    default: 0,
  },
  showDirectorColumn: {
    type: Boolean,
    default: false,
  },
})

defineEmits(['open-details', 'cancel'])

const { t } = useI18n()
const settings = useSettingsStore()

const statusText = computed(() => props.job.runtimeStatus ?? props.job.status ?? '?')
</script>
