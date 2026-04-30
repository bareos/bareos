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

import { describe, expect, it } from 'vitest'
import {
  buildRestoreSourceQuery,
  filterRestoreSourceClients,
  getRestoreBrowserPlaceholder,
  resolveRestoreSourceClient,
} from '../../src/utils/restore.js'

describe('restore browser placeholder', () => {
  it('prefers the error state', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: 'Failed to load file tree',
      loadingBrowser: true,
      hasSelectedJob: true,
    })).toBe('error')
  })

  it('shows loading while a selected job is still fetching', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: '',
      loadingBrowser: true,
      hasSelectedJob: true,
    })).toBe('loading')
  })

  it('falls back to the initial empty prompt otherwise', () => {
    expect(getRestoreBrowserPlaceholder({
      browserError: '',
      loadingBrowser: false,
      hasSelectedJob: false,
    })).toBe('empty')
  })

  it('prefers an explicit director when resolving a queried source client', () => {
    expect(resolveRestoreSourceClient([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'bareos-fd', director: 'prod-b' },
    ], {
      clientName: 'bareos-fd',
      directorName: 'prod-b',
      currentDirector: 'prod-a',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-b',
    })
  })

  it('falls back to the current director when no explicit director is given', () => {
    expect(resolveRestoreSourceClient([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'bareos-fd', director: 'prod-b' },
    ], {
      clientName: 'bareos-fd',
      currentDirector: 'prod-a',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-a',
    })
  })

  it('filters source clients to the selected common-mode director', () => {
    expect(filterRestoreSourceClients([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'db-fd', director: 'prod-b' },
      { name: 'web-fd', director: 'prod-a' },
    ], 'prod-a')).toEqual([
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'web-fd', director: 'prod-a' },
    ])
  })

  it('keeps all source clients when no common-mode director is selected', () => {
    const clients = [
      { name: 'bareos-fd', director: 'prod-a' },
      { name: 'db-fd', director: 'prod-b' },
    ]
    expect(filterRestoreSourceClients(clients)).toEqual(clients)
  })

  it('writes the selected restore source back into the query', () => {
    expect(buildRestoreSourceQuery({
      foo: 'bar',
      jobid: 'old',
    }, {
      clientName: 'bareos-fd',
      directorName: 'prod-a',
      jobid: 42,
    })).toEqual({
      foo: 'bar',
      client: 'bareos-fd',
      director: 'prod-a',
      jobid: '42',
    })
  })

  it('clears restore source query fields when no source is selected', () => {
    expect(buildRestoreSourceQuery({
      foo: 'bar',
      client: 'bareos-fd',
      director: 'prod-a',
      jobid: '42',
    }, {})).toEqual({
      foo: 'bar',
    })
  })
})
