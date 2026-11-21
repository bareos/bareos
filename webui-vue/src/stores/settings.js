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
  tableFlags: {},
  schedulesViewMode: 'week',
  jobsTimelineViewMode: 'week',
  clientBackupWarningFailedJobs: 2,
}

function normalizeTableFlags(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    return {}
  }

  const result = {}
  for (const [key, flag] of Object.entries(value)) {
    const normalizedKey = String(key ?? '').trim()
    if (normalizedKey && typeof flag === 'boolean') {
      result[normalizedKey] = flag
    }
  }
  return result
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
              sortBy: sort.sortBy === null || typeof sort.sortBy === 'string' ? sort.sortBy : '',
              descending: normalizeBoolean(sort.descending, false),
            }
          : null,
      ])
      .filter(([key, sort]) => key && sort && (sort.sortBy === null || sort.sortBy))
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

  // An explicitly stored empty list is kept: it records that the user
  // re-enabled columns a table hides by default.
  return Object.fromEntries(
    Object.entries(value)
      .filter(([, hidden]) => Array.isArray(hidden))
      .map(([key, hidden]) => [
        String(key).trim(),
        [...new Set(hidden.map(name => String(name ?? '').trim()).filter(Boolean))],
      ])
      .filter(([key]) => key)
  )
}

function normalizeSchedulesViewMode(value, fallback = DEFAULTS.schedulesViewMode) {
  return value === 'month' || value === 'week' ? value : fallback
}

function normalizeJobsTimelineViewMode(value, fallback = DEFAULTS.jobsTimelineViewMode) {
  return value === 'month' || value === 'week' ? value : fallback
}

