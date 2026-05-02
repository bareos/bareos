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
import { switchActiveDirector } from '../../src/composables/useDirectorSession.js'
import { useAuthStore } from '../../src/stores/auth.js'
import { useDirectorStore } from '../../src/stores/director.js'
import { useSettingsStore } from '../../src/stores/settings.js'

describe('director session switching', () => {
  beforeEach(() => {
    sessionStorage.clear()
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('switches the active director and reconnects the singleton session', async () => {
    const auth = useAuthStore()
    const settings = useSettingsStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir-a', 'secret')
    settings.directorName = 'bareos-dir-a'
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn().mockResolvedValue(true)

    await expect(switchActiveDirector('bareos-dir-b')).resolves.toBe(true)

    expect(director.disconnect).toHaveBeenCalledOnce()
    expect(director.connectAndWait).toHaveBeenCalledWith({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-b',
    })
    expect(auth.user?.director).toBe('bareos-dir-b')
    expect(settings.directorName).toBe('bareos-dir-b')
  })

  it('restores the previous active director when reconnecting fails', async () => {
    const auth = useAuthStore()
    const settings = useSettingsStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir-a', 'secret')
    settings.directorName = 'bareos-dir-a'
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn()
      .mockRejectedValueOnce(new Error('boom'))
      .mockResolvedValueOnce(true)

    await expect(switchActiveDirector('bareos-dir-b')).rejects.toThrow('boom')

    expect(auth.user?.director).toBe('bareos-dir-a')
    expect(settings.directorName).toBe('bareos-dir-a')
    expect(director.connectAndWait).toHaveBeenNthCalledWith(1, {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-b',
    })
    expect(director.connectAndWait).toHaveBeenNthCalledWith(2, {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
  })
})
