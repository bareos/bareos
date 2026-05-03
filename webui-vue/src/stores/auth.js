import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'
const SESSION_TOKEN_KEY = 'bareos_session_token'

export function canAuthenticateWithDirector(credentials) {
  return !!(
    credentials
    && (
      credentials.sessionToken
      || (credentials.username && credentials.password)
    )
  )
}

export function buildDirectorAuthMessage(credentials, options = {}) {
  if (!canAuthenticateWithDirector(credentials)) {
    return null
  }

  const director = options.director ?? credentials.director ?? DEFAULT_DIRECTOR_NAME
  const payload = {
    type: 'auth',
    director,
  }

  if (options.mode) {
    payload.mode = options.mode
  }

  if (credentials.sessionToken) {
    payload.session_token = credentials.sessionToken
  } else {
    payload.username = credentials.username
    payload.password = credentials.password
  }

  return payload
}

export const useAuthStore = defineStore('auth', () => {
  const user = ref(JSON.parse(sessionStorage.getItem('bareos_user') || 'null'))
  // Only opaque proxy session tokens survive a reload. Plaintext passwords are
  // kept in memory only during the initial login flow.
  const _password = ref('')
  const _sessionToken = ref(sessionStorage.getItem(SESSION_TOKEN_KEY) || '')
  const isLoggedIn = computed(() => !!user.value)

  function persistUser() {
    if (user.value) {
      sessionStorage.setItem('bareos_user', JSON.stringify(user.value))
    } else {
      sessionStorage.removeItem('bareos_user')
    }
  }

  function persistSessionToken() {
    if (_sessionToken.value) {
      sessionStorage.setItem(SESSION_TOKEN_KEY, _sessionToken.value)
    } else {
      sessionStorage.removeItem(SESSION_TOKEN_KEY)
    }
  }

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    authData = {},
  ) {
    const normalizedAuth = typeof authData === 'string'
      ? { password: authData }
      : (authData ?? {})

    user.value = { username, director }
    _password.value = normalizedAuth.password ?? ''
    _sessionToken.value = normalizedAuth.sessionToken ?? ''
    persistUser()
    persistSessionToken()
  }

  function setDirector(director = DEFAULT_DIRECTOR_NAME) {
    if (!user.value) {
      return
    }

    user.value = {
      ...user.value,
      director: director || DEFAULT_DIRECTOR_NAME,
    }
    persistUser()
  }

  function logout() {
    user.value = null
    _password.value = ''
    _sessionToken.value = ''
    persistUser()
    persistSessionToken()
  }

  // Returns credentials object suitable for useDirectorStore.connect()
  function getCredentials() {
    if (!user.value) return null
    if (!_password.value && !_sessionToken.value) return null

    const credentials = {
      username:  user.value.username,
      director:  user.value.director || DEFAULT_DIRECTOR_NAME,
    }
    if (_password.value) {
      credentials.password = _password.value
    }
    if (_sessionToken.value) {
      credentials.sessionToken = _sessionToken.value
    }
    return credentials
  }

  return { user, isLoggedIn, login, setDirector, logout, getCredentials }
})
