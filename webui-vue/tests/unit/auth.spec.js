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
import { useAuthStore } from '../../src/stores/auth.js'
import { SESSION_AUTH_PASSWORD } from '../../src/utils/sessionApi.js'

describe('auth store', () => {
  beforeEach(() => {
    sessionStorage.clear()
    setActivePinia(createPinia())
    vi.stubGlobal('fetch', vi.fn(async () => ({
      status: 401,
      ok: false,
      text: async () => JSON.stringify({ message: 'No active session' }),
    })))
  })

  it('keeps login credentials in memory only', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')

    expect(auth.isLoggedIn).toBe(true)
    expect(sessionStorage.getItem('bareos_user')).toBeNull()
    expect(sessionStorage.getItem('bareos_pass')).toBeNull()
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: SESSION_AUTH_PASSWORD,
      director: 'bareos-dir',
    })
  })

  it('restores an active proxy session on startup', async () => {
    global.fetch.mockResolvedValueOnce({
      status: 200,
      ok: true,
      text: async () => JSON.stringify({
        authenticatedDirectors: [
          { director: 'bareos-dir', username: 'admin' },
          { director: 'site-b', username: 'ops' },
        ],
      }),
    })
    const auth = useAuthStore()
    await auth.restoreSession(false, 'site-b')

    expect(auth.isLoggedIn).toBe(true)
    expect(auth.getCredentials()).toEqual({
      username: 'ops',
      password: SESSION_AUTH_PASSWORD,
      director: 'site-b',
    })
    expect(auth.getCredentials('bareos-dir')).toEqual({
      username: 'admin',
      password: SESSION_AUTH_PASSWORD,
      director: 'bareos-dir',
    })
  })

  it('falls back to the default director connection when omitted', () => {
    const auth = useAuthStore()

    auth.login('admin')

    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: SESSION_AUTH_PASSWORD,
      director: 'bareos-dir',
    })
  })

  it('clears the stored session on logout', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', 'secret')
    auth.logout()

    expect(auth.isLoggedIn).toBe(false)
    expect(sessionStorage.getItem('bareos_user')).toBeNull()
    expect(sessionStorage.getItem('bareos_pass')).toBeNull()
    expect(auth.getCredentials()).toBeNull()
  })

  it('updates the active director without losing the in-memory password', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')
    auth.loginDirector('ops', 'bareos-dir-2', SESSION_AUTH_PASSWORD, {
      setCurrent: false,
    })
    auth.setDirector('bareos-dir-2')

    expect(auth.getCredentials()).toEqual({
      username: 'ops',
      password: SESSION_AUTH_PASSWORD,
      director: 'bareos-dir-2',
    })
  })

  it('tracks additional authenticated directors with individual usernames', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')
    auth.loginDirector('ops', 'site-b', SESSION_AUTH_PASSWORD, {
      setCurrent: false,
    })

    expect(auth.authenticatedDirectors).toEqual(['bareos-dir', 'site-b'])
    expect(auth.hasDirectorSession('site-b')).toBe(true)
    expect(auth.getDirectorUsername('site-b')).toBe('ops')
    expect(auth.getCredentials('site-b')).toEqual({
      username: 'ops',
      password: SESSION_AUTH_PASSWORD,
      director: 'site-b',
    })
    expect(auth.user?.director).toBe('bareos-dir')
  })

  it('lists authenticated director sessions with the current marker', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')
    auth.loginDirector('ops', 'site-b', SESSION_AUTH_PASSWORD, {
      setCurrent: false,
    })

    expect(auth.directorSessions).toEqual([
      { director: 'bareos-dir', username: 'admin', current: true },
      { director: 'site-b', username: 'ops', current: false },
    ])
  })

  it('reports selected directors without stored credentials', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')
    auth.loginDirector('ops', 'site-b', SESSION_AUTH_PASSWORD, {
      setCurrent: false,
    })

    expect(auth.missingDirectorSessions(['bareos-dir', 'site-b', 'site-c', 'site-c']))
      .toEqual(['site-c'])
  })

  it('falls back to another director when removing the current one', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir')
    auth.loginDirector('ops', 'site-b', SESSION_AUTH_PASSWORD, {
      setCurrent: false,
    })

    expect(auth.removeDirector('bareos-dir')).toBe(true)
    expect(auth.user).toEqual({
      username: 'ops',
      director: 'site-b',
    })
    expect(auth.authenticatedDirectors).toEqual(['site-b'])
  })

  it('merges sessions when applying multiple director sessions sequentially', () => {
    const auth = useAuthStore()

    auth.applySession({
      authenticatedDirectors: [{ director: 'bareos-dir', username: 'admin' }],
    }, SESSION_AUTH_PASSWORD, { merge: false })

    expect(auth.authenticatedDirectors).toEqual(['bareos-dir'])
    expect(auth.getDirectorUsername('bareos-dir')).toBe('admin')

    auth.applySession({
      authenticatedDirectors: [{ director: 'site-b', username: 'ops' }],
    }, SESSION_AUTH_PASSWORD, { merge: true })

    expect(auth.authenticatedDirectors).toContain('bareos-dir')
    expect(auth.authenticatedDirectors).toContain('site-b')
    expect(auth.getDirectorUsername('bareos-dir')).toBe('admin')
    expect(auth.getDirectorUsername('site-b')).toBe('ops')
  })

  it('replaces sessions when merge option is not set', () => {
    const auth = useAuthStore()

    auth.applySession({
      authenticatedDirectors: [{ director: 'bareos-dir', username: 'admin' }],
    }, SESSION_AUTH_PASSWORD)

    expect(auth.authenticatedDirectors).toEqual(['bareos-dir'])

    auth.applySession({
      authenticatedDirectors: [{ director: 'site-b', username: 'ops' }],
    }, SESSION_AUTH_PASSWORD)

    expect(auth.authenticatedDirectors).toEqual(['site-b'])
  })
})
