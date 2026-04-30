import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { DEFAULT_DIRECTOR_NAME } from './auth.js'
import { setI18nLocale } from '../i18n/index.js'
import {
  applyDocumentLocale,
  detectPreferredLocale,
  normalizeWebUiLocale,
} from '../utils/locales.js'

const LS_KEY = 'bareos_settings'

const DEFAULTS = {
  refreshInterval: 10,  // seconds
  darkMode: false,
  relativeTime: false,  // show timestamps as relative ("2 hours ago") or absolute
  locale: detectPreferredLocale(),
  loginUsername: 'admin',
  directorName: DEFAULT_DIRECTOR_NAME,
}

function loadFromStorage() {
  try {
    const raw = localStorage.getItem(LS_KEY)
    if (raw) return { ...DEFAULTS, ...JSON.parse(raw) }
  } catch { /* ignore */ }
  return { ...DEFAULTS }
}

export const useSettingsStore = defineStore('settings', () => {
  const saved = loadFromStorage()

  const refreshInterval = ref(saved.refreshInterval)
  const darkMode        = ref(saved.darkMode)
  const relativeTime    = ref(saved.relativeTime)
  const locale          = ref(normalizeWebUiLocale(saved.locale))
  const loginUsername   = ref(saved.loginUsername)
  const directorName    = ref(saved.directorName)

  function save() {
    localStorage.setItem(LS_KEY, JSON.stringify({
      refreshInterval: refreshInterval.value,
      darkMode:        darkMode.value,
      relativeTime:    relativeTime.value,
      locale:          locale.value,
      loginUsername:   loginUsername.value,
      directorName:    directorName.value,
    }))
  }

  function setLocale(value) {
    locale.value = normalizeWebUiLocale(value)
  }

  watch(refreshInterval, save)
  watch(darkMode, save)
  watch(relativeTime, save)
  watch(loginUsername, save)
  watch(directorName, save)
  watch(locale, (value) => {
    applyDocumentLocale(value)
    setI18nLocale(value)
    save()
  }, { immediate: true })

  return {
    refreshInterval,
    darkMode,
    relativeTime,
    locale,
    loginUsername,
    directorName,
    setLocale,
  }
})
