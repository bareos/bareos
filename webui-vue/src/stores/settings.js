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
  selectedDirectors: [],
  tableRowsPerPage: {},
}

function normalizeSelectedDirectors(value) {
  if (!Array.isArray(value)) {
    return []
  }

  return [...new Set(
    value
      .map(item => String(item ?? '').trim())
      .filter(Boolean)
  )]
}

function normalizeTableRowsPerPage(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {}
  }

  return Object.fromEntries(
    Object.entries(value)
      .map(([key, rowsPerPage]) => [String(key).trim(), Number(rowsPerPage)])
      .filter(([key, rowsPerPage]) => key && Number.isInteger(rowsPerPage) && rowsPerPage >= 0)
  )
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
  const selectedDirectors = ref(normalizeSelectedDirectors(saved.selectedDirectors))
  const tableRowsPerPage = ref(normalizeTableRowsPerPage(saved.tableRowsPerPage))

  function save() {
    localStorage.setItem(LS_KEY, JSON.stringify({
      refreshInterval: refreshInterval.value,
      darkMode:        darkMode.value,
      relativeTime:    relativeTime.value,
      locale:          locale.value,
      loginUsername:   loginUsername.value,
      directorName:    directorName.value,
      selectedDirectors: selectedDirectors.value,
      tableRowsPerPage: tableRowsPerPage.value,
    }))
  }

  function setLocale(value) {
    locale.value = normalizeWebUiLocale(value)
  }

  function setSelectedDirectors(value) {
    selectedDirectors.value = normalizeSelectedDirectors(value)
  }

  function getTableRowsPerPage(key, fallback) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    const value = tableRowsPerPage.value[normalizedKey]
    return Number.isInteger(value) && value >= 0 ? value : fallback
  }

  function setTableRowsPerPage(key, value) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    const normalizedValue = Number(value)
    if (Number.isInteger(normalizedValue) && normalizedValue >= 0) {
      tableRowsPerPage.value = {
        ...tableRowsPerPage.value,
        [normalizedKey]: normalizedValue,
      }
      return
    }

    const { [normalizedKey]: _removed, ...rest } = tableRowsPerPage.value
    tableRowsPerPage.value = rest
  }

  watch(refreshInterval, save)
  watch(darkMode, save)
  watch(relativeTime, save)
  watch(loginUsername, save)
  watch(directorName, save)
  watch(selectedDirectors, save, { deep: true })
  watch(tableRowsPerPage, save, { deep: true })
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
    selectedDirectors,
    tableRowsPerPage,
    setLocale,
    setSelectedDirectors,
    getTableRowsPerPage,
    setTableRowsPerPage,
  }
})