function normalizeClientBackupWarningFailedJobs(value, fallback = DEFAULTS.clientBackupWarningFailedJobs) {
  const normalized = Number(value)
  return Number.isInteger(normalized) && normalized > 0 && normalized <= 1000
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
  const tableFlags = ref(normalizeTableFlags(saved.tableFlags))
  const schedulesViewMode = ref(normalizeSchedulesViewMode(saved.schedulesViewMode))
  const jobsTimelineViewMode = ref(normalizeJobsTimelineViewMode(saved.jobsTimelineViewMode))
  const clientBackupWarningFailedJobs = ref(
    normalizeClientBackupWarningFailedJobs(saved.clientBackupWarningFailedJobs)
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
      tableFlags: tableFlags.value,
      schedulesViewMode: schedulesViewMode.value,
      jobsTimelineViewMode: jobsTimelineViewMode.value,
      clientBackupWarningFailedJobs: clientBackupWarningFailedJobs.value,
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

  // Generic per-table boolean preferences (collapsing, filter toggles, ...).
  function getTableFlag(key, fallback = false) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    return tableFlags.value[normalizedKey] ?? fallback
  }

  function setTableFlag(key, value) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    tableFlags.value = {
      ...tableFlags.value,
      [normalizedKey]: Boolean(value),
    }
  }

  function setSchedulesViewMode(value) {
    schedulesViewMode.value = normalizeSchedulesViewMode(value, schedulesViewMode.value)
  }

  function setJobsTimelineViewMode(value) {
    jobsTimelineViewMode.value = normalizeJobsTimelineViewMode(value, jobsTimelineViewMode.value)
  }

  function getTableHiddenColumns(key, fallback = []) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return fallback
    }

    return tableHiddenColumns.value[normalizedKey] ?? fallback
  }

  function setTableHiddenColumns(key, value, { keepEmpty = false } = {}) {
    const normalizedKey = String(key ?? '').trim()
    if (!normalizedKey) {
      return
    }

    const normalized = normalizeTableHiddenColumns({ [normalizedKey]: value })
    const hidden = normalized[normalizedKey]
    if (hidden?.length || (hidden && keepEmpty)) {
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
   * Detached serialisable snapshot of user settings, suitable for JSON export.
   * Excludes `loginUsername`: it is only a per-browser convenience default
   * for the login form and should not be silently carried over when
   * restoring a backup into a different browser/profile.
   */
  function exportSettings() {
    return JSON.parse(JSON.stringify({
      refreshInterval: refreshInterval.value,
      darkMode: darkMode.value,
      relativeTime: relativeTime.value,
      locale: locale.value,
      directorName: directorName.value,
      selectedDirectors: selectedDirectors.value,
      tableRowsPerPage: tableRowsPerPage.value,
      tableSort: tableSort.value,
      tableFilter: tableFilter.value,
      tableHiddenColumns: tableHiddenColumns.value,
      tableFlags: tableFlags.value,
      schedulesViewMode: schedulesViewMode.value,
      jobsTimelineViewMode: jobsTimelineViewMode.value,
      clientBackupWarningFailedJobs: clientBackupWarningFailedJobs.value,
    }))
  }

  /**
   * Prepare a previously exported settings snapshot for later application.
   * Unknown/missing fields are left untouched; recognised fields are
   * re-validated with the same normalisers used when loading from localStorage.
   */
  function prepareSettingsImport(data) {
    if (!data || typeof data !== 'object' || Array.isArray(data)) {
      throw new Error('This file does not contain a settings backup.')
    }
    const updates = []
    if ('refreshInterval' in data) {
      const value = Number(data.refreshInterval)
      if (Number.isInteger(value) && value > 0) updates.push([refreshInterval, value])
    }
    if ('darkMode' in data) {
      updates.push([darkMode, normalizeBoolean(data.darkMode, darkMode.value)])
    }
    if ('relativeTime' in data) {
      updates.push([relativeTime, normalizeBoolean(data.relativeTime, relativeTime.value)])
    }
    if ('locale' in data) {
      updates.push([locale, normalizeWebUiLocale(data.locale)])
    }
    if ('directorName' in data && data.directorName != null) {
      updates.push([directorName, String(data.directorName)])
    }
    if ('selectedDirectors' in data) {
      updates.push([selectedDirectors, normalizeSelectedDirectors(data.selectedDirectors)])
    }
    if ('tableRowsPerPage' in data) {
      updates.push([tableRowsPerPage, normalizeTableRowsPerPage(data.tableRowsPerPage)])
    }
    if ('tableSort' in data) {
      updates.push([tableSort, normalizeTableSort(data.tableSort)])
    }
    if ('tableFilter' in data) {
      updates.push([tableFilter, normalizeTableFilter(data.tableFilter)])
    }
    if ('tableHiddenColumns' in data) {
      updates.push([tableHiddenColumns, normalizeTableHiddenColumns(data.tableHiddenColumns)])
    }

    if ('tableFlags' in data) {
      updates.push([tableFlags, normalizeTableFlags(data.tableFlags)])
    }
    if ('schedulesViewMode' in data) {
      updates.push([schedulesViewMode, normalizeSchedulesViewMode(
        data.schedulesViewMode,
        schedulesViewMode.value
      )])
    }
    if ('jobsTimelineViewMode' in data) {
      updates.push([jobsTimelineViewMode, normalizeJobsTimelineViewMode(
        data.jobsTimelineViewMode,
        jobsTimelineViewMode.value
      )])
    }
    if ('clientBackupWarningFailedJobs' in data) {
      updates.push([clientBackupWarningFailedJobs, normalizeClientBackupWarningFailedJobs(
        data.clientBackupWarningFailedJobs,
        clientBackupWarningFailedJobs.value
      )])
    }
    return () => {
      for (const [target, value] of updates) target.value = value
    }
  }

  function importSettings(data) {
    prepareSettingsImport(data)()
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
  watch(tableFlags, save, { deep: true })
  watch(schedulesViewMode, save)
  watch(jobsTimelineViewMode, save)
  watch(clientBackupWarningFailedJobs, (value) => {
    clientBackupWarningFailedJobs.value = normalizeClientBackupWarningFailedJobs(value)
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
    tableFlags,
    schedulesViewMode,
    jobsTimelineViewMode,
    clientBackupWarningFailedJobs,
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
    getTableFlag,
    setTableFlag,
    setSchedulesViewMode,
    setJobsTimelineViewMode,
    exportSettings,
    importSettings,
    prepareSettingsImport,
  }
})
