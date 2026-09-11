import { ref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'

/**
 * A `ref` holding a q-table search string, persisted per `key` in the
 * settings store (localStorage) the same way `usePersistedTablePagination`
 * persists rows-per-page/sort, so a search term survives navigation and
 * page reloads.
 */
export function usePersistedTableFilter(key, defaultValue = '') {
  const settings = useSettingsStore()
  const filter = ref(settings.getTableFilter(key, defaultValue))

  watch(filter, (value) => {
    settings.setTableFilter(key, value)
  })

  return filter
}
