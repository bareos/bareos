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
              v-else-if="!hasAvailableDirectors"
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
            <q-btn
              v-if="canReuseCurrentCredentials"
              flat
              color="primary"
              class="full-width q-mt-sm"
              :label="t('Reuse current credentials')"
              :disable="loading"
              @click="reuseCurrentCredentials"
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
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import {
  useAuthStore,
} from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildDirectorOptions } from '../utils/director.js'
import { toUserVisibleDirectorError } from '../utils/directorErrors.js'
import {
  loginDirectorProxySession,
  loginProxySession,
  reuseProxySessionCredentials,
  SESSION_AUTH_PASSWORD,
  setCurrentProxySessionDirector,
} from '../utils/sessionApi.js'
import LanguageSelect from '../components/LanguageSelect.vue'
import bareosLogo from '../assets/bareos-logo-small.png'

const auth     = useAuthStore()
const director = useDirectorStore()
const route    = useRoute()
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
const LOGIN_CONNECT_TIMEOUT_MS = 15000
const requestedDirector = computed(() => (
  typeof route.query.director === 'string' ? route.query.director.trim() : ''
))
const returnTo = computed(() => (
  typeof route.query.returnTo === 'string' ? route.query.returnTo : ''
))
const isAddDirectorMode = computed(() => (
  auth.isLoggedIn && route.query.mode === 'add'
))
const hasAvailableDirectors = computed(() => director.availableDirectors.length > 0)
const loginPageDirectors = computed(() => (
  directorRef.value && !director.availableDirectors.includes(directorRef.value)
    ? [...director.availableDirectors, directorRef.value]
    : director.availableDirectors
))
const directorOptions = computed(() => (
  buildDirectorOptions({
    availableDirectors: loginPageDirectors.value,
    selectedDirectors: auth.authenticatedDirectors,
    currentDirector: directorRef.value,
    fallbackDirector: auth.user?.director || settings.directorName,
  })
))
const canReuseCurrentCredentials = computed(() => (
  isAddDirectorMode.value
  && !!auth.user?.director
  && !!directorRef.value
  && directorRef.value !== auth.user.director
))

watch(
  () => directorRef.value,
  (value) => {
    if (!isAddDirectorMode.value) {
      return
    }

    username.value = auth.getDirectorUsername(value) || auth.user?.username || settings.loginUsername
  },
  { immediate: true }
)

onMounted(async () => {
  if (requestedDirector.value) {
    directorRef.value = requestedDirector.value
  }

  try {
    const available = await director.fetchAvailableDirectors()
    if (available.length > 0 && !available.includes(directorRef.value)) {
      if (isAddDirectorMode.value) {
        directorRef.value = available.find(value => !auth.hasDirectorSession(value)) || available[0]
      } else {
        directorRef.value = available[0]
      }
    }
  } catch {
    // Leave the manual director input available when the proxy list is not
    // reachable.
  }
})

function currentDirectorError(error, fallbackMessage = '') {
  return toUserVisibleDirectorError(error?.message || fallbackMessage, {
    authenticationMessage: t('Authentication failed'),
    connectionMessage: t('Could not connect to director. Is the proxy running?'),
  })
}

async function activateDirector(targetDirector) {
  auth.setDirector(targetDirector)
  settings.directorName = targetDirector
  director.disconnect()
  await setCurrentProxySessionDirector({ director: targetDirector })
  await director.connectAndWait(auth.getCredentials(targetDirector), LOGIN_CONNECT_TIMEOUT_MS)
}

function finishLoginRedirect() {
  settings.setLocale(locale.value)
  router.push(returnTo.value || { name: 'dashboard' })
}

async function doLogin() {
  loading.value = true
  errorMsg.value = null

  try {
    if (isAddDirectorMode.value) {
      await loginDirectorProxySession({
        username: username.value,
        password: password.value,
        director: directorRef.value,
      })
    } else {
      await loginProxySession({
        username: username.value,
        password: password.value,
        director: directorRef.value,
      })
    }
  } catch (error) {
    errorMsg.value = currentDirectorError(error)
    loading.value = false
    return
  }

  auth.loginDirector(username.value, directorRef.value, SESSION_AUTH_PASSWORD)

  try {
    await activateDirector(directorRef.value)
  } catch (error) {
    errorMsg.value = currentDirectorError(error, director.errorMsg)
    loading.value = false
    return
  }

  loading.value = false
  finishLoginRedirect()
}

async function reuseCurrentCredentials() {
  loading.value = true
  errorMsg.value = null

  try {
    const result = await reuseProxySessionCredentials({
      directors: [directorRef.value],
      sourceDirector: auth.user?.director,
    })
    const reuseResult = result?.results?.find(item => item?.director === directorRef.value)
    if (!reuseResult || !['authenticated', 'already_authenticated'].includes(reuseResult.status)) {
      throw new Error(
        reuseResult?.status === 'authentication_failed'
          ? t('Authentication failed')
          : t('Could not connect to director. Is the proxy running?')
      )
    }

    if (result?.session) {
      auth.applySession(result.session, SESSION_AUTH_PASSWORD)
    } else {
      await auth.restoreSession(true)
    }
    await activateDirector(directorRef.value)
  } catch (error) {
    errorMsg.value = currentDirectorError(error, director.errorMsg)
    loading.value = false
    return
  }

  loading.value = false
  finishLoginRedirect()
}
</script>
