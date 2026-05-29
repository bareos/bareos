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
  <q-card flat bordered class="bareos-panel">
    <q-card-section class="panel-header row items-center">
      <span>{{ t('Director Info') }}</span>
      <q-space />
      <span class="text-white text-caption q-mr-sm panel-refresh-countdown">
        <span aria-hidden="true">↻</span>
        <span class="panel-refresh-countdown__value">{{ statusCountdown }}s</span>
      </span>
      <q-btn
        flat
        round
        dense
        icon="refresh"
        color="white"
        :loading="statusLoading"
        @click="emit('refresh')"
      />
    </q-card-section>
    <q-card-section class="q-py-sm">
      <div class="column q-gutter-md">
        <div
          v-for="card in statusCards"
          :key="card.scopeDirector"
          class="row items-center q-gutter-sm text-body2 flex-wrap"
        >
          <DirectorBadge
            :director="card.scopeDirector"
            icon="dns"
            class="director-info-chip"
          >
            {{ card.scopeDirector }}
            <q-tooltip>
              {{
                card.reportedDirector && card.reportedDirector !== card.scopeDirector
                  ? `${t('Configured as')}: ${card.scopeDirector}\n${t('Reported by Director')}: ${card.reportedDirector}`
                  : `${t('Director')}: ${card.scopeDirector}`
              }}
            </q-tooltip>
          </DirectorBadge>
          <q-chip
            v-if="card.config_warnings != null || !card.configStatusAvailable"
            dense
            square
            :clickable="card.configStatusAvailable"
            :class="[
              'director-info-chip',
              card.configStatusAvailable ? 'director-info-chip--clickable' : '',
            ]"
            :color="configStatusChipColor(card)"
            :icon="configStatusChipIcon(card)"
            text-color="white"
            @click="card.configStatusAvailable && emit('show-config-status', card.scopeDirector)"
          >
            {{ configStatusChipLabel(card, t) }}
            <q-tooltip>
              {{ configStatusChipTooltip(card, t) }}
            </q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.reportedDirector && card.reportedDirector !== card.scopeDirector"
            dense
            square
            class="director-info-chip"
            color="blue-grey-6"
            text-color="white"
            icon="link"
          >
            {{ card.reportedDirector }}
            <q-tooltip>{{ t('Reported by Director') }}: {{ card.reportedDirector }}</q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.version"
            dense
            square
            class="director-info-chip"
            color="blue-7"
            text-color="white"
            icon="info"
          >
            {{ card.version }}
            <q-tooltip>Version: {{ card.version }}</q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.release_date"
            dense
            square
            class="director-info-chip"
            color="blue-grey-6"
            text-color="white"
            icon="event"
          >
            {{ card.release_date }}
            <q-tooltip>Released: {{ card.release_date }}</q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.binary_info"
            dense
            square
            class="director-info-chip"
            color="blue-grey-7"
            text-color="white"
            icon="build"
          >
            {{ card.binary_info }}
            <q-tooltip>Build: {{ card.binary_info }}</q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.os"
            dense
            square
            class="director-info-chip"
            :color="card.osIcon.color"
            text-color="white"
            :icon="card.osIcon.icon"
          >
            {{ card.os }}
            <q-tooltip>OS: {{ card.os }}</q-tooltip>
          </q-chip>
          <q-chip
            v-if="card.daemon_started"
            dense
            square
            class="director-info-chip"
            color="teal-7"
            text-color="white"
            icon="schedule"
          >
            {{ relativeTime ? formatDirectorRelativeTime(card.daemon_started, locale) : card.daemon_started }}
            <q-tooltip>Started: {{ card.daemon_started }}</q-tooltip>
          </q-chip>
          <router-link
            v-if="card.jobs_run != null"
            :to="{ name: 'jobs' }"
            class="text-decoration-none"
          >
            <q-chip
              dense
              square
              clickable
              class="director-info-chip director-info-chip--clickable"
              color="purple-7"
              text-color="white"
              icon="check"
            >
              {{ card.jobs_run }}
              <q-tooltip>Jobs Run: {{ card.jobs_run }}</q-tooltip>
            </q-chip>
          </router-link>
          <router-link
            v-if="card.jobs_running != null"
            :to="{
              name: 'jobs',
              query: withJobsStatusFilterQuery({}, 'R'),
            }"
            class="text-decoration-none"
          >
            <q-chip
              dense
              square
              clickable
              class="director-info-chip director-info-chip--clickable"
              color="orange-7"
              text-color="white"
              icon="play_arrow"
            >
              {{ card.jobs_running }}
              <q-tooltip>Running: {{ card.jobs_running }}</q-tooltip>
            </q-chip>
          </router-link>
        </div>
      </div>
    </q-card-section>
  </q-card>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
import DirectorBadge from './DirectorBadge.vue'
import { formatDirectorRelativeTime } from '../utils/locales.js'
import { withJobsStatusFilterQuery } from '../utils/jobs.js'
import {
  configStatusChipColor,
  configStatusChipIcon,
  configStatusChipLabel,
  configStatusChipTooltip,
} from '../utils/directorStatus.js'

defineProps({
  statusCards: {
    type: Array,
    default: () => [],
  },
  statusCountdown: {
    type: Number,
    default: 0,
  },
  statusLoading: {
    type: Boolean,
    default: false,
  },
  locale: {
    type: String,
    default: 'en_EN',
  },
  relativeTime: {
    type: Boolean,
    default: false,
  },
})

const emit = defineEmits(['refresh', 'show-config-status'])
const { t } = useI18n()
</script>

<style scoped>
.director-info-chip {
  cursor: default;
  user-select: none;
}

.director-info-chip--clickable {
  cursor: pointer;
}
</style>
