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
import { useAuthStore } from '../../src/stores/auth.js'

describe('auth store', () => {
  beforeEach(() => {
    sessionStorage.clear()
    setActivePinia(createPinia())
  })

  it('persists proxy session tokens for reconnects', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', { sessionToken: 'proxy-session' })

    expect(auth.isLoggedIn).toBe(true)
    expect(JSON.parse(sessionStorage.getItem('bareos_user'))).toEqual({
      username: 'admin',
      director: 'bareos-dir',
    })
    expect(sessionStorage.getItem('bareos_session_token')).toBe('proxy-session')
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      director: 'bareos-dir',
      sessionToken: 'proxy-session',
    })
  })

  it('falls back to the default director connection when omitted', () => {
    const auth = useAuthStore()

    auth.login('admin', undefined, { sessionToken: 'proxy-session' })

    expect(JSON.parse(sessionStorage.getItem('bareos_user'))).toEqual({
      username: 'admin',
      director: 'bareos-dir',
    })
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      director: 'bareos-dir',
      sessionToken: 'proxy-session',
    })
  })

  it('clears the stored session on logout', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', { sessionToken: 'proxy-session' })
    auth.logout()

    expect(auth.isLoggedIn).toBe(false)
    expect(sessionStorage.getItem('bareos_user')).toBeNull()
    expect(sessionStorage.getItem('bareos_session_token')).toBeNull()
    expect(auth.getCredentials()).toBeNull()
  })

  it('updates the active director without losing the stored session token', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', { sessionToken: 'proxy-session' })
    auth.setDirector('bareos-dir-2')

    expect(JSON.parse(sessionStorage.getItem('bareos_user'))).toEqual({
      username: 'admin',
      director: 'bareos-dir-2',
    })
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      director: 'bareos-dir-2',
      sessionToken: 'proxy-session',
    })
  })

  it('keeps plaintext passwords in memory only', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', 'secret')

    expect(sessionStorage.getItem('bareos_session_token')).toBeNull()
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })
  })
})
