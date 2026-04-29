import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'

export const useAuthStore = defineStore('auth', () => {
  const user = ref(JSON.parse(sessionStorage.getItem('bareos_user') || 'null'))
  // Password is kept in sessionStorage (tab-scoped, cleared on close) so the
  // app can reconnect the WebSocket after a page reload without asking again.
  const _password = ref(sessionStorage.getItem('bareos_pass') || '')
  const isLoggedIn = computed(() => !!user.value)

  function persistUser() {
    if (user.value) {
      sessionStorage.setItem('bareos_user', JSON.stringify(user.value))
    } else {
      sessionStorage.removeItem('bareos_user')
    }
  }

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password,
  ) {
    user.value = { username, director }
    _password.value = password
    persistUser()
    sessionStorage.setItem('bareos_pass', password)
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
    persistUser()
    sessionStorage.removeItem('bareos_pass')
  }

  // Returns credentials object suitable for useDirectorStore.connect()
  function getCredentials() {
    if (!user.value) return null
    return {
      username:  user.value.username,
      password:  _password.value,
      director:  user.value.director || DEFAULT_DIRECTOR_NAME,
    }
  }

  return { user, isLoggedIn, login, setDirector, logout, getCredentials }
})
