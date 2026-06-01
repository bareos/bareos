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

  it('keeps login credentials in memory only', () => {
    const auth = useAuthStore()

    auth.login('admin', 'bareos-dir', 'secret')

    expect(auth.isLoggedIn).toBe(true)
    expect(sessionStorage.getItem('bareos_user')).toBeNull()
    expect(sessionStorage.getItem('bareos_pass')).toBeNull()
    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })
  })

  it('clears legacy stored credentials on startup', () => {
    sessionStorage.setItem('bareos_user', JSON.stringify({
      username: 'admin',
      director: 'bareos-dir',
    }))
    sessionStorage.setItem('bareos_pass', 'secret')

    const auth = useAuthStore()

    expect(auth.isLoggedIn).toBe(false)
    expect(sessionStorage.getItem('bareos_user')).toBeNull()
    expect(sessionStorage.getItem('bareos_pass')).toBeNull()
    expect(auth.getCredentials()).toBeNull()
  })

  it('falls back to the default director connection when omitted', () => {
    const auth = useAuthStore()

    auth.login('admin', undefined, 'secret')

    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: 'secret',
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

    auth.login('admin', 'bareos-dir', 'secret')
    auth.setDirector('bareos-dir-2')

    expect(auth.getCredentials()).toEqual({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir-2',
    })
  })
})
