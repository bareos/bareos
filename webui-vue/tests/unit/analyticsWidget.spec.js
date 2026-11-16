/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { createApp, defineComponent, h, nextTick, ref } from 'vue'
import { describe, expect, it, vi } from 'vitest'

const { fetchAnalytics } = vi.hoisted(() => ({ fetchAnalytics: vi.fn() }))
const activeDirectors = ref(['old-a', 'old-b'])
vi.mock('../../src/composables/analyticsAggregate.js', () => ({
  fetchAggregatedAnalytics: fetchAnalytics,
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([]),
    selectedDirectorsModel: ref([]),
    activeDirectors,
    isCommonScope: ref(true),
    scopeLabel: ref('All'),
    syncSelectedDirectors: () => {},
    ensureSingleScopeDirector: async () => {},
  }),
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: () => ({ password: 'secret' }) }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({ fetchAvailableDirectors: async () => {} }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ locale: 'en' }),
}))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({ useRouter: () => ({ push: vi.fn() }) }))

const { default: AnalyticsWidgets } = await import('../../src/dashboard/widgets/AnalyticsWidgets.vue')

function deferred() {
  let resolve
  const promise = new Promise(done => { resolve = done })
  return { promise, resolve }
}

describe('standalone analytics', () => {
  it('does not replace current results with an older director selection', async () => {
    const old = deferred()
    const current = deferred()
    fetchAnalytics.mockReset()
    fetchAnalytics.mockReturnValueOnce(old.promise).mockReturnValueOnce(current.promise)
    activeDirectors.value = ['old-a', 'old-b']
    const app = createApp(AnalyticsWidgets)
    app.component('router-link', defineComponent({
      setup: (_, { slots }) => () => h('a', slots.default?.()),
    }))
    const stub = defineComponent({ setup: (_, { slots }) => () => h('div', slots.default?.()) })
    for (const name of ['q-banner', 'q-btn-toggle', 'q-spinner', 'q-icon', 'q-chip']) {
      app.component(name, stub)
    }
    const host = document.createElement('div')
    app.mount(host)
    try {
      activeDirectors.value = ['new-a', 'new-b']
      await nextTick()
      current.resolve({ jobs: [{ status: 'T', bytes: 0, files: 0 }], directorErrors: [] })
      await vi.waitFor(() => expect(host.textContent).toContain('Total Jobs1'))
      old.resolve({ jobs: [], directorErrors: [{ director: 'old-a', message: 'stale failure' }] })
      await new Promise(resolve => setTimeout(resolve, 0))
      expect(host.textContent).toContain('Total Jobs1')
      expect(host.textContent).not.toContain('stale failure')
    } finally {
      app.unmount()
    }
  })
})
