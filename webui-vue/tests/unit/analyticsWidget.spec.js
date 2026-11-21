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
import { DASHBOARD_CONTEXT_KEY } from '../../src/dashboard/dashboardContext.js'

const { fetchAnalytics } = vi.hoisted(() => ({ fetchAnalytics: vi.fn() }))
const activeDirectors = ref(['old-a', 'old-b'])
const isCommonScope = ref(true)
vi.mock('../../src/composables/analyticsAggregate.js', () => ({
  fetchAggregatedAnalytics: fetchAnalytics,
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([]),
    selectedDirectorsModel: ref([]),
    activeDirectors,
    isCommonScope,
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

function mountDashboardWidget(type, jobs) {
  vi.stubGlobal('ResizeObserver', class {
    observe() {}
    disconnect() {}
  })
  const app = createApp(AnalyticsWidgets, { widgetProps: { type } })
  app.provide(DASHBOARD_CONTEXT_KEY, {
    analyticsJobs: ref(jobs),
    analyticsLoading: ref(false),
    aggregate: ref({}),
  })
  app.component('router-link', defineComponent({
    props: ['to'],
    setup: (props, { slots }) => () => h('a', { 'data-query': JSON.stringify(props.to?.query) }, slots.default?.()),
  }))
  app.component('q-btn-toggle', defineComponent({
    emits: ['update:modelValue'],
    setup: (_, { emit }) => () => h('button', { onClick: () => emit('update:modelValue', 'files') }, 'Files'),
  }))
  for (const name of ['q-banner', 'q-spinner', 'q-icon', 'q-chip']) {
    app.component(name, defineComponent({ setup: (_, { slots }) => () => h('div', slots.default?.()) }))
  }
  const host = document.createElement('div')
  app.mount(host)
  return { host, app }
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

  describe('dashboard analytics', () => {
    it.each(['analytics-client-bytes', 'analytics-treemap'])('shows special catalog names in %s', type => {
      isCommonScope.value = false
      const { app, host } = mountDashboardWidget(type, [
        { client: 'constructor', name: 'constructor', director: 'A', bytes: 42, files: 1, type: 'B' },
        { client: '__proto__', name: '__proto__', director: 'A', bytes: 21, files: 1, type: 'B' },
      ])
      try {
        expect(host.textContent).toContain('constructor')
        expect(host.textContent).toContain('__proto__')
      } finally {
        app.unmount()
        isCommonScope.value = true
        vi.unstubAllGlobals()
      }
    })

    it('ranks client tiles by the selected metric before applying the limit', async () => {
      const jobs = Array.from({ length: 12 }, (_, i) => ({
        client: `bytes-${i}`, name: `job-${i}`, director: 'A', bytes: 100 + i, files: 0,
      }))
      jobs.push({ client: 'files-only', name: 'files-job', director: 'A', bytes: 0, files: 1000 })
      const { app, host } = mountDashboardWidget('analytics-client-bytes', jobs)
      try {
        expect(host.textContent).not.toContain('files-only')
        host.querySelector('button').click()
        await nextTick()
        expect(host.textContent).toContain('files-only')
        expect(host.textContent).not.toContain('No data')
      } finally {
        app.unmount()
        vi.unstubAllGlobals()
      }
    })
  })
})
