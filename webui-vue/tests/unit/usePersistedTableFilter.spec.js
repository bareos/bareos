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

import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'
import { usePersistedTableFilter } from '../../src/composables/usePersistedTableFilter.js'

const LS_KEY = 'bareos_settings'

describe('usePersistedTableFilter', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('restores a previously persisted search term', () => {
    localStorage.setItem(LS_KEY, JSON.stringify({
      tableFilter: { 'storages.volumes': 'LTO-1' },
    }))

    const filter = usePersistedTableFilter('storages.volumes')

    expect(filter.value).toBe('LTO-1')
  })

  it('falls back to the given default when nothing is persisted', () => {
    const filter = usePersistedTableFilter('storages.pools', 'default-term')

    expect(filter.value).toBe('default-term')
  })

  it('persists updates to the search term', async () => {
    const filter = usePersistedTableFilter('schedules.show')

    filter.value = 'weekly'
    await nextTick()

    expect(JSON.parse(localStorage.getItem(LS_KEY))).toEqual(
      expect.objectContaining({
        tableFilter: { 'schedules.show': 'weekly' },
      })
    )

    filter.value = ''
    await nextTick()

    expect(JSON.parse(localStorage.getItem(LS_KEY)).tableFilter).toEqual({})
  })
})
