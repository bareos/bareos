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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
-->

<template>
  <q-banner
    v-if="showWarning"
    class="bg-warning text-black q-ma-md rounded-borders"
    role="alert"
  >
    <template #avatar>
      <q-icon name="warning" />
    </template>
    {{ t('This WebUI is using an insecure HTTP connection. Credentials and data may be exposed.') }}
  </q-banner>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
import { isHttpConnection } from '../utils/httpSecurity.js'

const { t } = useI18n()
const showWarning = isHttpConnection()
</script>
