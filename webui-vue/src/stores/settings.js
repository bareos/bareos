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
  refreshInterval: 30,  // seconds
  darkMode: false,
  relativeTime: false,  // show timestamps as relative ("2 hours ago") or absolute
  locale: detectPreferredLocale(),
  loginUsername: 'admin',
  directorName: DEFAULT_DIRECTOR_NAME,
  selectedDirectors: [],
  tableRowsPerPage: {},
  tableSort: {},
  tableFilter: {},
  tableHiddenColumns: {},
  schedulesViewMode: 'week',
  clientBackupWarningDays: 2,
}

function normalizeBoolean(value, fallback) {
  return typeof value === 'boolean' ? value : fallback
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

function normalizeTableSort(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {}
  }

  return Object.fromEntries(
    Object.entries(value)
      .map(([key, sort]) => [
        String(key).trim(),
        typeof sort === 'object' && sort !== null && !Array.isArray(sort)
          ? {
              sortBy: typeof sort.sortBy === 'string' ? sort.sortBy : '',
              descending: normalizeBoolean(sort.descending, false),
            }
          : null,
      ])
      .filter(([key, sort]) => key && sort && sort.sortBy)
  )
}

function normalizeTableFilter(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {}
  }

  return Object.fromEntries(
    Object.entries(value)
      .map(([key, filter]) => [String(key).trim(), String(filter ?? '')])
      .filter(([key, filter]) => key && filter)
  )
}

function normalizeTableHiddenColumns(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {}
  }

  return Object.fromEntries(
    Object.entries(value)
      .map(([key, hidden]) => [
        String(key).trim(),
        Array.isArray(hidden)
          ? [...new Set(hidden.map(name => String(name ?? '').trim()).filter(Boolean))]
          : [],
      ])
      .filter(([key, hidden]) => key && hidden.length)
  )
}

function normalizeSchedulesViewMode(value, fallback = DEFAULTS.schedulesViewMode) {
  return value === 'month' || value === 'week' ? value : fallback
}

