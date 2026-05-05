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

import { describe, expect, it, vi } from 'vitest'
import { resolveSetupNavigation } from '../../src/router/guards.js'

describe('router setup guard', () => {
  it('drops stale login state when setup is required', () => {
    const auth = {
      isLoggedIn: true,
      logout: vi.fn(),
    }
    const setup = {
      needsSetup: true,
      isReady: false,
    }

    const redirect = resolveSetupNavigation(auth, setup, {
      name: 'dashboard',
      meta: { requiresAuth: true },
    })

    expect(auth.logout).toHaveBeenCalledOnce()
    expect(redirect).toEqual({ name: 'setup' })
  })
})
