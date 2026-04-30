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
  buildJobDetailsQuery,
  normaliseJobStatusFilter,
  resolveJobDetailsClientOrigin,
  resolveJobDetailsQuery,
  resolveJobDetailsDashboardOrigin,
  resolveJobDetailsDirectorOrigin,
  resolveJobDetailsRestoreOrigin,
  resolveJobDetailsVolumeOrigin,
  resolveJobsSearchQuery,
  resolveJobsScopeDirector,
  resolveJobsListQuery,
  withJobsSearchQuery,
  withJobsScopeDirectorQuery,
  withJobsStatusFilterQuery,
} from '../../src/utils/jobs.js'

describe('jobs filter helpers', () => {
  it('accepts supported job status filters only', () => {
    expect(normaliseJobStatusFilter('T')).toBe('T')
    expect(normaliseJobStatusFilter('R')).toBe('R')
    expect(normaliseJobStatusFilter('successful')).toBe('')
    expect(normaliseJobStatusFilter(undefined)).toBe('')
  })

  it('adds and removes the status query parameter', () => {
    expect(withJobsStatusFilterQuery({ search: 'Backup' }, 'T')).toEqual({
      search: 'Backup',
      status: 'T',
    })
    expect(withJobsStatusFilterQuery({ search: 'Backup', status: 'T' }, '')).toEqual({
      search: 'Backup',
    })
  })

  it('prefers the explicit search query and falls back to name', () => {
    expect(resolveJobsSearchQuery({ search: 'Explicit', name: 'Legacy' })).toBe('Explicit')
    expect(resolveJobsSearchQuery({ name: 'Legacy' })).toBe('Legacy')
    expect(resolveJobsSearchQuery({})).toBe('')
  })

  it('writes the search query and clears the legacy name fallback', () => {
    expect(withJobsSearchQuery({ name: 'Legacy', status: 'T' }, 'Explicit')).toEqual({
      search: 'Explicit',
      status: 'T',
    })
    expect(withJobsSearchQuery({ search: 'Explicit', status: 'T' }, '')).toEqual({
      status: 'T',
    })
  })

  it('adds and removes the scope director query parameter', () => {
    expect(withJobsScopeDirectorQuery({ search: 'Backup' }, 'prod-a')).toEqual({
      search: 'Backup',
      scopeDirector: 'prod-a',
    })
    expect(withJobsScopeDirectorQuery({ search: 'Backup', scopeDirector: 'prod-a' }, '')).toEqual({
      search: 'Backup',
    })
  })

  it('resolves an optional scope director query parameter', () => {
    expect(resolveJobsScopeDirector({ scopeDirector: 'prod-a' })).toBe('prod-a')
    expect(resolveJobsScopeDirector({ scopeDirector: 42 })).toBe('')
    expect(resolveJobsScopeDirector({})).toBe('')
  })

  it('builds job details query with return-state fields', () => {
    expect(buildJobDetailsQuery({
      director: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsSearch: 'backup',
      clientJobsScopeDirector: 'prod-a',
      volumeName: 'Full-0001',
      volumeDirector: 'prod-a',
      restoreClient: 'bareos-fd',
      restoreDirector: 'prod-a',
      restoreJobid: 42,
      directorTab: 'catalog',
      directorTarget: 'prod-a',
      dashboardOrigin: true,
    })).toEqual({
      director: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsSearch: 'backup',
      clientJobsScopeDirector: 'prod-a',
      volumeName: 'Full-0001',
      volumeDirector: 'prod-a',
      restoreClient: 'bareos-fd',
      restoreDirector: 'prod-a',
      restoreJobid: '42',
      directorTab: 'catalog',
      directorTarget: 'prod-a',
      dashboardOrigin: '1',
    })

    expect(buildJobDetailsQuery({
      director: 'prod-a',
      jobsAction: 'list',
      jobsStatus: '',
      jobsSearch: '',
    })).toEqual({
      director: 'prod-a',
    })
  })

  it('restores jobs list query from detail return-state fields', () => {
    expect(resolveJobsListQuery({
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

    expect(resolveJobsListQuery({
      jobsAction: 'list',
      jobsStatus: '',
      jobsSearch: '',
    })).toEqual({})
  })

  it('sanitizes job details query state from route query fields', () => {
    expect(resolveJobDetailsQuery({
      director: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsSearch: 'backup',
      clientJobsScopeDirector: 'prod-a',
      volumeName: 'Full-0001',
      volumeDirector: 'prod-a',
      restoreClient: 'bareos-fd',
      restoreDirector: 'prod-a',
      restoreJobid: '42',
      directorTab: 'catalog',
      directorTarget: 'prod-a',
      dashboardOrigin: '1',
      ignored: 'value',
    })).toEqual({
      director: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsSearch: 'backup',
      clientJobsScopeDirector: 'prod-a',
      volumeName: 'Full-0001',
      volumeDirector: 'prod-a',
      restoreClient: 'bareos-fd',
      restoreDirector: 'prod-a',
      restoreJobid: '42',
      directorTab: 'catalog',
      directorTarget: 'prod-a',
      dashboardOrigin: '1',
    })
  })

  it('resolves an optional client origin for job details routes', () => {
    expect(resolveJobDetailsClientOrigin({
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsSearch: 'backup',
      clientJobsScopeDirector: 'prod-a',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-a',
      clientsTab: 'timeline',
      scopeDirector: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsSearch: 'backup',
      jobsScopeDirector: 'prod-a',
    })

    expect(resolveJobDetailsClientOrigin({
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
    })).toBeNull()
  })

  it('resolves an optional volume origin for job details routes', () => {
    expect(resolveJobDetailsVolumeOrigin({
      volumeName: 'Full-0001',
      volumeDirector: 'prod-a',
    })).toEqual({
      name: 'Full-0001',
      director: 'prod-a',
    })

    expect(resolveJobDetailsVolumeOrigin({
      volumeDirector: 'prod-a',
    })).toBeNull()
  })

  it('resolves an optional restore origin for job details routes', () => {
    expect(resolveJobDetailsRestoreOrigin({
      restoreClient: 'bareos-fd',
      restoreDirector: 'prod-a',
      restoreJobid: '42',
    })).toEqual({
      client: 'bareos-fd',
      director: 'prod-a',
      jobid: '42',
    })

    expect(resolveJobDetailsRestoreOrigin({
      restoreDirector: 'prod-a',
      restoreJobid: '42',
    })).toBeNull()
  })

  it('resolves an optional director origin for job details routes', () => {
    expect(resolveJobDetailsDirectorOrigin({
      directorTab: 'catalog',
      directorTarget: 'prod-a',
    })).toEqual({
      tab: 'catalog',
      targetDirector: 'prod-a',
    })

    expect(resolveJobDetailsDirectorOrigin({
      directorTarget: 'prod-a',
    })).toBeNull()
  })

  it('resolves an optional dashboard origin for job details routes', () => {
    expect(resolveJobDetailsDashboardOrigin({ dashboardOrigin: '1' })).toBe(true)
    expect(resolveJobDetailsDashboardOrigin({ dashboardOrigin: '0' })).toBe(false)
    expect(resolveJobDetailsDashboardOrigin({})).toBe(false)
  })
})
