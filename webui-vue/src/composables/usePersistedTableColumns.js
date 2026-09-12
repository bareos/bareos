import { computed, unref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'

/**
 * Column-visibility state for a q-table, persisted per `key` in the
 * settings store (localStorage) the same way search text and sort order
 * are persisted. `allColumns` is the full list of q-table column
 * definitions (or a ref/computed of it); columns whose `name` is not
 * "essential" can be toggled off by the user via a column-picker menu.
 *
 * Returns:
 * - `visibleColumns`: computed array of column definitions to pass to
 *   q-table's `:columns` prop.
 * - `toggleableColumns`: computed array of `{ name, label, visible }`
 *   entries for building a column-picker menu.
 * - `toggleColumn(name)`: flips a single column's visibility.
 */
export function usePersistedTableColumns(key, allColumns, options = {}) {
  const settings = useSettingsStore()
  const essential = new Set(options.essential ?? [])

  const hiddenColumns = computed({
    get: () => settings.getTableHiddenColumns(key, []),
    set: (value) => settings.setTableHiddenColumns(key, value),
  })

  const columns = computed(() => unref(allColumns))

  const visibleColumns = computed(() => {
    const hidden = new Set(hiddenColumns.value)
    return columns.value.filter(col => essential.has(col.name) || !hidden.has(col.name))
  })

  const toggleableColumns = computed(() => {
    const hidden = new Set(hiddenColumns.value)
    return columns.value
      .filter(col => !essential.has(col.name) && col.label)
      .map(col => ({ name: col.name, label: col.label, visible: !hidden.has(col.name) }))
  })

  function toggleColumn(name) {
    const hidden = new Set(hiddenColumns.value)
    if (hidden.has(name)) {
      hidden.delete(name)
    } else {
      hidden.add(name)
    }
    hiddenColumns.value = [...hidden]
  }

  // Drop stale column names (e.g. after a schema change) so an empty
  // persisted array doesn't linger in localStorage forever.
  watch(columns, (cols) => {
    const validNames = new Set(cols.map(col => col.name))
    const filtered = hiddenColumns.value.filter(name => validNames.has(name))
    if (filtered.length !== hiddenColumns.value.length) {
      hiddenColumns.value = filtered
    }
  })

  return { visibleColumns, toggleableColumns, toggleColumn }
}
