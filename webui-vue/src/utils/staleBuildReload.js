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

// After a redeploy replaces the webui-vue static assets with a new,
// differently content-hashed build, a browser tab that already has the old
// index.html/entry bundle loaded will keep trying to lazily import route
// chunks and CSS by their old hashed filenames, which no longer exist on the
// server. This surfaces as a wall of 404s and "Failed to fetch dynamically
// imported module"/"Unable to preload CSS for ..." errors. Vite fires a
// `vite:preloadError` window event for stale CSS/JS chunk preloads (with the
// underlying error as `event.payload`), and Vue Router's `onError` hook
// receives a similarly worded error for stale lazy route components. The
// fix for both is the same: reload the page once so the browser fetches the
// current index.html/asset manifest. Only messages that specifically
// indicate a missing/stale chunk are matched, so a one-off transient
// network error elsewhere doesn't trigger an unnecessary reload.
const STALE_CHUNK_FAILURE_PATTERN
  = /Failed to fetch dynamically imported module|error loading dynamically imported module|Importing a module script failed|Unable to preload CSS for/i

const RELOAD_ATTEMPTED_KEY = 'bareos:stale-build-reload-attempted'

export function isDynamicImportFailure(error) {
  const message = error?.message ?? String(error ?? '')
  return STALE_CHUNK_FAILURE_PATTERN.test(message)
}

// Reloads the page at most once per browser tab (guarded via
// sessionStorage, which is cleared when the tab is closed) so a genuinely
// broken/incomplete deployment can't cause a rapid, infinite reload loop --
// clearing the guard is intentionally *not* done automatically on a
// successful mount, since the same stale-chunk failure could otherwise
// recur immediately after the reload and loop.
export function reloadOnce(storage, reload) {
  let alreadyAttempted = false
  try {
    alreadyAttempted = storage?.getItem(RELOAD_ATTEMPTED_KEY) === 'true'
  } catch {
    alreadyAttempted = false
  }
  if (alreadyAttempted) {
    return false
  }
  try {
    storage?.setItem(RELOAD_ATTEMPTED_KEY, 'true')
  } catch {
    // sessionStorage unavailable (e.g. private browsing quota) -- reload
    // anyway, worst case is a single extra reload without loop protection.
  }
  reload?.()
  return true
}

// Exposed mainly for tests; not called automatically on mount (see
// reloadOnce()'s comment on why an eager clear would risk a reload loop).
export function clearReloadGuard(storage) {
  try {
    storage?.removeItem(RELOAD_ATTEMPTED_KEY)
  } catch {
    // ignore
  }
}

// Wires up the reload-on-stale-chunk handling. `windowObj`/`storage`/
// `reload` are injectable for unit testing; in production, call with
// `installStaleBuildReloadHandling(router)` and rely on the defaults.
export function installStaleBuildReloadHandling(
  router,
  {
    windowObj = typeof window !== 'undefined' ? window : undefined,
    storage = windowObj?.sessionStorage,
    reload = () => windowObj?.location?.reload(),
  } = {},
) {
  windowObj?.addEventListener?.('vite:preloadError', (event) => {
    if (isDynamicImportFailure(event?.payload)) {
      reloadOnce(storage, reload)
    }
  })

  router?.onError?.((error) => {
    if (isDynamicImportFailure(error)) {
      reloadOnce(storage, reload)
    }
  })

  return { clearReloadGuard: () => clearReloadGuard(storage) }
}
