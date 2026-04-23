import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'
export const DEFAULT_DIRECTOR_HOST = import.meta.env.VITE_DIRECTOR_HOST || 'localhost'
export const DEFAULT_DIRECTOR_PORT = Number(import.meta.env.VITE_DIRECTOR_PORT) || 9101

export const useAuthStore = defineStore('auth', () => {
  const user = ref(JSON.parse(sessionStorage.getItem('bareos_user') || 'null'))
  // Password is kept in sessionStorage (tab-scoped, cleared on close) so the
  // app can reconnect the WebSocket after a page reload without asking again.
  const _password = ref(sessionStorage.getItem('bareos_pass') || '')
  const isLoggedIn = computed(() => !!user.value)

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password,
    host = DEFAULT_DIRECTOR_HOST,
    port = DEFAULT_DIRECTOR_PORT,
  ) {
    user.value = { username, director, host, port }
    _password.value = password
    sessionStorage.setItem('bareos_user', JSON.stringify(user.value))
    sessionStorage.setItem('bareos_pass', password)
  }

  function logout() {
    user.value = null
    _password.value = ''
    sessionStorage.removeItem('bareos_user')
    sessionStorage.removeItem('bareos_pass')
  }

  // Returns credentials object suitable for useDirectorStore.connect()
  function getCredentials() {
    if (!user.value) return null
    return {
      username:  user.value.username,
      password:  _password.value,
      director:  user.value.director || DEFAULT_DIRECTOR_NAME,
      host:      user.value.host || DEFAULT_DIRECTOR_HOST,
      port:      user.value.port || DEFAULT_DIRECTOR_PORT,
    }
  }

  return { user, isLoggedIn, login, logout, getCredentials }
})
