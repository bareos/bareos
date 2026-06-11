import { defineStore } from 'pinia'
import { computed, ref } from 'vue'
import {
  fetchCurrentSession,
  SESSION_AUTH_PASSWORD,
} from '../utils/sessionApi.js'

export const DEFAULT_DIRECTOR_NAME = 'bareos-dir'

function normalizeDirectorName(value) {
  const director = String(value ?? '').trim()
  return director || DEFAULT_DIRECTOR_NAME
}

function normalizeDirectorUsers(session) {
  const byDirector = {}

  for (const entry of session?.authenticatedDirectors ?? []) {
    const director = normalizeDirectorName(entry?.director)
    const username = String(entry?.username ?? '').trim()
    if (!username) {
      continue
    }
    byDirector[director] = { username }
  }

  if (Object.keys(byDirector).length === 0) {
    const username = String(session?.username ?? '').trim()
    const director = normalizeDirectorName(session?.currentDirector ?? session?.director)
    if (username) {
      byDirector[director] = { username }
    }
  }

  return byDirector
}

function firstAuthenticatedDirector(directorUsers) {
  return Object.keys(directorUsers)[0] ?? ''
}

export const useAuthStore = defineStore('auth', () => {
  const user = ref(null)
  const _password = ref('')
  const initialized = ref(false)
  const directorUsers = ref({})
  const authenticatedDirectors = computed(() => Object.keys(directorUsers.value))
  const directorSessions = computed(() => authenticatedDirectors.value.map((director) => ({
    director,
    username: getDirectorUsername(director),
    current: user.value?.director === director,
  })))
  const isLoggedIn = computed(() => (
    !!user.value && !!_password.value && authenticatedDirectors.value.length > 0
  ))
  let restorePromise = null

  function setCurrentUser(director = DEFAULT_DIRECTOR_NAME) {
    const normalizedDirector = normalizeDirectorName(director)
    const directorUser = directorUsers.value[normalizedDirector]
    if (!directorUser?.username) {
      user.value = null
      return false
    }

    user.value = {
      username: directorUser.username,
      director: normalizedDirector,
    }
    return true
  }

  function applyLogin(username, director = DEFAULT_DIRECTOR_NAME,
    password = SESSION_AUTH_PASSWORD, options = {}) {
    const normalizedDirector = normalizeDirectorName(director)
    const normalizedUsername = String(username ?? '').trim()
    if (!normalizedUsername) {
      return
    }

    directorUsers.value = {
      ...directorUsers.value,
      [normalizedDirector]: { username: normalizedUsername },
    }
    _password.value = password
    if (options.setCurrent !== false || !user.value) {
      setCurrentUser(normalizedDirector)
    }
    initialized.value = true
  }

  function applySession(session, password = SESSION_AUTH_PASSWORD) {
    const nextDirectorUsers = normalizeDirectorUsers(session)
    directorUsers.value = nextDirectorUsers
    _password.value = Object.keys(nextDirectorUsers).length > 0 ? password : ''
    const currentDirector = normalizeDirectorName(
      session?.currentDirector
      ?? session?.director
      ?? firstAuthenticatedDirector(nextDirectorUsers)
    )
    if (!setCurrentUser(currentDirector)) {
      user.value = null
    }
    initialized.value = true
  }

  function login(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password = SESSION_AUTH_PASSWORD,
  ) {
    applyLogin(username, director, password)
  }

  function loginDirector(
    username,
    director = DEFAULT_DIRECTOR_NAME,
    password = SESSION_AUTH_PASSWORD,
    options = {},
  ) {
    applyLogin(username, director, password, options)
  }

  function getDirectorUsername(director = DEFAULT_DIRECTOR_NAME) {
    return directorUsers.value[normalizeDirectorName(director)]?.username ?? ''
  }

  function hasDirectorSession(director = DEFAULT_DIRECTOR_NAME) {
    return !!directorUsers.value[normalizeDirectorName(director)]
  }

  function missingDirectorSessions(directors = []) {
    return [...new Set(
      (Array.isArray(directors) ? directors : [])
        .map(value => normalizeDirectorName(value))
        .filter(Boolean)
    )].filter(director => !hasDirectorSession(director))
  }

  function setDirector(director = DEFAULT_DIRECTOR_NAME) {
    if (!isLoggedIn.value) {
      return false
    }

    return setCurrentUser(director)
  }

  function removeDirector(director = DEFAULT_DIRECTOR_NAME) {
    const normalizedDirector = normalizeDirectorName(director)
    if (!directorUsers.value[normalizedDirector]) {
      return false
    }

    const nextDirectorUsers = { ...directorUsers.value }
    delete nextDirectorUsers[normalizedDirector]
    directorUsers.value = nextDirectorUsers

    if (Object.keys(nextDirectorUsers).length === 0) {
      logout()
      return true
    }

    if (user.value?.director === normalizedDirector) {
      setCurrentUser(firstAuthenticatedDirector(nextDirectorUsers))
    }
    return true
  }

  function logout() {
    user.value = null
    directorUsers.value = {}
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
        if (session) {
          applySession(session, SESSION_AUTH_PASSWORD)
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

  function getCredentials(director = user.value?.director || DEFAULT_DIRECTOR_NAME) {
    const normalizedDirector = normalizeDirectorName(director)
    const username = getDirectorUsername(normalizedDirector)
    if (!username || !_password.value) {
      return null
    }
    return {
      username,
      password: _password.value,
      director: normalizedDirector,
    }
  }

  return {
    user,
    initialized,
    isLoggedIn,
    directorUsers,
    authenticatedDirectors,
    directorSessions,
    login,
    loginDirector,
    getDirectorUsername,
    hasDirectorSession,
    missingDirectorSessions,
    setDirector,
    removeDirector,
    logout,
    restoreSession,
    getCredentials,
    applySession,
  }
})
