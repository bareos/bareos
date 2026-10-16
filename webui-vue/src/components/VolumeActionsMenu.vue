<!--
  bareos-webui - Bareos Web-Frontend

  @link      https://github.com/bareos/bareos
  @copyright Copyright (C) 2013-2026 Bareos GmbH & Co. KG
  @license   GNU Affero General Public License (http://www.gnu.org/licenses/)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
-->

<template>
  <q-btn-dropdown
    color="primary" dense no-caps icon="playlist_play"
    :label="label ?? t('Volume actions')"
    :disable="disable || !count"
    :data-testid="`${testid}-actions`"
  >
    <q-list dense style="min-width:220px">
      <q-item
        v-for="action in nonDestructiveActions"
        :key="action.id"
        clickable v-close-popup
        :data-testid="`${testid}-${action.id}`"
        @click="emit('select', action.id)"
      >
        <q-item-section avatar><q-icon :name="action.icon" /></q-item-section>
        <q-item-section>{{ t(action.label) }}</q-item-section>
      </q-item>
      <template v-if="statusActions.length">
        <q-separator />
        <q-item-label header>{{ t('Volume status') }}</q-item-label>
        <q-item
          v-for="action in statusActions"
          :key="action.id"
          clickable v-close-popup
          :data-testid="`${testid}-${action.id}`"
          @click="emit('select', action.id)"
        >
          <q-item-section avatar><q-icon :name="action.icon" /></q-item-section>
          <q-item-section>
            <q-item-label>{{ t(action.label) }}</q-item-label>
            <q-item-label caption>
              {{ action.requiresStatus.join(', ') }} → {{ action.volstatus }}
            </q-item-label>
          </q-item-section>
        </q-item>
      </template>
      <q-separator />
      <q-item-label header class="text-negative">
        <q-icon name="warning" class="q-mr-xs" />{{ t('Destructive') }}
      </q-item-label>
      <q-item
        v-for="action in destructiveActions"
        :key="action.id"
        clickable v-close-popup
        class="text-negative"
        :data-testid="`${testid}-${action.id}`"
        @click="emit('select', action.id)"
      >
        <q-item-section avatar><q-icon :name="action.icon" color="negative" /></q-item-section>
        <q-item-section>{{ t(action.label) }}</q-item-section>
      </q-item>
    </q-list>
  </q-btn-dropdown>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { VOLUME_BULK_ACTIONS, isVolumeActionApplicable } from '../utils/volumeBulk.js'

const props = defineProps({
  // Number of volumes the actions would apply to; zero disables the menu.
  count: { type: Number, default: 0 },
  disable: { type: Boolean, default: false },
  label: { type: String, default: null },
  testid: { type: String, default: 'volume-actions' },
  // Current volstatus values of the volumes; status actions that apply to
  // none of them are hidden. Null shows all of them.
  statuses: { type: Array, default: null },
})
const emit = defineEmits(['select'])
const { t } = useI18n()

const nonDestructiveActions = VOLUME_BULK_ACTIONS.filter(
  action => !action.destructive && action.group !== 'status',
)
const statusActions = computed(() => VOLUME_BULK_ACTIONS.filter(
  action => action.group === 'status'
    && (!props.statuses
      || props.statuses.some(status => isVolumeActionApplicable(action, status))),
))
const destructiveActions = VOLUME_BULK_ACTIONS.filter(action => action.destructive)
</script>
