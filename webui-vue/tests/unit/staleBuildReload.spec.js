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
import {
  clearReloadGuard,
  installStaleBuildReloadHandling,
  isDynamicImportFailure,
  reloadOnce,
} from '../../src/utils/staleBuildReload.js'

function fakeStorage() {
  const store = new Map()
  return {
    getItem: (key) => (store.has(key) ? store.get(key) : null),
    setItem: (key, value) => store.set(key, value),
    removeItem: (key) => store.delete(key),
  }
}

function fakeWindow() {
  const listeners = {}
  return {
    addEventListener: (type, handler) => {
      listeners[type] = listeners[type] || []
      listeners[type].push(handler)
    },
    dispatch: (type, event) => {
      for (const handler of listeners[type] || []) handler(event)
    },
    location: { reload: vi.fn() },
  }
}

describe('isDynamicImportFailure', () => {
  it('matches Vite/browser stale dynamic import error messages', () => {
    expect(isDynamicImportFailure(new Error('Failed to fetch dynamically imported module: https://x/y.js'))).toBe(true)
    expect(isDynamicImportFailure(new Error('error loading dynamically imported module'))).toBe(true)
    expect(isDynamicImportFailure(new Error('Importing a module script failed'))).toBe(true)
    expect(isDynamicImportFailure(new Error('Unable to preload CSS for https://x/y.css'))).toBe(true)
  })

  it('does not match unrelated errors', () => {
    expect(isDynamicImportFailure(new Error('Network Error'))).toBe(false)
    expect(isDynamicImportFailure(undefined)).toBe(false)
  })
})

describe('reloadOnce', () => {
  it('reloads exactly once per session', () => {
    const storage = fakeStorage()
    const reload = vi.fn()

    expect(reloadOnce(storage, reload)).toBe(true)
    expect(reload).toHaveBeenCalledTimes(1)

    expect(reloadOnce(storage, reload)).toBe(false)
    expect(reload).toHaveBeenCalledTimes(1)
  })

  it('reloads again after the guard is cleared', () => {
    const storage = fakeStorage()
    const reload = vi.fn()

    reloadOnce(storage, reload)
    clearReloadGuard(storage)
    reloadOnce(storage, reload)

    expect(reload).toHaveBeenCalledTimes(2)
  })

  it('still reloads once even if sessionStorage throws', () => {
    const throwingStorage = {
      getItem: () => { throw new Error('quota') },
      setItem: () => { throw new Error('quota') },
    }
    const reload = vi.fn()

    expect(reloadOnce(throwingStorage, reload)).toBe(true)
    expect(reload).toHaveBeenCalledTimes(1)
  })
})

describe('installStaleBuildReloadHandling', () => {
  it('reloads once on a matching vite:preloadError payload', () => {
    const windowObj = fakeWindow()
    const storage = fakeStorage()

    installStaleBuildReloadHandling(undefined, { windowObj, storage, reload: windowObj.location.reload })

    const event = { payload: new Error('Failed to fetch dynamically imported module: https://x/y.js') }
    windowObj.dispatch('vite:preloadError', event)
    windowObj.dispatch('vite:preloadError', event)

    expect(windowObj.location.reload).toHaveBeenCalledTimes(1)
  })

  it('ignores a vite:preloadError payload that is not a stale-chunk failure', () => {
    const windowObj = fakeWindow()
    const storage = fakeStorage()

    installStaleBuildReloadHandling(undefined, { windowObj, storage, reload: windowObj.location.reload })

    windowObj.dispatch('vite:preloadError', { payload: new Error('transient network hiccup') })

    expect(windowObj.location.reload).not.toHaveBeenCalled()
  })

  it('reloads once on a matching router.onError failure, ignores others', () => {
    const windowObj = fakeWindow()
    const storage = fakeStorage()
    let errorHandler
    const router = { onError: (handler) => { errorHandler = handler } }

    installStaleBuildReloadHandling(router, { windowObj, storage, reload: windowObj.location.reload })

    errorHandler(new Error('some unrelated navigation error'))
    expect(windowObj.location.reload).not.toHaveBeenCalled()

    errorHandler(new Error('Failed to fetch dynamically imported module: https://x/y.js'))
    expect(windowObj.location.reload).toHaveBeenCalledTimes(1)
  })

  it('exposes a clearReloadGuard helper bound to the injected storage', () => {
    const windowObj = fakeWindow()
    const storage = fakeStorage()

    const { clearReloadGuard: clear } = installStaleBuildReloadHandling(undefined, {
      windowObj,
      storage,
      reload: windowObj.location.reload,
    })

    const event = { payload: new Error('Failed to fetch dynamically imported module: https://x/y.js') }
    windowObj.dispatch('vite:preloadError', event)
    clear()
    windowObj.dispatch('vite:preloadError', event)

    expect(windowObj.location.reload).toHaveBeenCalledTimes(2)
  })
})
