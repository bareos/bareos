<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:600px">
      <q-card-section class="panel-header">
        <span>{{ t('User Settings') }}</span>
      </q-card-section>

      <q-card-section>
        <div class="text-subtitle2 q-mb-sm">{{ t('Language') }}</div>
        <q-item dense>
          <q-item-section avatar>
            <q-icon name="language" />
          </q-item-section>
          <q-item-section>
            <q-item-label>{{ t('WebUI Language') }}</q-item-label>
            <q-item-label caption>
              {{ t('Reuses the locale catalog from the legacy PHP WebUI.') }}
            </q-item-label>
          </q-item-section>
        </q-item>
        <div class="q-px-md q-pb-sm">
          <q-select
            v-model="settings.locale"
            :options="availableLocales"
            emit-value
            map-options
            outlined
            dense
          />
        </div>
      </q-card-section>

      <q-separator />

      <q-card-section>
        <!-- Dark mode -->
        <div class="text-subtitle2 q-mb-sm">{{ t('Appearance') }}</div>
        <q-item tag="label" dense>
          <q-item-section avatar>
            <q-icon :name="settings.darkMode ? 'dark_mode' : 'light_mode'" />
          </q-item-section>
          <q-item-section>
            <q-item-label>{{ t('Dark Mode') }}</q-item-label>
            <q-item-label caption>{{ t('Switch between light and dark theme') }}</q-item-label>
          </q-item-section>
          <q-item-section side>
            <q-toggle v-model="settings.darkMode" @update:model-value="applyDark" />
          </q-item-section>
        </q-item>
      </q-card-section>

      <q-separator />

      <q-card-section>
        <!-- Refresh interval -->
        <div class="text-subtitle2 q-mb-sm">{{ t('Auto-Refresh') }}</div>
        <q-item dense>
          <q-item-section avatar>
            <q-icon name="schedule" />
          </q-item-section>
          <q-item-section>
            <q-item-label>{{ t('Refresh Interval') }}</q-item-label>
            <q-item-label caption>
              {{ t('How often pages auto-refresh:') }} <strong>{{ settings.refreshInterval }}s</strong>
            </q-item-label>
          </q-item-section>
        </q-item>
        <div class="q-px-md q-pb-sm">
          <q-slider
            v-model="settings.refreshInterval"
            :min="5" :max="120" :step="5"
            label snap
            :label-value="settings.refreshInterval + 's'"
            color="primary"
            class="q-mt-xs"
          />
          <div class="row justify-between text-caption text-grey-6">
            <span>5s</span>
            <span>30s</span>
            <span>60s</span>
            <span>120s</span>
          </div>
        </div>
      </q-card-section>
      <q-separator />

      <q-card-section>
        <!-- Time format -->
        <div class="text-subtitle2 q-mb-sm">{{ t('Tables') }}</div>
        <q-item tag="label" dense>
          <q-item-section avatar>
            <q-icon :name="settings.relativeTime ? 'schedule' : 'calendar_today'" />
          </q-item-section>
          <q-item-section>
            <q-item-label>{{ t('Time Display') }}</q-item-label>
            <q-item-label caption>
              {{ settings.relativeTime
                ? t('Relative (e.g. "2 hours ago")')
                : t('Absolute (e.g. "2025-04-01 14:22")' ) }}
            </q-item-label>
          </q-item-section>
          <q-item-section side>
            <q-toggle v-model="settings.relativeTime" />
          </q-item-section>
        </q-item>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import { useSettingsStore } from '../stores/settings.js'
import { localeOptions } from '../utils/locales.js'

const $q       = useQuasar()
const settings = useSettingsStore()
const availableLocales = localeOptions
const { t } = useI18n()

function applyDark(val) {
  $q.dark.set(val)
}

// Apply on mount in case this page is navigated to
$q.dark.set(settings.darkMode)
</script>
