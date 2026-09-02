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
import { usePersistedTablePagination } from '../../src/composables/usePersistedTablePagination.js'

const LS_KEY = 'bareos_settings'

describe('usePersistedTablePagination', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('keeps a previously persisted value that is still allowed', () => {
    localStorage.setItem(LS_KEY, JSON.stringify({
      tableRowsPerPage: { 'jobs.list': 50 },
    }))

    const pagination = usePersistedTablePagination('jobs.list', {
      rowsPerPage: 25,
    }, { allowedRowsPerPage: [10, 25, 50] })

    expect(pagination.value.rowsPerPage).toBe(50)
  })

  it('falls back to the default when the persisted "All" (0) value is no longer allowed', () => {
    // Simulates a user who saved "All" before it was removed from the page
    // size options: without this guard the page would freeze again on
    // every load, before the user could reach the selector to change it.
    localStorage.setItem(LS_KEY, JSON.stringify({
      tableRowsPerPage: { 'jobs.list': 0 },
    }))

    const pagination = usePersistedTablePagination('jobs.list', {
      rowsPerPage: 25,
    }, { allowedRowsPerPage: [10, 25, 50] })

    expect(pagination.value.rowsPerPage).toBe(25)
  })

  it('does not restrict values when allowedRowsPerPage is not given', () => {
    localStorage.setItem(LS_KEY, JSON.stringify({
      tableRowsPerPage: { 'other.list': 0 },
    }))

    const pagination = usePersistedTablePagination('other.list', {
      rowsPerPage: 15,
    })

    expect(pagination.value.rowsPerPage).toBe(0)
  })
})
