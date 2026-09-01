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

// Small helper implementing a TTL cache that is safe to use with
// concurrent/in-flight async fetches:
//  - expired entries are pruned lazily on read instead of growing forever
//  - clear() bumps a generation counter so that any fetch that was already
//    in flight when clear() was called (e.g. triggered by logout) will not
//    resurrect stale data into the cache once it resolves.
export function createTtlCache() {
  const store = new Map()
  let generation = 0

  // Returns the cached data if present and still within ttlMs, otherwise
  // evicts the (now stale) entry so the map doesn't grow unboundedly with
  // dead entries, and returns undefined.
  function get(key, ttlMs) {
    const entry = store.get(key)
    if (!entry) {
      return undefined
    }
    if (Date.now() - entry.timestamp >= ttlMs) {
      store.delete(key)
      return undefined
    }
    return entry.data
  }

  function beginFetch() {
    return generation
  }

  function set(key, data, fetchGeneration) {
    // Ignore stale writes from fetches started before the most recent clear().
    if (fetchGeneration !== generation) {
      return
    }
    store.set(key, { timestamp: Date.now(), data })
  }

  function clear() {
    generation += 1
    store.clear()
  }

  return { get, beginFetch, set, clear }
}
