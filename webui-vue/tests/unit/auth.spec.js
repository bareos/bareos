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

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { fetchAggregatedClients } from '../../src/composables/clientsAggregate.js'
import { fetchAggregatedFilesets } from '../../src/composables/filesetsAggregate.js'
import { fetchAggregatedSchedulesShow } from '../../src/composables/schedulesAggregate.js'
import { useAuthStore } from '../../src/stores/auth.js'
import { createDirectorCommandSession } from '../../src/utils/directorCommandSocket.js'
import { SESSION_AUTH_PASSWORD } from '../../src/utils/sessionApi.js'

vi.mock('../../src/utils/directorCommandSocket.js', async (importOriginal) => ({
  ...await importOriginal(),
  createDirectorCommandSession: vi.fn(),
}))

const session = {
  authenticatedDirectors: [{ director: 'bareos-dir', username: 'admin' }],
}

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

  afterEach(() => {
    vi.unstubAllGlobals()
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

  it('does not reapply a session restore that finished after logout', async () => {
    let respond
    global.fetch.mockReturnValueOnce(new Promise(resolve => { respond = resolve }))
    const auth = useAuthStore()
    const restoring = auth.restoreSession()
    auth.logout()
    respond({ status: 200, ok: true, text: async () => JSON.stringify(session) })
    await expect(restoring).resolves.toBe(false)
    expect(auth.getCredentials()).toBeNull()
  })
})

describe('Director aggregate cache lifecycle', () => {
  function load(auth, options) {
    const credentials = auth.getCredentials('bareos-dir')
    return Promise.all([
      fetchAggregatedClients(credentials, ['bareos-dir'], options),
      fetchAggregatedFilesets(credentials, ['bareos-dir'], options),
      fetchAggregatedSchedulesShow(credentials, ['bareos-dir'], options),
    ])
  }

  function serve(name, wait) {
    createDirectorCommandSession.mockImplementation(async () => ({
      call: async () => {
        await wait
        return {
          clients: [{ name }],
          filesets: [{ fileset: name }],
          schedules: [{ name }],
        }
      },
      close: vi.fn(),
    }))
  }

  beforeEach(() => {
    setActivePinia(createPinia())
    createDirectorCommandSession.mockReset()
    const auth = useAuthStore()
    auth.login('admin')
    auth.loginDirector('ops', 'site-b', SESSION_AUTH_PASSWORD, { setCurrent: false })
    serve('old')
  })

  function expectRows([clients, filesets, schedules], name) {
    expect(clients.clients).toMatchObject([{ name }])
    expect(filesets.filesets).toMatchObject([{ name }])
    expect(schedules.schedules).toMatchObject([{ name }])
  }

  it.each([
    ['same-user login', auth => auth.login('admin')],
    ['same-user session replacement', auth => auth.applySession({
      authenticatedDirectors: [
        ...session.authenticatedDirectors,
        { director: 'site-b', username: 'ops' },
      ],
    })],
    ['Director addition', auth => auth.loginDirector('ops', 'site-c', SESSION_AUTH_PASSWORD, { setCurrent: false })],
    ['Director removal', auth => auth.removeDirector('site-b')],
  ])('invalidates all three caches after %s', async (_label, transition) => {
    const auth = useAuthStore()
    expectRows(await load(auth), 'old')
    expect(createDirectorCommandSession).toHaveBeenCalledTimes(3)

    transition(auth)
    serve('new')
    expectRows(await load(auth), 'new')
    expect(createDirectorCommandSession).toHaveBeenCalledTimes(6)
  })

  it('invalidates all three caches when switching between authenticated Directors', async () => {
    const auth = useAuthStore()
    expectRows(await load(auth), 'old')

    expect(auth.setDirector('site-b')).toBe(true)
    expect(auth.user.director).toBe('site-b')
    serve('new')
    expectRows(await load(auth), 'new')
    expect(createDirectorCommandSession).toHaveBeenCalledTimes(6)
  })

  it('preserves caches when reselecting the current Director', async () => {
    const auth = useAuthStore()
    expectRows(await load(auth), 'old')

    expect(auth.setDirector(' bareos-dir ')).toBe(true)
    expect(auth.user.director).toBe('bareos-dir')
    serve('new')
    expectRows(await load(auth), 'old')
    expect(createDirectorCommandSession).toHaveBeenCalledTimes(3)
  })

  it('clears cached data at logout and blocks earlier requests from repopulating it', async () => {
    const auth = useAuthStore()
    await load(auth)
    let release
    serve('in-flight', new Promise(resolve => { release = resolve }))
    const inFlight = load(auth, { forceRefresh: true })
    auth.logout()
    release()
    await inFlight

    // Same cache key as before logout, without allowing a login to clear it again.
    const credentials = { username: 'admin', password: SESSION_AUTH_PASSWORD }
    const loggedOut = await Promise.all([
      fetchAggregatedClients(credentials, ['bareos-dir']),
      fetchAggregatedFilesets(credentials, ['bareos-dir']),
      fetchAggregatedSchedulesShow(credentials, ['bareos-dir']),
    ])
    for (const result of loggedOut) {
      expect(result.directorErrors).toMatchObject([{ director: 'bareos-dir' }])
    }

    auth.login('admin')
    serve('new')
    expectRows(await load(auth), 'new')
    expect(createDirectorCommandSession).toHaveBeenCalledTimes(9)
  })
})
