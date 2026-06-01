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
  buildCancelJobCommand,
  buildJobDefaultsCommand,
  buildJobDetailsQuery,
  buildJobsFilterClause,
  buildListJobCommand,
  buildListJobsCommand,
  buildListJobsCountCommand,
  buildRerunJobCommand,
  buildRunJobCommand,
  buildSetJobEnabledCommand,
  encodeJobsLevelFilters,
  encodeJobsStatusFilters,
  encodeJobsTypeFilters,
  filterJobsBySearch,
  filterJobsTextOptions,
  normaliseJobId,
  normaliseJobLevelFilter,
  normaliseJobLevelFilters,
  normaliseJobsSearchTerm,
  normaliseJobsTextFilter,
  normaliseJobsTextOptions,
  normaliseJobStatusFilter,
  normaliseJobStatusFilters,
  normaliseJobTypeFilter,
  normaliseJobTypeFilters,
  paginateJobs,
  resolveJobDetailsClientOrigin,
  resolveJobDetailsQuery,
  resolveJobDetailsDashboardOrigin,
  resolveJobDetailsDirectorOrigin,
  resolveJobDetailsRestoreOrigin,
  resolveJobDetailsVolumeOrigin,
  resolveJobsClientQuery,
  resolveJobsJobQuery,
  resolveJobsLevelFilters,
  resolveJobsSearchQuery,
  resolveJobsStatusFilters,
  resolveJobsTypeFilters,
  resolveJobsListQuery,
  withJobsClientQuery,
  withJobsJobQuery,
  withJobsLevelFilterQuery,
  withJobsSearchQuery,
  withJobsStatusFilterQuery,
  withJobsTypeFilterQuery,
} from '../../src/utils/jobs.js'

