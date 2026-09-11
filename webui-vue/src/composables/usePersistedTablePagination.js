import { ref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'

export function usePersistedTablePagination(key, defaults = {}, options = {}) {
  const settings = useSettingsStore()
  const fallbackRowsPerPage = Number.isInteger(defaults.rowsPerPage) && defaults.rowsPerPage >= 0
    ? defaults.rowsPerPage
    : 15
  const { allowedRowsPerPage, persistSort = false } = options
  const storedRowsPerPage = settings.getTableRowsPerPage(key, fallbackRowsPerPage)
  // Guard against a value that was persisted before `allowedRowsPerPage` was
  // introduced (e.g. the "All" option, encoded as 0), which would otherwise
  // freeze the page again on every load before the user can reach the
  // rows-per-page selector to change it.
  const rowsPerPage = Array.isArray(allowedRowsPerPage) && !allowedRowsPerPage.includes(storedRowsPerPage)
    ? fallbackRowsPerPage
    : storedRowsPerPage

  const storedSort = persistSort
    ? settings.getTableSort(key, {
        sortBy: typeof defaults.sortBy === 'string' ? defaults.sortBy : '',
        descending: typeof defaults.descending === 'boolean' ? defaults.descending : false,
      })
    : {}

  const pagination = ref({
    ...defaults,
    ...storedSort,
    rowsPerPage,
  })

  watch(() => pagination.value.rowsPerPage, (value) => {
    settings.setTableRowsPerPage(key, value)
  }, { immediate: true })

  if (persistSort) {
    watch(
      () => [pagination.value.sortBy, pagination.value.descending],
      ([sortBy, descending]) => {
        settings.setTableSort(key, { sortBy, descending })
      },
      { immediate: true }
    )
  }

  return pagination
}
