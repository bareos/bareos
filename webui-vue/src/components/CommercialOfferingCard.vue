<!--
  bareos-webui - Bareos Web-Frontend

  @link      https://github.com/bareos/bareos
  @copyright Copyright (C) 2026-2026 Bareos GmbH & Co. KG
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
  <q-card
    flat bordered
    class="commercial-offering-card"
    :class="{ 'commercial-offering-card--compact': compact }"
    data-testid="commercial-offering-card"
  >
    <q-card-section class="q-pb-sm">
      <div class="text-subtitle1 text-weight-medium">
        <q-icon name="workspace_premium" color="primary" class="q-mr-xs" />
        {{ t('Bareos for production environments') }}
      </div>
      <div class="text-body2 text-grey-8">
        <template v-if="kind === 'unknown'">
          {{ t('Community builds of Bareos come without official subscription. The following services are available from bareos.com.') }}
        </template>
        <template v-else>
          {{ t('You are running a {kind} without official subscription. The following services are available from bareos.com.', { kind: t(buildKindLabel(kind)).toLowerCase() }) }}
        </template>
      </div>
    </q-card-section>
    <q-card-section class="q-pt-none">
      <div class="row q-col-gutter-sm">
        <div
          v-for="offering in COMMERCIAL_OFFERINGS"
          :key="offering.id"
          :class="offering.highlight || compact ? 'col-12' : 'col-12 col-sm-6'"
        >
          <a
            :href="offering.url"
            target="_blank"
            rel="noopener noreferrer"
            class="offering-tile"
            :class="{ 'offering-tile--highlight': offering.highlight }"
            :data-testid="`commercial-offering-${offering.id}`"
          >
            <q-icon :name="offering.icon" :size="compact ? '20px' : '24px'" :color="offering.highlight ? 'white' : 'primary'" />
            <div class="offering-text">
              <div class="text-weight-medium">{{ t(offering.label) }}</div>
              <div v-if="!compact || offering.highlight" class="text-caption">
                {{ t(offering.description) }}
                <template v-if="offering.id === 'subscription'">
                  {{ t('Plugins') }}: {{ SUBSCRIPTION_ONLY_PLUGINS.join(', ') }}, …
                </template>
              </div>
            </div>
            <q-icon name="open_in_new" size="14px" class="offering-open" />
          </a>
        </div>
      </div>
    </q-card-section>
    <q-card-actions align="right">
      <q-btn
        flat no-caps color="primary" icon-right="open_in_new"
        :label="t('All services on bareos.com')"
        :href="SERVICES_URL" target="_blank" rel="noopener noreferrer"
      />
    </q-card-actions>
  </q-card>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
import {
  COMMERCIAL_OFFERINGS,
  SERVICES_URL,
  SUBSCRIPTION_ONLY_PLUGINS,
  buildKindLabel,
} from '../utils/commercialOffering.js'

defineProps({
  kind: { type: String, default: 'unknown' },
  compact: { type: Boolean, default: false },
})
const { t } = useI18n()
</script>

<style scoped>
.offering-tile {
  display: flex;
  align-items: flex-start;
  gap: 10px;
  height: 100%;
  padding: 10px 12px;
  border: 1px solid rgba(0, 0, 0, 0.12);
  border-radius: 6px;
  color: inherit;
  text-decoration: none;
  transition: background 0.15s ease, border-color 0.15s ease;
}

.offering-tile:hover,
.offering-tile:focus-visible {
  border-color: var(--q-primary);
  background: rgba(0, 0, 0, 0.03);
}

.offering-tile--highlight {
  background: var(--q-primary);
  border-color: var(--q-primary);
  color: white;
}

.offering-tile--highlight:hover,
.offering-tile--highlight:focus-visible {
  background: var(--q-primary);
  filter: brightness(1.08);
}

.offering-text {
  flex: 1;
}

.commercial-offering-card--compact .offering-tile {
  padding: 6px 10px;
  align-items: center;
}

.offering-open {
  opacity: 0.6;
}
</style>
