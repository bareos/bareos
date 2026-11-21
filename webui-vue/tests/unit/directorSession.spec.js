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
    auth.loginDirector('ops', 'bareos-dir-b', 'secret', { setCurrent: false })
    settings.directorName = 'bareos-dir-a'
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn().mockResolvedValue(true)

    await expect(switchActiveDirector('bareos-dir-b')).resolves.toBe(true)

    expect(director.disconnect).toHaveBeenCalledOnce()
    expect(director.connectAndWait).toHaveBeenCalledWith({
      username: 'ops',
      password: 'secret',
      director: 'bareos-dir-b',
    })
    expect(auth.user?.director).toBe('bareos-dir-b')
    expect(auth.user?.username).toBe('ops')
    expect(settings.directorName).toBe('bareos-dir-b')
  })

  it('rejects switching to a director without stored credentials', async () => {
    const auth = useAuthStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir-a', 'secret')
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn()

    await expect(switchActiveDirector('bareos-dir-b')).rejects.toThrow(
      'Please log in to director "bareos-dir-b" first.'
    )

    expect(director.disconnect).not.toHaveBeenCalled()
  })

  it('restores the previous active director when reconnecting fails', async () => {
    const auth = useAuthStore()
    const settings = useSettingsStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir-a', 'secret')
    auth.loginDirector('ops', 'bareos-dir-b', 'secret', { setCurrent: false })
    settings.directorName = 'bareos-dir-a'
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn()
      .mockRejectedValueOnce(new Error('boom'))
      .mockResolvedValueOnce(true)

    await expect(switchActiveDirector('bareos-dir-b')).rejects.toThrow('boom')

    expect(auth.user?.director).toBe('bareos-dir-a')
    expect(auth.user?.username).toBe('admin')
    expect(settings.directorName).toBe('bareos-dir-a')
    expect(director.connectAndWait).toHaveBeenNthCalledWith(1, {
      username: 'ops',
      password: 'secret',
      director: 'bareos-dir-b',
    })
    expect(director.connectAndWait).toHaveBeenNthCalledWith(2, {
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-a',
    })
  })

  it('skips a queued switch when its caller is no longer current', async () => {
    const auth = useAuthStore()
    const settings = useSettingsStore()
    const director = useDirectorStore()
    let finishConnecting
    const connecting = new Promise(resolve => { finishConnecting = resolve })

    auth.login('admin', 'bareos-dir-a', 'secret')
    auth.loginDirector('ops', 'bareos-dir-b', 'secret', { setCurrent: false })
    auth.loginDirector('backup', 'bareos-dir-c', 'secret', { setCurrent: false })
    settings.directorName = 'bareos-dir-a'
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn()
      .mockImplementationOnce(() => connecting)
      .mockResolvedValue(true)

    const firstSwitch = switchActiveDirector('bareos-dir-b')
    await vi.waitFor(() => expect(director.connectAndWait).toHaveBeenCalledOnce())
    let current = true
    const queuedSwitch = switchActiveDirector('bareos-dir-c', () => current)
    current = false
    finishConnecting(true)
    await firstSwitch

    await expect(queuedSwitch).resolves.toBe(false)
    expect(director.disconnect).toHaveBeenCalledOnce()
    expect(director.connectAndWait).toHaveBeenCalledOnce()
    expect(auth.user?.director).toBe('bareos-dir-b')
    expect(settings.directorName).toBe('bareos-dir-b')
  })
})
