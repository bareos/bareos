import { computed, unref, watch } from 'vue'
import { useSettingsStore } from '../stores/settings.js'
import { columnIsUniform } from '../utils/tableCommonValues.js'

/**
 * Column-visibility state for a q-table, persisted per `key` in the
 * settings store (localStorage) the same way search text and sort order
 * are persisted. `allColumns` is the full list of q-table column
 * definitions (or a ref/computed of it); columns whose `name` is not
 * "essential" can be toggled off by the user via a column-picker menu.
 * `options.defaultHidden` lists columns hidden until the user changes the
 * column selection for this table. When `options.autoRevealRows` is given,
 * a default-hidden column is revealed again as soon as those rows hold more
 * than one distinct value for it — an explicit user choice always wins.
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

  const defaultHidden = options.defaultHidden ?? []

  const hiddenColumns = computed({
    get: () => settings.getTableHiddenColumns(key, defaultHidden),
    set: (value) => settings.setTableHiddenColumns(key, value, {
      keepEmpty: defaultHidden.length > 0,
    }),
  })

  const columns = computed(() => unref(allColumns))

  // `null` as fallback tells us whether the user ever touched this table.
  const userChoseColumns = computed(
    () => settings.getTableHiddenColumns(key, null) !== null,
  )

  const autoRevealed = computed(() => {
    const revealed = new Set()
    if (userChoseColumns.value || !options.autoRevealRows) {
      return revealed
    }

    const rows = unref(options.autoRevealRows)
    for (const column of columns.value) {
      if (defaultHidden.includes(column.name) && !columnIsUniform(rows, column)) {
        revealed.add(column.name)
      }
    }
    return revealed
  })

  const visibleColumns = computed(() => {
    const hidden = new Set(hiddenColumns.value)
    return columns.value.filter(col => (
      essential.has(col.name)
      || !hidden.has(col.name)
      || autoRevealed.value.has(col.name)
    ))
  })

  const toggleableColumns = computed(() => {
    const hidden = new Set(hiddenColumns.value)
    return columns.value
      .filter(col => !essential.has(col.name) && col.label)
      .map(col => ({
        name: col.name,
        label: col.label,
        visible: !hidden.has(col.name) || autoRevealed.value.has(col.name),
      }))
  })

  function toggleColumn(name) {
    const hidden = new Set(hiddenColumns.value)
    // Turning an auto-revealed column off has to persist the whole current
    // selection, otherwise the automatic rule would immediately undo it.
    if (autoRevealed.value.has(name)) {
      hidden.delete(name)
      hiddenColumns.value = [...hidden, name]
      return
    }
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
