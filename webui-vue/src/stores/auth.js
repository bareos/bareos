import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import {
  fetchCurrentSession,
  SESSION_AUTH_PASSWORD,
} from '../utils/sessionApi.js'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'

export const useAuthStore = defineStore('auth', () => {
  const user = ref(null)
  const _password = ref('')
  const initialized = ref(false)
  const isLoggedIn = computed(() => !!user.value && !!_password.value)
  let restorePromise = null

  function applyLogin(username, director = DEFAULT_DIRECTOR_NAME, password = SESSION_AUTH_PASSWORD) {
    user.value = { username, director }
    _password.value = password
    initialized.value = true
  }

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password = SESSION_AUTH_PASSWORD,
  ) {
    applyLogin(username, director, password)
  }

  function setDirector(director = DEFAULT_DIRECTOR_NAME) {
    if (!user.value) {
      return
    }

    user.value = {
      ...user.value,
      director: director || DEFAULT_DIRECTOR_NAME,
    }
  }

  function logout() {
    user.value = null
    _password.value = ''
    initialized.value = true
  }

  async function restoreSession(force = false) {
    if (initialized.value && !force) {
      return isLoggedIn.value
    }

    if (restorePromise && !force) {
      return restorePromise
    }

    restorePromise = (async () => {
      try {
        const session = await fetchCurrentSession()
        if (session?.username) {
          applyLogin(
            session.username,
            session.director || DEFAULT_DIRECTOR_NAME,
            SESSION_AUTH_PASSWORD
          )
        } else {
          logout()
        }
      } catch {
        logout()
      } finally {
        initialized.value = true
        restorePromise = null
      }

      return isLoggedIn.value
    })()

    return restorePromise
  }

  // Returns credentials object suitable for useDirectorStore.connect()
  function getCredentials() {
    if (!user.value || !_password.value) return null
    return {
      username:  user.value.username,
      password:  _password.value,
      director:  user.value.director || DEFAULT_DIRECTOR_NAME,
    }
  }

  return {
    user,
    isLoggedIn,
    initialized,
    login,
    setDirector,
    logout,
    restoreSession,
    getCredentials,
  }
})
