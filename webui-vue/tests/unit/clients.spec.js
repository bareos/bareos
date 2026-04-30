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
  buildClientDetailsQuery,
  resolveClientDetailsDashboardOrigin,
  resolveClientDetailsJobsOrigin,
  resolveClientsListQuery,
  resolveClientsScopeDirector,
  withClientsScopeDirectorQuery,
} from '../../src/utils/clients.js'

describe('clients route helpers', () => {
  it('builds the client details query with return-state fields', () => {
    expect(buildClientDetailsQuery({
      director: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      dashboardOrigin: true,
    })).toEqual({
      director: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      dashboardOrigin: '1',
    })

    expect(buildClientDetailsQuery({
      director: 'prod-a',
      clientsTab: 'list',
    })).toEqual({
      director: 'prod-a',
    })
  })

  it('restores the clients list query from detail return-state fields', () => {
    expect(resolveClientsListQuery({
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
    })).toEqual({
      tab: 'timeline',
      scopeDirector: 'prod-a',
    })

    expect(resolveClientsListQuery({
      clientsTab: 'list',
    })).toEqual({})
  })

  it('resolves an optional dashboard origin for client details routes', () => {
    expect(resolveClientDetailsDashboardOrigin({ dashboardOrigin: '1' })).toBe(true)
    expect(resolveClientDetailsDashboardOrigin({ dashboardOrigin: '0' })).toBe(false)
    expect(resolveClientDetailsDashboardOrigin({})).toBe(false)
  })

  it('resolves an optional jobs origin for client details routes', () => {
    expect(resolveClientDetailsJobsOrigin({
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
    })).toEqual({
      action: 'timeline',
      status: 'T',
      search: 'backup',
      scopeDirector: 'prod-a',
    })

    expect(resolveClientDetailsJobsOrigin({ jobsAction: 'list' })).toEqual({})
  })

  it('adds and removes the scope director query parameter', () => {
    expect(withClientsScopeDirectorQuery({ tab: 'timeline' }, 'prod-a')).toEqual({
      tab: 'timeline',
      scopeDirector: 'prod-a',
    })
    expect(withClientsScopeDirectorQuery({ tab: 'timeline', scopeDirector: 'prod-a' }, '')).toEqual({
      tab: 'timeline',
    })
  })

  it('resolves an optional clients scope director query parameter', () => {
    expect(resolveClientsScopeDirector({ scopeDirector: 'prod-a' })).toBe('prod-a')
    expect(resolveClientsScopeDirector({ scopeDirector: 42 })).toBe('')
    expect(resolveClientsScopeDirector({})).toBe('')
  })
})
