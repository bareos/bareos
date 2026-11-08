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

// Options for a searchable QSelect: `filtered` follows `options`, and
// `filter` is the handler for the QSelect @filter event.
export function useFilteredOptions(options) {
  const filtered = ref([])
  watch(options, (value) => { filtered.value = value }, { immediate: true })

  function filter(value, update) {
    update(() => {
      if (!value) {
        filtered.value = options.value
        return
      }
      const needle = value.toLowerCase()
      filtered.value = options.value.filter(option => (
        option.label.toLowerCase().includes(needle)
      ))
    })
  }

  return { filtered, filter }
}