function normalizeClientBackupWarningDays(value, fallback = DEFAULTS.clientBackupWarningDays) {
  const normalized = Number(value)
  return Number.isInteger(normalized) && normalized > 0 && normalized <= 365
    ? normalized
    : fallback
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
  const tableSort = ref(normalizeTableSort(saved.tableSort))
  const tableFilter = ref(normalizeTableFilter(saved.tableFilter))
  const tableHiddenColumns = ref(normalizeTableHiddenColumns(saved.tableHiddenColumns))
  const schedulesViewMode = ref(normalizeSchedulesViewMode(saved.schedulesViewMode))
  const clientBackupWarningDays = ref(
    normalizeClientBackupWarningDays(saved.clientBackupWarningDays)
  )
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
      tableSort: tableSort.value,
      tableFilter: tableFilter.value,
      tableHiddenColumns: tableHiddenColumns.value,
      schedulesViewMode: schedulesViewMode.value,
      clientBackupWarningDays: clientBackupWarningDays.value,
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

  function getTableSort(key, fallback = {}) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    const value = tableSort.value[normalizedKey]
    return value ? { ...fallback, ...value } : fallback
  }

  function setTableSort(key, value) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    const normalized = normalizeTableSort({ [normalizedKey]: value })
    const sort = normalized[normalizedKey]
    if (sort) {
      tableSort.value = {
        ...tableSort.value,
        [normalizedKey]: sort,
      }
      return
    }

    const { [normalizedKey]: _removed, ...rest } = tableSort.value
    tableSort.value = rest
  }

  function getTableFilter(key, fallback = '') {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    return tableFilter.value[normalizedKey] ?? fallback
  }

  function setTableFilter(key, value) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    const normalizedValue = String(value ?? '')
    if (normalizedValue) {
      tableFilter.value = {
        ...tableFilter.value,
        [normalizedKey]: normalizedValue,
      }
      return
    }

    const { [normalizedKey]: _removed, ...rest } = tableFilter.value
    tableFilter.value = rest
  }

  function setSchedulesViewMode(value) {
    schedulesViewMode.value = normalizeSchedulesViewMode(value, schedulesViewMode.value)
  }

  function getTableHiddenColumns(key, fallback = []) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    return tableHiddenColumns.value[normalizedKey] ?? fallback
  }

  function setTableHiddenColumns(key, value) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    const normalized = normalizeTableHiddenColumns({ [normalizedKey]: value })
    const hidden = normalized[normalizedKey]
    if (hidden?.length) {
      tableHiddenColumns.value = {
        ...tableHiddenColumns.value,
        [normalizedKey]: hidden,
      }
      return
    }

    const { [normalizedKey]: _removed, ...rest } = tableHiddenColumns.value
    tableHiddenColumns.value = rest
  }

  // ── backup / restore ─────────────────────────────────────────────────────

  /**
   * Serialisable snapshot of user settings, suitable for JSON export.
   * Excludes `loginUsername`: it is only a per-browser convenience default
   * for the login form and should not be silently carried over when
   * restoring a backup into a different browser/profile.
   */
  function exportSettings() {
    return {
      refreshInterval: refreshInterval.value,
      darkMode: darkMode.value,
      relativeTime: relativeTime.value,
      locale: locale.value,
      directorName: directorName.value,
      selectedDirectors: selectedDirectors.value,
      tableRowsPerPage: tableRowsPerPage.value,
      tableSort: tableSort.value,
      tableHiddenColumns: tableHiddenColumns.value,
      schedulesViewMode: schedulesViewMode.value,
      clientBackupWarningDays: clientBackupWarningDays.value,
    }
  }

  /**
   * Apply a previously exported settings snapshot. Unknown/missing fields
   * are left untouched; recognised fields are re-validated with the same
   * normalisers used when loading from localStorage.
   */
  function importSettings(data) {
    if (!data || typeof data !== 'object' || Array.isArray(data)) {
      throw new Error('This file does not contain a settings backup.')
    }
    if ('refreshInterval' in data) {
      const value = Number(data.refreshInterval)
      if (Number.isInteger(value) && value > 0) refreshInterval.value = value
    }
    if ('darkMode' in data) {
      darkMode.value = normalizeBoolean(data.darkMode, darkMode.value)
    }
    if ('relativeTime' in data) {
      relativeTime.value = normalizeBoolean(data.relativeTime, relativeTime.value)
    }
    if ('locale' in data) {
      locale.value = normalizeWebUiLocale(data.locale)
    }
    if ('directorName' in data && data.directorName != null) {
      directorName.value = String(data.directorName)
    }
    if ('selectedDirectors' in data) {
      selectedDirectors.value = normalizeSelectedDirectors(data.selectedDirectors)
    }
    if ('tableRowsPerPage' in data) {
      tableRowsPerPage.value = normalizeTableRowsPerPage(data.tableRowsPerPage)
    }
    if ('tableSort' in data) {
      tableSort.value = normalizeTableSort(data.tableSort)
    }
    if ('tableHiddenColumns' in data) {
      tableHiddenColumns.value = normalizeTableHiddenColumns(data.tableHiddenColumns)
    }
    if ('schedulesViewMode' in data) {
      schedulesViewMode.value = normalizeSchedulesViewMode(
        data.schedulesViewMode,
        schedulesViewMode.value
      )
    }
    if ('clientBackupWarningDays' in data) {
      clientBackupWarningDays.value = normalizeClientBackupWarningDays(
        data.clientBackupWarningDays,
        clientBackupWarningDays.value
      )
    }
  }

  watch(refreshInterval, save)
  watch(darkMode, save)
  watch(relativeTime, save)
  watch(loginUsername, save)
  watch(directorName, save)
  watch(selectedDirectors, save, { deep: true })
  watch(tableRowsPerPage, save, { deep: true })
  watch(tableSort, save, { deep: true })
  watch(tableFilter, save, { deep: true })
  watch(tableHiddenColumns, save, { deep: true })
  watch(schedulesViewMode, save)
  watch(clientBackupWarningDays, (value) => {
    clientBackupWarningDays.value = normalizeClientBackupWarningDays(value)
    save()
  })
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
    tableSort,
    tableFilter,
    tableHiddenColumns,
    schedulesViewMode,
    clientBackupWarningDays,
    setLocale,
    setSelectedDirectors,
    getTableRowsPerPage,
    setTableRowsPerPage,
    getTableSort,
    setTableSort,
    getTableFilter,
    setTableFilter,
    getTableHiddenColumns,
    setTableHiddenColumns,
    setSchedulesViewMode,
    exportSettings,
    importSettings,
  }
})
