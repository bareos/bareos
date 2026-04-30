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

import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'
import { useSettingsStore } from '../../src/stores/settings.js'

describe('settings store', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('persists the last username and selected director in local storage', async () => {
    const settings = useSettingsStore()

    settings.loginUsername = 'alice'
    settings.directorName = 'prod-dir'

    await nextTick()

    expect(JSON.parse(localStorage.getItem('bareos_settings'))).toEqual(
      expect.objectContaining({
        loginUsername: 'alice',
        directorName: 'prod-dir',
      })
    )
  })

  it('restores the last username and selected director from local storage', () => {
    localStorage.setItem('bareos_settings', JSON.stringify({
      loginUsername: 'alice',
      directorName: 'prod-dir',
    }))

    setActivePinia(createPinia())
    const settings = useSettingsStore()

    expect(settings.loginUsername).toBe('alice')
    expect(settings.directorName).toBe('prod-dir')
  })
})
