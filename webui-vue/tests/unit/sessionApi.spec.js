/*
   BAREOS - Backup Archiving REcovery Open Sourced

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

import { afterEach, describe, expect, it, vi } from 'vitest'
import { loginDirectorProxySession } from '../../src/utils/sessionApi.js'

afterEach(() => vi.unstubAllGlobals())

describe('session API failures', () => {
  it('classifies a non-JSON upstream failure as a proxy connection error', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(
      new Response('<html>Service unavailable</html>', { status: 503 }),
    ))
    await expect(loginDirectorProxySession({
      username: 'test', password: 'test', director: 'bareos-dir',
    })).rejects.toThrow('Cannot connect to proxy')
  })

  it('preserves ordinary authentication failures', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue(
      new Response(JSON.stringify({ message: 'Authentication failed' }), { status: 401 }),
    ))
    await expect(loginDirectorProxySession({
      username: 'test', password: 'test', director: 'bareos-dir',
    })).rejects.toThrow('Authentication failed')
  })
})