describe('jobs filter helpers', () => {
  it('accepts supported job status filters only', () => {
    expect(normaliseJobStatusFilter('T')).toBe('T')
    expect(normaliseJobStatusFilter('R')).toBe('R')
    expect(normaliseJobStatusFilter('successful')).toBe('')
    expect(normaliseJobStatusFilter(undefined)).toBe('')
  })

  it('accepts supported job level filters only', () => {
    expect(normaliseJobLevelFilter('F')).toBe('F')
    expect(normaliseJobLevelFilter('V')).toBe('V')
    expect(normaliseJobLevelFilter('Full')).toBe('')
    expect(normaliseJobLevelFilter(undefined)).toBe('')
  })

  it('accepts supported job type filters only', () => {
    expect(normaliseJobTypeFilter('B')).toBe('B')
    expect(normaliseJobTypeFilter('Restore')).toBe('R')
    expect(normaliseJobTypeFilter('NotAType')).toBe('')
    expect(normaliseJobTypeFilter(undefined)).toBe('')
  })

  it('accepts positive integer job ids only', () => {
    expect(normaliseJobId(42)).toBe(42)
    expect(normaliseJobId('42')).toBe(42)
    expect(normaliseJobId(' 42 ')).toBe(42)
    expect(normaliseJobId('0')).toBeNull()
    expect(normaliseJobId('-1')).toBeNull()
    expect(normaliseJobId('1.5')).toBeNull()
    expect(normaliseJobId('abc')).toBeNull()
    expect(normaliseJobId(undefined)).toBeNull()
  })

  it('normalizes and applies search terms to jobs', () => {
    expect(normaliseJobsSearchTerm('  Backup  ')).toBe('Backup')
    expect(normaliseJobsTextFilter('  bareos-fd  ')).toBe('bareos-fd')
    expect(normaliseJobsSearchTerm(undefined)).toBe('')

    expect(filterJobsBySearch([
      { id: 42, name: 'DailyBackup', client: 'bareos-fd' },
      { id: 7, name: 'Archive', client: 'db-fd' },
    ], ' bareos ')).toEqual([
      { id: 42, name: 'DailyBackup', client: 'bareos-fd' },
    ])
    expect(filterJobsBySearch([
      { id: 42, name: 'DailyBackup', client: 'bareos-fd' },
      { id: 7, name: 'Archive', client: 'db-fd' },
    ], '42')).toEqual([
      { id: 42, name: 'DailyBackup', client: 'bareos-fd' },
    ])
  })

  it('normalizes and filters text options for autocomplete', () => {
    expect(normaliseJobsTextOptions([
      ' Nightly ',
      'nightly',
      '',
      'bareos-fd',
      ' Bareos-FD ',
      null,
    ])).toEqual(['bareos-fd', 'Nightly'])

    expect(filterJobsTextOptions([
      ' Nightly ',
      'Weekly',
      'bareos-fd',
      'db-fd',
    ], 'fd')).toEqual(['bareos-fd', 'db-fd'])

    expect(filterJobsTextOptions([
      'Nightly',
      'Weekly',
      'Monthly',
    ], '', 2)).toEqual(['Monthly', 'Nightly'])
  })

  it('paginates jobs and keeps all rows for the All option', () => {
    const jobs = [{ id: 4 }, { id: 3 }, { id: 2 }, { id: 1 }]

    expect(paginateJobs(jobs, { page: 2, rowsPerPage: 2 })).toEqual([
      { id: 2 },
      { id: 1 },
    ])
    expect(paginateJobs(jobs, { page: 1, rowsPerPage: 0 })).toEqual(jobs)
  })

  it('normalizes and encodes multi-status filters', () => {
    expect(normaliseJobStatusFilters('f,E,f,invalid')).toEqual(['f', 'E'])
    expect(normaliseJobStatusFilters(['R', 'A', 'R', 'invalid'])).toEqual(['R', 'A'])
    expect(encodeJobsStatusFilters(['f', 'E', 'f'])).toBe('f,E')
  })

  it('normalizes and encodes multi-level filters', () => {
    expect(normaliseJobLevelFilters('F,I,F,invalid')).toEqual(['F', 'I'])
    expect(normaliseJobLevelFilters(['D', 'V', 'D', 'invalid'])).toEqual(['D', 'V'])
    expect(encodeJobsLevelFilters(['F', 'I', 'F'])).toBe('F,I')
  })

  it('normalizes and encodes multi-type filters', () => {
    expect(normaliseJobTypeFilters('B,Restore,B,invalid')).toEqual(['B', 'R'])
    expect(normaliseJobTypeFilters(['Copy', 'g', 'Copy', 'invalid'])).toEqual(['c', 'g'])
    expect(encodeJobsTypeFilters(['B', 'Restore', 'B'])).toBe('B,R')
  })

  it('builds list and action commands for jobs', () => {
    expect(buildJobsFilterClause({
      statusFilter: ['f', 'E'],
      levelFilter: ['F', 'I'],
      typeFilter: ['B', 'Restore'],
      jobFilter: ' Daily "Backup" ',
      clientFilter: ' fd one ',
    })).toBe(' jobstatus=f,E joblevel=F,I jobtype=B,R job="Daily \\"Backup\\"" client="fd one"')
    expect(buildListJobsCountCommand({
      statusFilter: 'T',
      levelFilter: 'F',
      typeFilter: 'B',
      jobFilter: 'Daily "Backup"',
      clientFilter: 'fd one',
    })).toBe('list jobs count jobstatus=T joblevel=F jobtype=B job="Daily \\"Backup\\"" client="fd one"')
    expect(buildListJobsCommand({
      limit: 25,
      offset: 50,
      statusFilter: 'T',
      levelFilter: 'F',
      typeFilter: 'B',
      jobFilter: 'Daily "Backup"',
      clientFilter: 'fd one',
    })).toBe('llist jobs reverse limit=25 offset=50 jobstatus=T joblevel=F jobtype=B job="Daily \\"Backup\\"" client="fd one"')
    expect(buildListJobCommand(42)).toBe('llist jobid=42')
    expect(buildCancelJobCommand(42)).toBe('cancel jobid=42 yes')
    expect(buildRerunJobCommand(42)).toBe('rerun jobid=42 yes')
  })

  it('quotes job command arguments consistently', () => {
    expect(buildSetJobEnabledCommand('Daily "Backup"', true))
      .toBe('enable job="Daily \\"Backup\\"" yes')
    expect(buildJobDefaultsCommand('Daily "Backup"'))
      .toBe('.defaults job="Daily \\"Backup\\""')
    expect(buildRunJobCommand({
      job: 'Daily "Backup"',
      client: 'fd one',
      fileset: 'Linux',
      pool: 'Full',
      storage: 'File',
      level: 'Incremental',
      when: '2026-05-28 12:00:00',
      priority: 10,
    })).toBe(
      'run job="Daily \\"Backup\\"" client="fd one" fileset="Linux" ' +
      'pool="Full" storage="File" level="Incremental" ' +
      'when="2026-05-28 12:00:00" priority=10 yes'
    )
  })

  it('adds and removes the status query parameter', () => {
    expect(withJobsStatusFilterQuery({ search: 'Backup' }, 'T')).toEqual({
      search: 'Backup',
      status: 'T',
    })
    expect(withJobsStatusFilterQuery({ search: 'Backup' }, ['f', 'E'])).toEqual({
      search: 'Backup',
      status: 'f,E',
    })
    expect(withJobsStatusFilterQuery({ search: 'Backup', status: 'T' }, '')).toEqual({
      search: 'Backup',
    })
  })

  it('adds and removes the level query parameter', () => {
    expect(withJobsLevelFilterQuery({ search: 'Backup' }, 'F')).toEqual({
      search: 'Backup',
      level: 'F',
    })
    expect(withJobsLevelFilterQuery({ search: 'Backup' }, ['F', 'I'])).toEqual({
      search: 'Backup',
      level: 'F,I',
    })
    expect(withJobsLevelFilterQuery({ search: 'Backup', level: 'F' }, '')).toEqual({
      search: 'Backup',
    })
  })

  it('adds and removes the type query parameter', () => {
    expect(withJobsTypeFilterQuery({ search: 'Backup' }, 'B')).toEqual({
      search: 'Backup',
      type: 'B',
    })
    expect(withJobsTypeFilterQuery({ search: 'Backup' }, ['B', 'Restore'])).toEqual({
      search: 'Backup',
      type: 'B,R',
    })
    expect(withJobsTypeFilterQuery({ search: 'Backup', type: 'B' }, '')).toEqual({
      search: 'Backup',
    })
  })

  it('adds and removes the job and client query parameters', () => {
    expect(withJobsJobQuery({ search: 'Backup' }, ' Daily ')).toEqual({
      search: 'Backup',
      job: 'Daily',
    })
    expect(withJobsJobQuery({ search: 'Backup', job: 'Daily' }, '')).toEqual({
      search: 'Backup',
    })
    expect(withJobsClientQuery({ search: 'Backup' }, ' bareos-fd ')).toEqual({
      search: 'Backup',
      client: 'bareos-fd',
    })
    expect(withJobsClientQuery({ search: 'Backup', client: 'bareos-fd' }, '')).toEqual({
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

  it('resolves optional multi-status route filters', () => {
    expect(resolveJobsStatusFilters({ status: 'f,E' })).toEqual(['f', 'E'])
    expect(resolveJobsStatusFilters({ status: ['R', 'A', 'invalid'] })).toEqual(['R', 'A'])
    expect(resolveJobsStatusFilters({})).toEqual([])
  })

  it('resolves optional multi-level route filters', () => {
    expect(resolveJobsLevelFilters({ level: 'F,I' })).toEqual(['F', 'I'])
    expect(resolveJobsLevelFilters({ level: ['D', 'V', 'invalid'] })).toEqual(['D', 'V'])
    expect(resolveJobsLevelFilters({})).toEqual([])
  })

  it('resolves optional multi-type route filters', () => {
    expect(resolveJobsTypeFilters({ type: 'B,R' })).toEqual(['B', 'R'])
    expect(resolveJobsTypeFilters({ type: ['Copy', 'g', 'invalid'] })).toEqual(['c', 'g'])
    expect(resolveJobsTypeFilters({})).toEqual([])
  })

  it('resolves optional job and client route filters', () => {
    expect(resolveJobsJobQuery({ job: ' Daily ' })).toBe('Daily')
    expect(resolveJobsClientQuery({ client: ' bareos-fd ' })).toBe('bareos-fd')
    expect(resolveJobsJobQuery({})).toBe('')
    expect(resolveJobsClientQuery({})).toBe('')
  })

  it('builds job details query with return-state fields', () => {
    expect(buildJobDetailsQuery({
      director: 'prod-a',
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsLevel: 'F,I',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientDashboardOrigin: true,
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsLevel: 'D',
      clientJobsType: 'c',
      clientJobsJob: 'Nightly',
      clientJobsClient: 'bareos-fd',
      clientJobsSearch: 'backup',
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
      jobsLevel: 'F,I',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientDashboardOrigin: '1',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsLevel: 'D',
      clientJobsType: 'c',
      clientJobsJob: 'Nightly',
      clientJobsClient: 'bareos-fd',
      clientJobsSearch: 'backup',
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
      jobsStatus: ['f', 'E'],
      jobsLevel: 'F',
      jobsType: ['B', 'Restore'],
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: '',
    })).toEqual({
      director: 'prod-a',
      jobsStatus: 'f,E',
      jobsLevel: 'F',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
    })
  })

  it('restores jobs list query from detail return-state fields', () => {
    expect(resolveJobsListQuery({
      jobsAction: 'timeline',
      jobsStatus: 'f,E',
      jobsLevel: 'F,I',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
    })).toEqual({
      action: 'timeline',
      status: 'f,E',
      level: 'F,I',
      type: 'B,R',
      job: 'Nightly',
      client: 'bareos-fd',
      search: 'backup',
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
      jobsLevel: 'F,I',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientDashboardOrigin: '1',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsLevel: 'D',
      clientJobsType: 'c',
      clientJobsJob: 'Nightly',
      clientJobsClient: 'bareos-fd',
      clientJobsSearch: 'backup',
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
      jobsLevel: 'F,I',
      jobsType: 'B,R',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
      clientName: 'bareos-fd',
      clientDirector: 'prod-a',
      clientsTab: 'timeline',
      clientsScopeDirector: 'prod-a',
      clientDashboardOrigin: '1',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsLevel: 'D',
      clientJobsType: 'c',
      clientJobsJob: 'Nightly',
      clientJobsClient: 'bareos-fd',
      clientJobsSearch: 'backup',
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
      clientDashboardOrigin: '1',
      clientJobsAction: 'timeline',
      clientJobsStatus: 'T',
      clientJobsLevel: 'F',
      clientJobsType: 'B',
      clientJobsJob: 'Nightly',
      clientJobsClient: 'bareos-fd',
      clientJobsSearch: 'backup',
    })).toEqual({
      name: 'bareos-fd',
      director: 'prod-a',
      clientsTab: 'timeline',
      scopeDirector: 'prod-a',
      dashboardOrigin: true,
      jobsAction: 'timeline',
      jobsStatus: 'T',
      jobsLevel: 'F',
      jobsType: 'B',
      jobsJob: 'Nightly',
      jobsClient: 'bareos-fd',
      jobsSearch: 'backup',
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
