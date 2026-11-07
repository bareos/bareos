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
  <div class="storage-details-panel row q-col-gutter-md">
    <div class="col-12 col-md-4">
      <q-card flat bordered class="bareos-panel">
        <q-card-section class="panel-header row items-center">
          <span>{{ t('Storage Details') }}</span>
        </q-card-section>
        <q-card-section class="q-pa-none">
          <q-list dense separator>
            <q-item v-for="field in fields" :key="field.label">
              <q-item-section>
                <q-item-label caption>{{ field.label }}</q-item-label>
                <q-item-label>
                  <component :is="field.component" v-if="field.component" v-bind="field.props" />
                  <span v-else-if="field.value">{{ field.value }}</span>
                  <span v-else class="text-grey-5">&mdash;</span>
                </q-item-label>
              </q-item-section>
            </q-item>
          </q-list>
        </q-card-section>
      </q-card>
    </div>

    <div class="col-12 col-md-8">
      <q-card flat bordered class="bareos-panel">
        <q-card-section class="panel-header row items-center">
          <span>{{ t('Status') }}</span>
          <q-space />
          <q-btn
            flat round dense icon="refresh" color="white"
            :title="t('Refresh')" :aria-label="t('Refresh')"
            :loading="loading"
            data-testid="storage-details-refresh"
            @click="reload"
          />
        </q-card-section>
        <q-card-section>
          <div v-if="error" class="text-negative">{{ error }}</div>
          <div v-else class="console-output" data-testid="storage-details-status">{{ statusText }}</div>
        </q-card-section>
      </q-card>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { useDirectorStore } from '../stores/director.js'
import { useAuthStore } from '../stores/auth.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import DirectorLabel from './DirectorLabel.vue'
import EnabledBadge from './EnabledBadge.vue'

const props = defineProps({
  // Decorated storage row (name, director, address, ...) owned by the
  // parent page; `null` means nothing is selected.
  storage: { type: Object, default: null },
  // Show the owning director, which is only meaningful across directors.
  showDirector: { type: Boolean, default: false },
})

const director = useDirectorStore()
const auth = useAuthStore()
const { t } = useI18n()

const loading = ref(false)
const error = ref(null)
const statusText = ref('')

const fields = computed(() => {
  const storage = props.storage
  if (!storage) {
    return []
  }

  return [
    { label: t('Name'), value: storage.name },
    {
      label: t('Address'),
      value: storage.address
        ? `${storage.address}${storage.port ? `:${storage.port}` : ''}`
        : '',
    },
    { label: t('Media Type'), value: storage.mediatype },
    { label: t('Device'), value: storage.device },
    {
      label: t('Status'),
      component: EnabledBadge,
      props: { enabled: !!storage.enabled },
    },
    ...(props.showDirector
      ? [{
        label: t('Director'),
        component: DirectorLabel,
        props: { director: storage.director ?? '' },
      }]
      : []),
  ]
})

async function ensureStorageDirector() {
  const target = props.storage?.director
  if (!target) {
    return
  }

  if (auth.user?.director === target && director.isConnected) {
    return
  }

  await switchActiveDirector(target)
}

async function reload() {
  if (!props.storage?.name) {
    statusText.value = ''
    return
  }

  loading.value = true
  error.value = null
  try {
    await ensureStorageDirector()
    statusText.value = await director.rawCall(
      `status storage=${quoteDirectorString(props.storage.name)}`
    )
  } catch (reason) {
    statusText.value = ''
    error.value = reason?.message ?? String(reason)
  } finally {
    loading.value = false
  }
}

watch(() => props.storage, () => {
  statusText.value = ''
  error.value = null
  void reload()
}, { immediate: true })
</script>
