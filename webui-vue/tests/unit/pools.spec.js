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
  buildPoolDetailsQuery,
  resolvePoolDetailsVolumeQuery,
  resolvePoolDetailsVolumeOrigin,
} from '../../src/utils/pools.js'

describe('pool route helpers', () => {
  it('builds pool details query with optional volume origin', () => {
    expect(buildPoolDetailsQuery({
      director: 'prod-a',
      volumeName: 'Full-0001',
    })).toEqual({
      director: 'prod-a',
      volumeName: 'Full-0001',
    })

    expect(buildPoolDetailsQuery({
      director: 'prod-a',
    })).toEqual({
      director: 'prod-a',
    })

    expect(buildPoolDetailsQuery({
      director: 'prod-a',
      volumeName: 'Full-0001',
      volumeQuery: {
        director: 'prod-a',
        jobId: '42',
        poolName: 'Full',
      },
    })).toEqual({
      director: 'prod-a',
      jobId: '42',
      poolName: 'Full',
      volumeName: 'Full-0001',
    })
  })

  it('resolves an optional volume origin for pool details routes', () => {
    expect(resolvePoolDetailsVolumeOrigin({
      volumeName: 'Full-0001',
    })).toEqual({
      name: 'Full-0001',
    })

    expect(resolvePoolDetailsVolumeOrigin({})).toBeNull()
  })

  it('sanitizes volume details query state for pool routes', () => {
    expect(resolvePoolDetailsVolumeQuery({
      director: 'prod-a',
      directorTab: 'catalog',
      directorTarget: 'prod-b',
      jobId: '42',
      poolName: 'Full',
      volumeName: 'Full-0001',
      autochangerStorage: 'TapeLibrary',
      autochangerDirector: 'prod-a',
      ignored: 'value',
    })).toEqual({
      director: 'prod-a',
      directorTab: 'catalog',
      directorTarget: 'prod-b',
      jobId: '42',
      poolName: 'Full',
      volumeName: 'Full-0001',
      autochangerStorage: 'TapeLibrary',
      autochangerDirector: 'prod-a',
    })
  })
})
