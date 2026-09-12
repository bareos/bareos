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
import { nextTick, ref } from 'vue'
import { usePersistedTableColumns } from '../../src/composables/usePersistedTableColumns.js'

const LS_KEY = 'bareos_settings'

const COLUMNS = [
  { name: 'name', label: 'Name', field: 'name' },
  { name: 'pooltype', label: 'Type', field: 'pooltype' },
  { name: 'numvols', label: 'Volumes', field: 'numvols' },
  { name: 'actions', label: '', field: 'actions' },
]

describe('usePersistedTableColumns', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('shows all columns by default', () => {
    const { visibleColumns, toggleableColumns } = usePersistedTableColumns('storages.pools', COLUMNS, {
      essential: ['name', 'actions'],
    })

    expect(visibleColumns.value.map(c => c.name)).toEqual(['name', 'pooltype', 'numvols', 'actions'])
    // The unlabeled "actions" column and essential columns are not
    // offered in the picker; essential columns can't be hidden, and
    // unlabeled columns have nothing meaningful to show in a menu.
    expect(toggleableColumns.value.map(c => c.name)).toEqual(['pooltype', 'numvols'])
    expect(toggleableColumns.value.every(c => c.visible)).toBe(true)
  })

  it('restores previously hidden columns and keeps essential columns visible', () => {
    localStorage.setItem(LS_KEY, JSON.stringify({
      tableHiddenColumns: { 'storages.pools': ['numvols', 'name'] },
    }))

    const { visibleColumns, toggleableColumns } = usePersistedTableColumns('storages.pools', COLUMNS, {
      essential: ['name', 'actions'],
    })

    expect(visibleColumns.value.map(c => c.name)).toEqual(['name', 'pooltype', 'actions'])
    expect(toggleableColumns.value.find(c => c.name === 'numvols').visible).toBe(false)
  })

  it('toggles a column and persists the change', async () => {
    const { visibleColumns, toggleColumn } = usePersistedTableColumns('storages.pools', COLUMNS, {
      essential: ['name', 'actions'],
    })

    toggleColumn('numvols')
    await nextTick()

    expect(visibleColumns.value.map(c => c.name)).toEqual(['name', 'pooltype', 'actions'])
    expect(JSON.parse(localStorage.getItem(LS_KEY))).toEqual(
      expect.objectContaining({
        tableHiddenColumns: { 'storages.pools': ['numvols'] },
      })
    )

    toggleColumn('numvols')
    await nextTick()

    expect(visibleColumns.value.map(c => c.name)).toEqual(['name', 'pooltype', 'numvols', 'actions'])
  })

  it('accepts a reactive column list and drops stale hidden-column names', async () => {
    const columns = ref(COLUMNS)
    const { toggleColumn, toggleableColumns } = usePersistedTableColumns('storages.pools', columns, {
      essential: ['name', 'actions'],
    })

    toggleColumn('numvols')
    await nextTick()

    columns.value = COLUMNS.filter(c => c.name !== 'numvols')
    await nextTick()

    expect(toggleableColumns.value.map(c => c.name)).toEqual(['pooltype'])
    expect(JSON.parse(localStorage.getItem(LS_KEY)).tableHiddenColumns).toEqual({})
  })
})
