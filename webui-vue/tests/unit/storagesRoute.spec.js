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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import { describe, expect, it } from 'vitest'
import {
  AUTOCHANGER_DIRECTOR_QUERY_KEY,
  AUTOCHANGER_STORAGE_QUERY_KEY,
  buildAutochangerLocation,
  buildAutochangerOriginQuery,
  buildPoolsTabQuery,
  normalisePoolsTab,
  resolveAutochangerSelectionQuery,
  resolveAutochangerSelection,
  resolveStoragesScopeDirector,
  withStoragesScopeDirectorQuery,
} from '../../src/utils/storagesRoute.js'

describe('storages route helpers', () => {
  it('keeps unrelated query fields while switching pools tabs', () => {
    expect(buildPoolsTabQuery({
      foo: 'bar',
      volPool: 'Full',
    }, 'volumes')).toEqual({
      foo: 'bar',
      tab: 'volumes',
      volPool: 'Full',
    })
  })

  it('drops the tab query for the default pools tab', () => {
    expect(buildPoolsTabQuery({
      tab: 'volumes',
      foo: 'bar',
    }, 'pools')).toEqual({
      foo: 'bar',
    })
  })

  it('normalises unknown pools tabs to pools', () => {
    expect(normalisePoolsTab('volumes')).toBe('volumes')
    expect(normalisePoolsTab('autochangers')).toBe('pools')
    expect(normalisePoolsTab(undefined)).toBe('pools')
  })

  it('adds and removes the storages scope director query parameter', () => {
    expect(withStoragesScopeDirectorQuery({ tab: 'volumes' }, 'prod-a')).toEqual({
      tab: 'volumes',
      scopeDirector: 'prod-a',
    })
    expect(withStoragesScopeDirectorQuery({ tab: 'volumes', scopeDirector: 'prod-a' }, '')).toEqual({
      tab: 'volumes',
    })
  })

  it('resolves an optional storages scope director from route state', () => {
    expect(resolveStoragesScopeDirector({ scopeDirector: 'prod-a' })).toBe('prod-a')
    expect(resolveStoragesScopeDirector({ scopeDirector: 42 })).toBe('')
    expect(resolveStoragesScopeDirector({})).toBe('')
  })

  it('builds the autochanger route location of a storage', () => {
    expect(buildAutochangerLocation({
      name: 'TapeLibrary',
      director: 'prod-a',
    }, { scopeDirector: 'prod-a', director: 'old' })).toEqual({
      name: 'autochanger',
      params: { name: 'TapeLibrary' },
      query: { scopeDirector: 'prod-a', director: 'prod-a' },
    })

    expect(buildAutochangerLocation({ name: 'TapeLibrary' })).toEqual({
      name: 'autochanger',
      params: { name: 'TapeLibrary' },
      query: {},
    })
  })

  it('writes the autochanger origin into the query', () => {
    expect(buildAutochangerOriginQuery({
      foo: 'bar',
      [AUTOCHANGER_DIRECTOR_QUERY_KEY]: 'stale',
    }, {
      name: 'TapeLibrary',
      director: 'prod-a',
    })).toEqual({
      foo: 'bar',
      [AUTOCHANGER_STORAGE_QUERY_KEY]: 'TapeLibrary',
      [AUTOCHANGER_DIRECTOR_QUERY_KEY]: 'prod-a',
    })
  })

  it('resolves autochanger selection from route query state', () => {
    expect(resolveAutochangerSelectionQuery({
      [AUTOCHANGER_STORAGE_QUERY_KEY]: 'TapeLibrary',
      [AUTOCHANGER_DIRECTOR_QUERY_KEY]: 'prod-a',
    })).toEqual({
      name: 'TapeLibrary',
      director: 'prod-a',
    })

    expect(resolveAutochangerSelectionQuery({
      [AUTOCHANGER_DIRECTOR_QUERY_KEY]: 'prod-a',
    })).toBeNull()
  })

  it('resolves an exact autochanger match by director and name', () => {
    expect(resolveAutochangerSelection([
      { name: 'TapeLibrary', director: 'prod-a', scopeKey: 'prod-a:TapeLibrary' },
      { name: 'TapeLibrary', director: 'prod-b', scopeKey: 'prod-b:TapeLibrary' },
    ], {
      storageName: 'TapeLibrary',
      directorName: 'prod-b',
      activeDirectors: ['prod-a', 'prod-b'],
    })).toEqual({
      name: 'TapeLibrary',
      director: 'prod-b',
      scopeKey: 'prod-b:TapeLibrary',
    })
  })

  it('falls back to the single active director when no explicit director is given', () => {
    expect(resolveAutochangerSelection([
      { name: 'TapeLibrary', director: 'prod-a', scopeKey: 'prod-a:TapeLibrary' },
      { name: 'TapeLibrary', director: 'prod-b', scopeKey: 'prod-b:TapeLibrary' },
    ], {
      storageName: 'TapeLibrary',
      activeDirectors: ['prod-a'],
    })).toEqual({
      name: 'TapeLibrary',
      director: 'prod-a',
      scopeKey: 'prod-a:TapeLibrary',
    })
  })

  it('honors an explicit storages scope director for autochanger selection', () => {
    expect(resolveAutochangerSelection([
      { name: 'TapeLibrary', director: 'prod-a', scopeKey: 'prod-a:TapeLibrary' },
      { name: 'TapeLibrary', director: 'prod-b', scopeKey: 'prod-b:TapeLibrary' },
    ], {
      storageName: 'TapeLibrary',
      directorName: 'prod-b',
      scopeDirector: 'prod-a',
      activeDirectors: ['prod-a', 'prod-b'],
    })).toBeNull()

    expect(resolveAutochangerSelection([
      { name: 'TapeLibrary', director: 'prod-a', scopeKey: 'prod-a:TapeLibrary' },
      { name: 'TapeLibrary', director: 'prod-b', scopeKey: 'prod-b:TapeLibrary' },
    ], {
      storageName: 'TapeLibrary',
      scopeDirector: 'prod-a',
      activeDirectors: ['prod-a', 'prod-b'],
    })).toEqual({
      name: 'TapeLibrary',
      director: 'prod-a',
      scopeKey: 'prod-a:TapeLibrary',
    })
  })
})
