import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'

export const useAuthStore = defineStore('auth', () => {
  sessionStorage.removeItem('bareos_user')
  sessionStorage.removeItem('bareos_pass')

  const user = ref(null)
  const _password = ref('')
  const isLoggedIn = computed(() => !!user.value && !!_password.value)

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password,
  ) {
    user.value = { username, director }
    _password.value = password
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

  return { user, isLoggedIn, login, setDirector, logout, getCredentials }
})
