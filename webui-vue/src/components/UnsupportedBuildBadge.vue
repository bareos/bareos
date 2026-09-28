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
  <a
    :href="EVALUATION_URL"
    target="_blank"
    rel="noopener noreferrer"
    class="unsupported-build-badge"
    data-testid="unsupported-build-badge"
  >
    <q-icon name="warning" size="14px" class="q-mr-xs" />
    {{ t('Unsupported build') }}
    <q-tooltip>{{ t(UNSUPPORTED_BUILD_TEXT) }} · {{ t('Try the subscription release') }}</q-tooltip>
  </a>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
import { EVALUATION_URL, UNSUPPORTED_BUILD_TEXT } from '../utils/commercialOffering.js'

const { t } = useI18n()
</script>

<style scoped>
.unsupported-build-badge {
  display: inline-flex;
  align-items: center;
  padding: 2px 10px;
  border-radius: 999px;
  background: #ef6c00;
  color: white;
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  text-decoration: none;
  white-space: nowrap;
}

.unsupported-build-badge:hover,
.unsupported-build-badge:focus-visible {
  background: #f57c00;
}
</style>
