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
import {
  createTtlCache,
  hashCacheFingerprint,
} from '../../src/composables/ttlCache.js'

describe('createTtlCache', () => {
  beforeEach(() => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-01-01T00:00:00Z'))
  })

  afterEach(() => {
    vi.useRealTimers()
  })

  it('returns undefined for an unknown key', () => {
    const cache = createTtlCache()
    expect(cache.get('missing', 1000)).toBeUndefined()
  })

  it('returns a stored value while it is within the ttl', () => {
    const cache = createTtlCache()
    cache.set('k', { a: 1 }, cache.beginFetch())

    vi.advanceTimersByTime(999)
    expect(cache.get('k', 1000)).toEqual({ a: 1 })
  })

  it('expires a value at the ttl and evicts it', () => {
    const cache = createTtlCache()
    cache.set('k', 'v', cache.beginFetch())

    vi.advanceTimersByTime(1000)
    expect(cache.get('k', 1000)).toBeUndefined()

    // Evicted: a longer ttl no longer revives it.
    expect(cache.get('k', 60000)).toBeUndefined()
  })

  it('applies the ttl given on read, not on write', () => {
    const cache = createTtlCache()
    cache.set('k', 'v', cache.beginFetch())

    vi.advanceTimersByTime(500)
    expect(cache.get('k', 100)).toBeUndefined()
  })

  it('clear() drops all entries', () => {
    const cache = createTtlCache()
    const generation = cache.beginFetch()
    cache.set('a', 1, generation)
    cache.set('b', 2, generation)

    cache.clear()

    expect(cache.get('a', 1000)).toBeUndefined()
    expect(cache.get('b', 1000)).toBeUndefined()
  })

  it('ignores writes from a fetch that started before clear()', () => {
    const cache = createTtlCache()
    const staleGeneration = cache.beginFetch()

    cache.clear()
    cache.set('k', 'stale', staleGeneration)
    expect(cache.get('k', 1000)).toBeUndefined()

    cache.set('k', 'fresh', cache.beginFetch())
    expect(cache.get('k', 1000)).toBe('fresh')
  })
})

describe('hashCacheFingerprint', () => {
  it('is deterministic', () => {
    expect(hashCacheFingerprint('secret')).toBe(hashCacheFingerprint('secret'))
  })

  it('differs for different inputs', () => {
    expect(hashCacheFingerprint('secret-a')).not.toBe(
      hashCacheFingerprint('secret-b')
    )
  })

  it('does not contain the raw value', () => {
    expect(hashCacheFingerprint('hunter2')).not.toContain('hunter2')
  })

  it('matches the 32-bit FNV-1a reference values', () => {
    expect(hashCacheFingerprint('')).toBe('811c9dc5')
    expect(hashCacheFingerprint('a')).toBe('e40c292c')
  })

  it('treats null and undefined like an empty string', () => {
    expect(hashCacheFingerprint(null)).toBe(hashCacheFingerprint(''))
    expect(hashCacheFingerprint(undefined)).toBe(hashCacheFingerprint(''))
  })
})
