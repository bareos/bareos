<template>
  <q-page class="login-page flex flex-center">
    <div class="login-shell">
      <q-card flat bordered class="login-card">
        <q-card-section class="login-card-header">
          <img :src="bareosLogo" alt="Bareos" class="login-card-logo" />
          <div class="text-h6 text-weight-medium q-ml-sm">{{ t('Log in to Bareos') }}</div>
        </q-card-section>
        <q-card-section class="q-pt-md">
          <q-form data-testid="login-form" @submit.prevent="doLogin">
            <q-input
              v-model="username"
              data-testid="login-username"
              :label="t('Username')"
              outlined
              class="login-input q-mb-sm"
              autocomplete="username"
            >
              <template #prepend><q-icon name="person" /></template>
            </q-input>
            <q-input
              v-model="password"
              data-testid="login-password"
              :label="t('Password')"
              type="password"
              outlined
              class="login-input q-mb-sm"
              autocomplete="current-password"
            >
              <template #prepend><q-icon name="key" /></template>
            </q-input>
            <q-banner
              v-if="isMultiDirectorLogin"
              dense
              class="login-multi-banner q-mb-md rounded-borders"
            >
              <div class="row items-center no-wrap text-weight-medium">
                <q-icon name="dns" class="q-mr-sm" />
                <span>{{ multiDirectorLoginMessage }}</span>
              </div>
              <div
                v-if="multiDirectorStatuses.length > 0"
                class="q-mt-sm"
                data-testid="login-target-directors"
              >
                <div class="text-caption text-grey-7 q-mb-xs">{{ t('Directors') }}</div>
                <div class="column q-gutter-xs">
                  <div
                    v-for="entry in multiDirectorStatuses"
                    :key="entry.director"
                    class="director-login-status"
                  >
                    <div class="row items-center no-wrap">
                      <q-icon :name="entry.icon" :color="entry.color" size="16px" class="q-mr-sm" />
                      <span class="text-weight-medium q-mr-sm">{{ entry.director }}</span>
                      <span :class="entry.statusClass">{{ entry.statusText }}</span>
                    </div>
                    <div
                      v-if="entry.failureMessage"
                      class="text-negative text-caption q-ml-lg"
                    >
                      {{ entry.failureMessage }}
                    </div>
                  </div>
                </div>
              </div>
            </q-banner>
            <q-select
              v-if="showDirectorSelect"
              v-model="directorRef"
              data-testid="login-director"
              :options="directorOptions"
              option-label="label"
              option-value="value"
              emit-value
              map-options
              :label="t('Director')"
              :error="!!directorLoadError"
              :error-message="directorLoadError"
              bottom-slots
              outlined
              class="login-director-field q-mb-lg"
            >
              <template #prepend><q-icon name="dns" /></template>
            </q-select>
            <q-input
              v-else-if="showDirectorTextInput"
              v-model="directorRef"
              data-testid="login-director"
              :label="t('Director')"
              :error="!!directorLoadError"
              :error-message="directorLoadError"
              bottom-slots
              outlined
              class="login-director-field q-mb-lg"
            >
              <template #prepend><q-icon name="dns" /></template>
            </q-input>
            <q-banner v-if="errorMsg" data-testid="login-error" dense class="bg-negative text-white q-mb-md rounded-borders">
              <template #avatar><q-icon name="error" /></template>
              {{ errorMsg }}
            </q-banner>

            <q-btn
              data-testid="login-submit"
              type="submit"
              :label="submitLabel"
              color="primary"
              unelevated
              class="full-width login-submit"
              :loading="loading"
            />
            <q-btn
              v-if="canSkipRemainingDirectors"
              flat
              color="primary"
              class="full-width q-mt-sm"
              :label="t('Skip failed directors')"
              :disable="loading"
              @click="skipFailedDirectors"
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
    </div>

    <div class="login-language-anchor">
      <LanguageSelect v-model="localeRef" hide-label class="login-language-select" />
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
import {
  directorListLoadErrorMessage,
  getLastSuccessfulDirector,
  shouldAutoLoginAllDirectors,
  summarizeDirectorLoginAttempts,
} from '../utils/directorLogin.js'
import { toUserVisibleDirectorError } from '../utils/directorErrors.js'
import {
  canReuseDirectorCredentials,
  getDirectorReuseResult,
  getDirectorReuseErrorMessage,
  isSuccessfulDirectorReuse,
} from '../utils/directorSessionReuse.js'
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
const localeRef = computed({
  get: () => settings.locale,
  set: (value) => { settings.setLocale(value) },
})
const password  = ref('')
const loading   = ref(false)
const errorMsg  = ref(null)
const directorLoadError = ref('')
const LOGIN_CONNECT_TIMEOUT_MS = 15000
const autoReusedDirectors = new Set()
const primaryDirector = ref('')
const remainingDirectorFailures = ref([])
const requestedDirector = computed(() => (
  typeof route.query.director === 'string' ? route.query.director.trim() : ''
))
const returnTo = computed(() => (
  typeof route.query.returnTo === 'string' ? route.query.returnTo : ''
))
const isAddDirectorMode = computed(() => (
  auth.isLoggedIn && route.query.mode === 'add'
))
const configuredDirectors = computed(() => (
  [...new Set(director.availableDirectors.map(value => String(value ?? '').trim()).filter(Boolean))]
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
const isMultiDirectorLogin = computed(() => (
  shouldAutoLoginAllDirectors({
    isAddDirectorMode: isAddDirectorMode.value,
    requestedDirector: requestedDirector.value,
    availableDirectors: configuredDirectors.value,
  })
))
const showDirectorSelect = computed(() => (
  !isMultiDirectorLogin.value && directorOptions.value.length > 1
))
const showDirectorTextInput = computed(() => (
  !isMultiDirectorLogin.value && !hasAvailableDirectors.value
))
const canReuseCurrentCredentials = computed(() => (
  canReuseDirectorCredentials({
    isAddDirectorMode: isAddDirectorMode.value,
    sourceDirector: auth.user?.director,
    targetDirector: directorRef.value,
  })
))
const canSkipRemainingDirectors = computed(() => (
  isMultiDirectorLogin.value
  && remainingDirectorFailures.value.length > 0
  && auth.authenticatedDirectors.length > 0
))
const submitLabel = computed(() => (
  remainingDirectorFailures.value.length > 0
    ? t('Retry remaining directors')
    : t('Login')
))
const multiDirectorLoginMessage = computed(() => {
  if (!isMultiDirectorLogin.value) {
    return ''
  }

  if (remainingDirectorFailures.value.length > 0) {
    return canSkipRemainingDirectors.value
      ? t('Retry the remaining directors or skip them.')
      : t('Retry the remaining directors.')
  }

  return t('The entered credentials will be tried on all configured directors.')
})
const multiDirectorFailureByDirector = computed(() => {
  const failureMap = {}
  for (const attempt of remainingDirectorFailures.value) {
    const directorName = String(attempt?.director ?? '').trim()
    if (!directorName) {
      continue
    }
    failureMap[directorName] = String(attempt?.message ?? '').trim()
  }
  return failureMap
})
const multiDirectorStatuses = computed(() => (
  configuredDirectors.value.map((directorName) => {
    const failureMessage = multiDirectorFailureByDirector.value[directorName] || ''
    if (failureMessage) {
      return {
        director: directorName,
        icon: 'cancel',
        color: 'negative',
        statusClass: 'text-negative',
        statusText: t('Login failure'),
        failureMessage,
      }
    }

    if (auth.hasDirectorSession(directorName)) {
      return {
        director: directorName,
        icon: 'check_circle',
        color: 'positive',
        statusClass: 'text-positive',
        statusText: t('Successfully logged in'),
        failureMessage: '',
      }
    }

    return {
      director: directorName,
      icon: 'radio_button_unchecked',
      color: 'grey-7',
      statusClass: 'text-grey-8',
      statusText: t('Not yet logged in'),
      failureMessage: '',
    }
  })
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
    directorLoadError.value = ''
    if (available.length > 0 && !available.includes(directorRef.value)) {
      if (isAddDirectorMode.value) {
        directorRef.value = available.find(value => !auth.hasDirectorSession(value)) || available[0]
      } else {
        directorRef.value = available[0]
      }
    }
  } catch (error) {
    directorLoadError.value = directorListLoadErrorMessage(error, t)
  }

  await autoReuseCurrentCredentials()
})

function currentDirectorError(error, fallbackMessage = '', { loginSucceeded = false } = {}) {
  const errorMsg = error?.message || fallbackMessage

  // If login succeeded (HTTP API works) but WebSocket failed,
  // show a specific message about WebSocket connection
  if (loginSucceeded && errorMsg?.includes('Could not connect to director')) {
    return toUserVisibleDirectorError(
      t('WebSocket connection failed. Check proxy configuration or firewall.'),
      {
        authenticationMessage: t('Authentication failed'),
        connectionMessage: t('WebSocket connection failed'),
      }
    )
  }

  return toUserVisibleDirectorError(errorMsg, {
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
  router.push(returnTo.value || { name: 'dashboard' })
}

async function finalizeSuccessfulLogin(targetDirector = primaryDirector.value || auth.authenticatedDirectors[0]) {
  if (!targetDirector) {
    errorMsg.value = t('Authentication failed')
    loading.value = false
    return false
  }

  try {
    await activateDirector(targetDirector)
  } catch (error) {
    errorMsg.value = currentDirectorError(error, director.errorMsg)
    loading.value = false
    return false
  }

  remainingDirectorFailures.value = []
  loading.value = false
  finishLoginRedirect()
  return true
}

async function attemptDirectorLogins(targetDirectors) {
  const attempts = []
  let hasSession = auth.isLoggedIn

  for (const targetDirector of targetDirectors) {
    try {
      const payload = hasSession
        ? await loginDirectorProxySession({
          username: username.value,
          password: password.value,
          director: targetDirector,
        })
        : await loginProxySession({
          username: username.value,
          password: password.value,
          director: targetDirector,
        })
      hasSession = true
      auth.applySession(payload, SESSION_AUTH_PASSWORD, { merge: true })
      attempts.push({
        director: targetDirector,
        success: true,
      })
    } catch (error) {
      attempts.push({
        director: targetDirector,
        success: false,
        message: currentDirectorError(error),
      })
    }
  }

  return attempts
}

async function doMultiDirectorLogin() {
  const targetDirectors = remainingDirectorFailures.value.length > 0
    ? remainingDirectorFailures.value.map(attempt => attempt.director)
    : configuredDirectors.value

  if (targetDirectors.length === 0) {
    errorMsg.value = t('Could not load the configured directors.')
    loading.value = false
    return
  }

  const attempts = await attemptDirectorLogins(targetDirectors)
  const {
    successfulDirectors,
    failedAttempts,
  } = summarizeDirectorLoginAttempts(attempts)
  const finalDirector = getLastSuccessfulDirector(attempts)

  if (successfulDirectors.length === 0) {
    errorMsg.value = t('Could not log in to any configured director. Retry the remaining directors.')
    password.value = ''
    loading.value = false
    return
  }

  errorMsg.value = null

  if (failedAttempts.length > 0) {
    password.value = ''
  }

  primaryDirector.value = finalDirector
  remainingDirectorFailures.value = failedAttempts
  await finalizeSuccessfulLogin(finalDirector)
}

async function doLogin() {
  loading.value = true
  if (!isMultiDirectorLogin.value) {
    errorMsg.value = null
  }

  if (isMultiDirectorLogin.value) {
    await doMultiDirectorLogin()
    return
  }

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
  primaryDirector.value = directorRef.value

  await finalizeSuccessfulLogin(directorRef.value)
}

async function reuseCurrentCredentials(options = {}) {
  const { silent = false } = options
  loading.value = true
  if (!silent) {
    errorMsg.value = null
  }

  let httpLoginSucceeded = false

  try {
    const result = await reuseProxySessionCredentials({
      directors: [directorRef.value],
      sourceDirector: auth.user?.director,
    })
    const reuseResult = getDirectorReuseResult(result, directorRef.value)
    if (!isSuccessfulDirectorReuse(result, directorRef.value)) {
      throw new Error(getDirectorReuseErrorMessage(reuseResult))
    }

    httpLoginSucceeded = true

    if (result?.session) {
      auth.applySession(result.session, SESSION_AUTH_PASSWORD)
    } else {
      await auth.restoreSession(true)
    }
    await activateDirector(directorRef.value)
  } catch (error) {
    if (!silent) {
      errorMsg.value = currentDirectorError(error, director.errorMsg, {
        loginSucceeded: httpLoginSucceeded,
      })
    }
    loading.value = false
    return false
  }

  loading.value = false
  finishLoginRedirect()
  return true
}

async function autoReuseCurrentCredentials() {
  if (!canReuseCurrentCredentials.value || autoReusedDirectors.has(directorRef.value)) {
    return false
  }

  autoReusedDirectors.add(directorRef.value)
  return reuseCurrentCredentials({ silent: true })
}

async function skipFailedDirectors() {
  if (!canSkipRemainingDirectors.value) {
    return
  }

  loading.value = true
  errorMsg.value = null
  await finalizeSuccessfulLogin()
}
</script>

<style scoped>
.login-page {
  background: transparent;
}

.login-shell {
  width: min(420px, calc(100vw - 32px));
  padding: 24px;
}

.login-card {
  border-radius: 8px;
  box-shadow: 0 6px 16px rgba(0, 0, 0, 0.18);
}

.login-card-header {
  display: flex;
  align-items: center;
  color: white;
  border-radius: 8px 8px 0 0;
  background: var(--q-primary);
}

.login-card-logo {
  height: 28px;
}

.login-input,
.login-director-field {
  transition: transform 0.15s ease, box-shadow 0.2s ease;
}

.login-input :deep(.q-field__control),
.login-director-field :deep(.q-field__control) {
  border-radius: 6px;
  min-height: 44px;
}

.login-input:focus-within,
.login-director-field:focus-within {
  transform: translateY(-1px);
}

.login-multi-banner {
  border: 1px solid rgba(0, 0, 0, 0.08);
  background: #f5f7fa;
}

.director-login-status {
  padding: 4px 0;
}

.login-director-field :deep(.q-field__bottom) {
  min-height: 2.8em;
}

.login-submit {
  min-height: 42px;
  font-weight: 600;
  letter-spacing: 0;
}

.login-language-anchor {
  position: fixed;
  left: 12px;
  bottom: 0;
  z-index: 10;
  padding: 10px 0;
  opacity: 0.78;
  transition: opacity 0.2s ease;
}

.login-language-anchor:hover,
.login-language-anchor:focus-within {
  opacity: 1;
}

.login-language-select {
  min-width: 156px;
  font-size: 0.85rem;
}

.login-language-select :deep(.q-field__control) {
  min-height: 30px;
  border-radius: 6px;
  background: rgba(255, 255, 255, 0.74);
}

.login-language-select :deep(.q-field__label),
.login-language-select :deep(.q-field__native span),
.login-language-select :deep(.q-item__label) {
  font-size: 0.8rem;
}

</style>
