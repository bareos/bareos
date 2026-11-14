/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

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
