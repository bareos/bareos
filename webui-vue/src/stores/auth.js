import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'
const STAGED_SETUP_PASSWORD_KEY = 'bareos_setup_pass'

export const useAuthStore = defineStore('auth', () => {
  const user = ref(JSON.parse(sessionStorage.getItem('bareos_user') || 'null'))
  // Password is kept in sessionStorage (tab-scoped, cleared on close) so the
  // app can reconnect the WebSocket after a page reload without asking again.
  const _password = ref(sessionStorage.getItem('bareos_pass') || '')
  const _stagedSetupPassword = ref(sessionStorage.getItem(STAGED_SETUP_PASSWORD_KEY) || '')
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
    clearStagedSetupPassword()
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
    clearStagedSetupPassword()
  }

  function stageSetupPassword(password) {
    _stagedSetupPassword.value = typeof password === 'string' ? password : ''
    if (_stagedSetupPassword.value) {
      sessionStorage.setItem(STAGED_SETUP_PASSWORD_KEY, _stagedSetupPassword.value)
    } else {
      sessionStorage.removeItem(STAGED_SETUP_PASSWORD_KEY)
    }
  }

  function getStagedSetupPassword() {
    return _stagedSetupPassword.value
  }

  function clearStagedSetupPassword() {
    _stagedSetupPassword.value = ''
    sessionStorage.removeItem(STAGED_SETUP_PASSWORD_KEY)
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

  return {
    user,
    isLoggedIn,
    login,
    setDirector,
    logout,
    stageSetupPassword,
    getStagedSetupPassword,
    clearStagedSetupPassword,
    getCredentials,
  }
})
