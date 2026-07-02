import { ref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'

export function usePersistedTablePagination(key, defaults = {}) {
  const settings = useSettingsStore()
  const rowsPerPage = settings.getTableRowsPerPage(
    key,
    Number.isInteger(defaults.rowsPerPage) && defaults.rowsPerPage >= 0
      ? defaults.rowsPerPage
      : 15
  )

  const pagination = ref({
    ...defaults,
    rowsPerPage,
  })

  watch(() => pagination.value.rowsPerPage, (value) => {
    settings.setTableRowsPerPage(key, value)
  }, { immediate: true })

  return pagination
}
