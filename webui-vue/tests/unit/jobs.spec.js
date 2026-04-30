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
  normaliseJobStatusFilter,
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
})
