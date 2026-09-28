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
  EVALUATION_URL,
  EXPERT_CIRCLE_URL,
  MENU_OFFERINGS,
  SERVICES_URL,
  SUBSCRIPTION_URL,
  SUPPORT_URL,
  UNSUPPORTED_BUILD_TEXT,
  buildKindLabel,
  classifyBinaryInfo,
  RIBBON_DISMISSED_STORAGE_KEY,
  closedRibbonNotification,
  formatDocumentTitle,
  loadRibbonDismissed,
  storeRibbonDismissed,
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
    removeItem: (key) => { delete data[key] },
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
    const urls = [SERVICES_URL, EXPERT_CIRCLE_URL, EVALUATION_URL, SUBSCRIPTION_URL,
      SUPPORT_URL, ...COMMERCIAL_OFFERINGS.map(offering => offering.url)]
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

  it('leads with the evaluation program, then subscription packages', () => {
    expect(COMMERCIAL_OFFERINGS[0]).toMatchObject({ id: 'evaluation', url: EVALUATION_URL })
    expect(COMMERCIAL_OFFERINGS[1].id).toBe('subscription')
    expect(COMMERCIAL_OFFERINGS.filter(offering => offering.highlight).map(offering => offering.id))
      .toEqual(['evaluation'])
    expect(EVALUATION_URL).toBe('https://www.bareos.com/try/')
    expect(MENU_OFFERINGS.map(offering => offering.id))
      .toEqual(['evaluation', 'subscription', 'support', 'expert-circle'])
  })

  it('does not name the company in user-facing texts', () => {
    const texts = [
      UNSUPPORTED_BUILD_TEXT,
      ...COMMERCIAL_OFFERINGS.flatMap(offering => [offering.label, offering.description]),
      ...Object.values(BUILD_KINDS).map(buildKindLabel),
    ]
    for (const text of texts) expect(text).not.toMatch(/GmbH/)
  })

  it('remembers a closed ribbon in the given storage', () => {
    const storage = memoryStorage()
    expect(loadRibbonDismissed(storage)).toBe(false)
    storeRibbonDismissed(true, storage)
    expect(storage.data[RIBBON_DISMISSED_STORAGE_KEY]).toBe('1')
    expect(loadRibbonDismissed(storage)).toBe(true)
    storeRibbonDismissed(false, storage)
    expect(loadRibbonDismissed(storage)).toBe(false)
    expect(loadRibbonDismissed(null)).toBe(false)
    const throwing = { getItem() { throw new Error('denied') }, setItem() { throw new Error('denied') } }
    expect(loadRibbonDismissed(throwing)).toBe(false)
    expect(() => storeRibbonDismissed(true, throwing)).not.toThrow()
  })

  it('says the build is not for production use', () => {
    expect(UNSUPPORTED_BUILD_TEXT).toMatch(/not for production use/)
  })

  it('offers evaluation and subscription after the ribbon is closed', () => {
    const opened = []
    const options = closedRibbonNotification(text => `T:${text}`, url => opened.push(url))
    expect(options.message).toBe('T:For production use: try it for free or buy a subscription')
    expect(options.classes).toBe('unsupported-build-toast')
    expect(options.actions.map(action => action.label))
      .toEqual(['T:Try for free', 'T:Buy a subscription'])
    options.actions.forEach(action => action.handler())
    expect(opened).toEqual([EVALUATION_URL, SUBSCRIPTION_URL])
  })

  it('marks the browser tab title for unsupported builds', () => {
    expect(formatDocumentTitle('Jobs')).toBe('Jobs - Bareos')
    expect(formatDocumentTitle('Jobs', true)).toBe('Jobs - Bareos (unsupported build)')
    expect(formatDocumentTitle(undefined)).toBe('Bareos')
    expect(formatDocumentTitle('', true)).toBe('Bareos (unsupported build)')
  })
})

describe('build info store', () => {
  beforeEach(() => {
    localStorage.clear()
    sessionStorage.clear()
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

  it('keeps the ribbon closed for the session until it is reset', () => {
    const store = useBuildInfoStore()
    expect(store.ribbonDismissed).toBe(false)
    store.setRibbonDismissed(true)
    setActivePinia(createPinia())
    expect(useBuildInfoStore().ribbonDismissed).toBe(true)
    useBuildInfoStore().setRibbonDismissed(false)
    expect(sessionStorage.getItem(RIBBON_DISMISSED_STORAGE_KEY)).toBeNull()
  })

  it('ignores status results without a header', () => {
    const store = useBuildInfoStore()
    store.recordStatus('dir-1', {})
    store.recordStatus('', { header: { binary_info: 'Bareos subscription' } })
    expect(store.kindsByDirector).toEqual({})
    expect(store.promote).toBe(true)
  })
})
