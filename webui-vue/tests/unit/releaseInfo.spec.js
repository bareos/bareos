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

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import {
  compareBareosVersions,
  getNearestVersionInfo,
  parseReleaseInfoPayload,
  useReleaseInfoStore,
} from '../../src/stores/releaseInfo.js'

describe('release info store', () => {
  beforeEach(() => {
    setActivePinia(createPinia())
    vi.unstubAllGlobals()
  })

  it('parses the JSONP payload returned by download.bareos.com', () => {
    expect(parseReleaseInfoPayload(`
      jsonCallback([
        {"version":"26.0.0","status":"uptodate","package_update_info":"Supported"},
        {"version":"25.0.0","status":"update_required","package_update_info":"Upgrade available"}
      ]);
    `)).toEqual([
      { version: '26.0.0', status: 'uptodate', package_update_info: 'Supported' },
      { version: '25.0.0', status: 'update_required', package_update_info: 'Upgrade available' },
    ])
  })

  it('matches a runtime version to the nearest supported release entry', () => {
    expect(getNearestVersionInfo([
      { version: '26.0.0', status: 'uptodate', package_update_info: 'Supported' },
      { version: '25.0.0', status: 'update_required', package_update_info: 'Upgrade available' },
    ], '25.0.3')).toEqual({
      version: '25.0.0',
      status: 'update_required',
      package_update_info: 'Upgrade available',
      requested_version: '25.0.3',
    })
  })

  it('compares numeric Bareos versions in release order', () => {
    expect(compareBareosVersions('26.0.1', '26.0.0')).toBeGreaterThan(0)
    expect(compareBareosVersions('25.0.0', '26.0.0')).toBeLessThan(0)
    expect(compareBareosVersions('26.0.0', '26.0.0')).toBe(0)
  })

  it('downloads and exposes release metadata through the store', async () => {
    vi.stubGlobal('fetch', vi.fn().mockResolvedValue({
      ok: true,
      text: async () => 'jsonCallback([{"version":"26.0.0","status":"uptodate","package_update_info":"Supported"}]);',
    }))

    const releaseInfo = useReleaseInfoStore()
    await releaseInfo.refresh()

    expect(fetch).toHaveBeenCalledTimes(1)
    expect(releaseInfo.getVersionInfo('26.0.0')).toEqual({
      version: '26.0.0',
      requested_version: '26.0.0',
      status: 'uptodate',
      package_update_info: 'Supported',
    })
  })
})
