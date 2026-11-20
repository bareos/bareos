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
//  - expired entries are pruned on cache access and the store has a hard cap
//  - clear() bumps a generation counter so that any fetch that was already
//    in flight when clear() was called (e.g. triggered by logout) will not
//    resurrect stale data into the cache once it resolves.
export function createTtlCache() {
  const maxEntries = 128
  const store = new Map()
  let generation = 0
  let nextFetchId = 0
  const latestFetchByKey = new Map()

  function pruneExpired(ttlMs) {
    const now = Date.now()
    for (const [entryKey, entry] of store) {
      if (now - entry.timestamp >= ttlMs) store.delete(entryKey)
    }
  }

  function get(key, ttlMs) {
    pruneExpired(ttlMs)
    return store.get(key)?.data
  }

  function beginFetch(key, ttlMs = 60_000) {
    pruneExpired(ttlMs)
    const fetch = { generation, id: ++nextFetchId }
    latestFetchByKey.set(key, fetch.id)
    return fetch
  }

  function isLatestFetch(key, fetch) {
    return fetch.generation === generation && fetch.id === latestFetchByKey.get(key)
  }

  function set(key, data, fetch) {
    if (!isLatestFetch(key, fetch)) return
    if (!store.has(key)) {
      while (store.size >= maxEntries) {
        store.delete(store.keys().next().value)
      }
    }
    store.set(key, { timestamp: Date.now(), data })
  }

  function discard(key, fetch) {
    if (isLatestFetch(key, fetch)) store.delete(key)
  }

  function finishFetch(key, fetch) {
    if (isLatestFetch(key, fetch)) latestFetchByKey.delete(key)
  }

  function clear() {
    generation += 1
    store.clear()
    latestFetchByKey.clear()
  }

  return { get, beginFetch, set, discard, finishFetch, clear }
}

// Cheap, non-cryptographic string hash (FNV-1a, 32-bit) used to fold a
// session credential (e.g. password) into a cache key without storing the
// raw secret as a literal Map key. This is not a security boundary by
// itself -- it only ensures cache entries are scoped per authenticated
// session rather than per username, so a re-login with different
// credentials under the same username can't reuse another session's
// cached catalog data within the TTL window.
export function hashCacheFingerprint(value) {
  let hash = 0x811c9dc5
  const input = String(value ?? '')
  for (let i = 0; i < input.length; i += 1) {
    hash ^= input.charCodeAt(i)
    hash = Math.imul(hash, 0x01000193)
  }
  return (hash >>> 0).toString(16)
}
