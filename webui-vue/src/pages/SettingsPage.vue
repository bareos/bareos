<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:600px">
      <q-card-section class="panel-header">
        <span>User Settings</span>
      </q-card-section>

      <q-card-section>
        <!-- Dark mode -->
        <div class="text-subtitle2 q-mb-sm">Appearance</div>
        <q-item tag="label" dense>
          <q-item-section avatar>
            <q-icon :name="settings.darkMode ? 'dark_mode' : 'light_mode'" />
          </q-item-section>
          <q-item-section>
            <q-item-label>Dark Mode</q-item-label>
            <q-item-label caption>Switch between light and dark theme</q-item-label>
          </q-item-section>
          <q-item-section side>
            <q-toggle v-model="settings.darkMode" @update:model-value="applyDark" />
          </q-item-section>
        </q-item>
      </q-card-section>

      <q-separator />

      <q-card-section>
        <!-- Refresh interval -->
        <div class="text-subtitle2 q-mb-sm">Auto-Refresh</div>
        <q-item dense>
          <q-item-section avatar>
            <q-icon name="schedule" />
          </q-item-section>
          <q-item-section>
            <q-item-label>Refresh Interval</q-item-label>
            <q-item-label caption>
              How often pages auto-refresh: <strong>{{ settings.refreshInterval }}s</strong>
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
        <div class="text-subtitle2 q-mb-sm">Tables</div>
        <q-item tag="label" dense>
          <q-item-section avatar>
            <q-icon :name="settings.relativeTime ? 'schedule' : 'calendar_today'" />
          </q-item-section>
          <q-item-section>
            <q-item-label>Time Display</q-item-label>
            <q-item-label caption>
              {{ settings.relativeTime ? 'Relative (e.g. "2 hours ago")' : 'Absolute (e.g. "2025-04-01 14:22")' }}
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
import { useQuasar } from 'quasar'
import { useSettingsStore } from '../stores/settings.js'

const $q       = useQuasar()
const settings = useSettingsStore()

function applyDark(val) {
  $q.dark.set(val)
}

// Apply on mount in case this page is navigated to
$q.dark.set(settings.darkMode)
</script>
