import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

export const useAuthStore = defineStore('auth', () => {
  const user = ref(JSON.parse(sessionStorage.getItem('bareos_user') || 'null'))
  // password is kept only in memory (never persisted) for WebSocket auth
  const _password = ref('')
  const isLoggedIn = computed(() => !!user.value)

  function login(username, director, password, host, port) {
    user.value = { username, director, host: host || 'localhost', port: port || 9101 }
    _password.value = password
    sessionStorage.setItem('bareos_user', JSON.stringify(user.value))
  }

  function logout() {
    user.value = null
    _password.value = ''
    sessionStorage.removeItem('bareos_user')
  }

  // Returns credentials object suitable for useDirectorStore.connect()
  function getCredentials() {
    if (!user.value) return null
    return {
      username:  user.value.username,
      password:  _password.value,
      director:  user.value.director,
      host:      user.value.host,
      port:      user.value.port,
    }
  }

  return { user, isLoggedIn, login, logout, getCredentials }
})
