<template>
  <q-page class="flex flex-center">
    <div style="width: 380px; padding: 24px">
      <div class="text-center q-mb-lg">
        <img :src="bareosLogo" alt="Bareos" style="height:48px" />
        <div class="text-h5 text-white text-weight-bold q-mt-sm">BAREOS</div>
        <div class="text-subtitle2 text-white">{{ t('Backup Archiving Recovery Open Sourced') }}</div>
      </div>

      <q-card flat bordered>
        <q-card-section class="bg-primary text-white">
          <div class="text-h6">{{ t('Login') }}</div>
        </q-card-section>
        <q-card-section>
          <q-form data-testid="login-form" @submit.prevent="doLogin">
            <q-input
              v-model="username"
              data-testid="login-username"
              :label="t('Username')"
              outlined dense
              class="q-mb-sm"
              autocomplete="username"
            />
            <q-input
              v-model="password"
              data-testid="login-password"
              :label="t('Password')"
              type="password"
              outlined dense
              class="q-mb-sm"
              autocomplete="current-password"
            />
            <q-select
              v-if="hasAvailableDirectors"
              v-model="directorRef"
              data-testid="login-director"
              :options="directorOptions"
              option-label="label"
              option-value="value"
              emit-value
              map-options
              :label="t('Director')"
              outlined dense
              class="q-mb-sm"
            />
            <q-input
              v-else
              v-model="directorRef"
              data-testid="login-director"
              :label="t('Director')"
              outlined dense
              class="q-mb-sm"
            />
            <LanguageSelect
              v-model="locale"
              data-testid="login-language"
              :label="t('Language')"
              class="q-mb-md"
            />

            <q-banner v-if="errorMsg" data-testid="login-error" dense class="bg-negative text-white q-mb-md rounded-borders">
              <template #avatar><q-icon name="error" /></template>
              {{ errorMsg }}
            </q-banner>

            <q-btn
              data-testid="login-submit"
              type="submit"
              :label="t('Login')"
              color="primary"
              class="full-width"
              :loading="loading"
            />
          </q-form>
        </q-card-section>
      </q-card>

      <div class="text-center text-white q-mt-md" style="font-size:0.75rem; opacity:0.7">
        Bareos WebUI &copy; 2013–2026 Bareos GmbH &amp; Co. KG
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { computed, onMounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import {
  useAuthStore,
} from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import LanguageSelect from '../components/LanguageSelect.vue'
import bareosLogo from '../assets/bareos-logo-small.png'

const auth     = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const router   = useRouter()
const { t } = useI18n()

const username = computed({
  get: () => settings.loginUsername,
  set: (value) => { settings.loginUsername = value },
})
const directorRef = computed({
  get: () => settings.directorName,
  set: (value) => { settings.directorName = value },
})
const password  = ref('')
const locale    = ref(settings.locale)
const loading   = ref(false)
const errorMsg  = ref(null)
const hasAvailableDirectors = computed(() => director.availableDirectors.length > 0)
const directorOptions = computed(() => {
  const options = [...director.availableDirectors]
  if (directorRef.value && !options.includes(directorRef.value)) {
    options.unshift(directorRef.value)
  }
  return options.map((value) => ({ label: value, value }))
})

onMounted(async () => {
  try {
    const available = await director.fetchAvailableDirectors()
    if (available.length > 0 && !available.includes(directorRef.value)) {
      directorRef.value = available[0]
    }
  } catch {
    // Leave the manual director input available when the proxy list is not
    // reachable.
  }
})

async function doLogin() {
  loading.value = true
  errorMsg.value = null

  // Connect WebSocket proxy → director
  director.connect({
    username:  username.value,
    password:  password.value,
    director:  directorRef.value,
  })

  // Wait up to 8 s for auth result
  const ok = await new Promise((resolve) => {
    const stop = watch(
      () => director.status,
      (s) => {
        if (s === 'connected')   { stop(); resolve(true)  }
        if (s === 'error')       { stop(); resolve(false) }
      }
    )
    setTimeout(() => { stop(); resolve(false) }, 8000)
  })

  if (!ok) {
    errorMsg.value = director.errorMsg || t('Could not connect to director. Is the proxy running?')
    loading.value = false
    return
  }

  settings.setLocale(locale.value)
  auth.login(username.value, directorRef.value, password.value)
  loading.value = false
  router.push({ name: 'dashboard' })
}
</script>
