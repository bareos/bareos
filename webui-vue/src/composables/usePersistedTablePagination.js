import { ref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'

export function usePersistedTablePagination(key, defaults = {}, options = {}) {
  const settings = useSettingsStore()
  const fallbackRowsPerPage = Number.isInteger(defaults.rowsPerPage) && defaults.rowsPerPage >= 0
    ? defaults.rowsPerPage
    : 15
  const { allowedRowsPerPage } = options
  const storedRowsPerPage = settings.getTableRowsPerPage(key, fallbackRowsPerPage)
  // Guard against a value that was persisted before `allowedRowsPerPage` was
  // introduced (e.g. the "All" option, encoded as 0), which would otherwise
  // freeze the page again on every load before the user can reach the
  // rows-per-page selector to change it.
  const rowsPerPage = Array.isArray(allowedRowsPerPage) && !allowedRowsPerPage.includes(storedRowsPerPage)
    ? fallbackRowsPerPage
    : storedRowsPerPage

  const pagination = ref({
    ...defaults,
    rowsPerPage,
  })

  watch(() => pagination.value.rowsPerPage, (value) => {
    settings.setTableRowsPerPage(key, value)
  }, { immediate: true })

  return pagination
}
