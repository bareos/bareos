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
import { getRestoreBrowserPlaceholder } from '../../src/utils/restore.js'

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
})
