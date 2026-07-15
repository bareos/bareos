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
  buildDirectorPageQuery,
  isDirectorSingletonTab,
  resolveDirectorTargetQuery,
} from '../../src/utils/director.js'

describe('director route helpers', () => {
  it('identifies singleton director tabs', () => {
    expect(isDirectorSingletonTab('catalog')).toBe(true)
    expect(isDirectorSingletonTab('subscription')).toBe(true)
    expect(isDirectorSingletonTab('status')).toBe(false)
  })

  it('adds and removes director page query state', () => {
    expect(buildDirectorPageQuery({ foo: 'bar' }, {
      tab: 'catalog',
      targetDirector: 'prod-a',
    })).toEqual({
      foo: 'bar',
      tab: 'catalog',
      directorTarget: 'prod-a',
    })

    expect(buildDirectorPageQuery({
      foo: 'bar',
      tab: 'catalog',
      directorTarget: 'prod-a',
    }, {
      tab: 'status',
      targetDirector: 'prod-b',
    })).toEqual({
      foo: 'bar',
    })
  })

  it('reads the explicit director target from the route', () => {
    expect(resolveDirectorTargetQuery({ directorTarget: 'prod-a' })).toBe('prod-a')
    expect(resolveDirectorTargetQuery({ directorTarget: '' })).toBe('')
    expect(resolveDirectorTargetQuery({})).toBe('')
  })
})
