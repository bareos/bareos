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
import {
  BUILD_KINDS,
  BUILD_KIND_STORAGE_KEY,
  COMMERCIAL_OFFERINGS,
  EXPERT_CIRCLE_URL,
  SERVICES_URL,
  classifyBinaryInfo,
  loadCachedBuildKind,
  shouldPromote,
  storeBuildKind,
  summariseBuildKinds,
} from '../../src/utils/commercialOffering.js'
import { useBuildInfoStore } from '../../src/stores/buildInfo.js'

function memoryStorage(initial = {}) {
  const data = { ...initial }
  return {
    data,
    getItem: key => (key in data ? data[key] : null),
    setItem: (key, value) => { data[key] = String(value) },
  }
}

describe('commercial offering helpers', () => {
  it('classifies the binary_info values used by the build definitions', () => {
    expect(classifyBinaryInfo('Bareos subscription')).toBe(BUILD_KINDS.subscription)
    expect(classifyBinaryInfo('Bareos community')).toBe(BUILD_KINDS.community)
    expect(classifyBinaryInfo('Bareos pre-release')).toBe(BUILD_KINDS.prerelease)
    expect(classifyBinaryInfo('self-compiled')).toBe(BUILD_KINDS.selfcompiled)
    expect(classifyBinaryInfo('  BAREOS SUBSCRIPTION ')).toBe(BUILD_KINDS.subscription)
    expect(classifyBinaryInfo('')).toBe(BUILD_KINDS.unknown)
    expect(classifyBinaryInfo(undefined)).toBe(BUILD_KINDS.unknown)
    expect(classifyBinaryInfo('something else')).toBe(BUILD_KINDS.unknown)
  })

  it('promotes unless every director is a subscription build', () => {
    expect(shouldPromote([])).toBe(true)
    expect(shouldPromote([BUILD_KINDS.subscription])).toBe(false)
    expect(shouldPromote([BUILD_KINDS.subscription, BUILD_KINDS.subscription])).toBe(false)
    expect(shouldPromote([BUILD_KINDS.subscription, BUILD_KINDS.community])).toBe(true)
    expect(shouldPromote([BUILD_KINDS.unknown])).toBe(true)
  })

  it('describes a mixed setup by its least supported build', () => {
    expect(summariseBuildKinds([BUILD_KINDS.subscription, BUILD_KINDS.selfcompiled]))
      .toBe(BUILD_KINDS.selfcompiled)
    expect(summariseBuildKinds([BUILD_KINDS.community, BUILD_KINDS.prerelease]))
      .toBe(BUILD_KINDS.prerelease)
    expect(summariseBuildKinds([BUILD_KINDS.subscription])).toBe(BUILD_KINDS.subscription)
    expect(summariseBuildKinds([])).toBe(BUILD_KINDS.unknown)
  })

  it('caches the build kind and ignores invalid or unavailable storage', () => {
    const storage = memoryStorage()
    expect(loadCachedBuildKind(storage)).toBe(BUILD_KINDS.unknown)
    storeBuildKind(BUILD_KINDS.subscription, storage)
    expect(storage.data[BUILD_KIND_STORAGE_KEY]).toBe('subscription')
    expect(loadCachedBuildKind(storage)).toBe(BUILD_KINDS.subscription)
    expect(loadCachedBuildKind(memoryStorage({ [BUILD_KIND_STORAGE_KEY]: 'bogus' })))
      .toBe(BUILD_KINDS.unknown)
    expect(loadCachedBuildKind(null)).toBe(BUILD_KINDS.unknown)
    const throwing = { getItem() { throw new Error('denied') }, setItem() { throw new Error('denied') } }
    expect(loadCachedBuildKind(throwing)).toBe(BUILD_KINDS.unknown)
    expect(() => storeBuildKind(BUILD_KINDS.community, throwing)).not.toThrow()
  })

  it('links only to https pages on bareos.com and includes the Expert Circle', () => {
    const urls = [SERVICES_URL, EXPERT_CIRCLE_URL, ...COMMERCIAL_OFFERINGS.map(offering => offering.url)]
    for (const url of urls) {
      const parsed = new URL(url)
      expect(parsed.protocol).toBe('https:')
      expect(parsed.hostname).toBe('www.bareos.com')
      expect(parsed.search).toBe('')
    }
    const ids = COMMERCIAL_OFFERINGS.map(offering => offering.id)
    expect(ids).toEqual(expect.arrayContaining([
      'subscription', 'support', 'training', 'consulting', 'development', 'expert-circle',
    ]))
    expect(new Set(ids).size).toBe(ids.length)
  })
})

describe('build info store', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('promotes on a first visit before any director is known', () => {
    const store = useBuildInfoStore()
    expect(store.kind).toBe(BUILD_KINDS.unknown)
    expect(store.promote).toBe(true)
  })

  it('hides the promotion once all directors are subscription builds and remembers it', () => {
    const store = useBuildInfoStore()
    store.recordStatus('dir-1', { header: { binary_info: 'Bareos subscription' } })
    expect(store.promote).toBe(false)
    expect(localStorage.getItem(BUILD_KIND_STORAGE_KEY)).toBe('subscription')

    setActivePinia(createPinia())
    const reloaded = useBuildInfoStore()
    expect(reloaded.kind).toBe(BUILD_KINDS.subscription)
    expect(reloaded.promote).toBe(false)
  })

  it('promotes if any director is not a subscription build', () => {
    const store = useBuildInfoStore()
    store.recordStatus('dir-1', { header: { binary_info: 'Bareos subscription' } })
    store.recordStatus('dir-2', { header: { binary_info: 'Bareos community' } })
    expect(store.promote).toBe(true)
    expect(store.kind).toBe(BUILD_KINDS.community)

    store.retain(['dir-1'])
    expect(store.promote).toBe(false)
  })

  it('ignores status results without a header', () => {
    const store = useBuildInfoStore()
    store.recordStatus('dir-1', {})
    store.recordStatus('', { header: { binary_info: 'Bareos subscription' } })
    expect(store.kindsByDirector).toEqual({})
    expect(store.promote).toBe(true)
  })
})
