/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.
*/

import { computed, ref, unref } from 'vue'

import { useSettingsStore } from '../stores/settings.js'
import { collectCommonColumnValues } from '../utils/tableCommonValues.js'

/*
 * Collapses table columns that hold the very same value in every visible row
 * into a summary bar above the table. The user can pin a collapsed column back
 * into the table, and a persisted master switch turns the whole behaviour off.
 *
 * `columns` are the columns the table would show without collapsing, `rows`
 * are the rows it currently shows. Both may be refs.
 */
export function useCollapsedColumns(key, rows, columns, options = {}) {
  const settings = useSettingsStore()
  const flagKey = `${key}.collapseUniform`
  const pinned = ref(new Set())

  const enabled = computed({
    get: () => settings.getTableFlag(flagKey, options.defaultEnabled ?? true),
    set: (value) => {
      settings.setTableFlag(flagKey, value)
      if (!value) {
        pinned.value = new Set()
      }
    },
  })

  const collapsible = computed(() => {
    if (!enabled.value) {
      return []
    }
    return collectCommonColumnValues(unref(rows), unref(columns), {
      exclude: options.exclude ?? [],
      format: options.format ?? {},
    })
  })

  const collapsedColumns = computed(
    () => collapsible.value.filter((entry) => !pinned.value.has(entry.name)),
  )

  const tableColumns = computed(() => {
    const hidden = new Set(collapsedColumns.value.map((entry) => entry.name))
    return unref(columns).filter((column) => !hidden.has(column.name))
  })

  function pinColumn(name) {
    const next = new Set(pinned.value)
    next.add(name)
    pinned.value = next
  }

  return { enabled, collapsedColumns, tableColumns, pinColumn }
}
