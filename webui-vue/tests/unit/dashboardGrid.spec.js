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

import { createApp, defineComponent, h, inject, nextTick, ref } from 'vue'
import { createPinia } from 'pinia'
import { describe, expect, it, vi } from 'vitest'

const { fetchSnapshot, fetchAnalytics } = vi.hoisted(() => ({
  fetchSnapshot: vi.fn(),
  fetchAnalytics: vi.fn(),
}))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('quasar', () => ({ useQuasar: () => ({ screen: { lt: { sm: true } } }) }))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: () => ({ username: 'admin' }) }),
}))
vi.mock('../../src/composables/directorAggregate.js', async (importOriginal) => ({
  ...(await importOriginal()),
  fetchDirectorDashboardSnapshot: fetchSnapshot,
}))
vi.mock('../../src/composables/analyticsAggregate.js', () => ({
  fetchAggregatedAnalytics: fetchAnalytics,
}))
vi.mock('../../src/dashboard/WidgetShell.vue', () => ({
  default: defineComponent({ setup: (_, { slots }) => () => h('div', slots.default?.()) }),
}))
vi.mock('../../src/components/DirectorErrorsBanner.vue', () => ({
  default: defineComponent({
    props: ['errors'],
    setup: props => () => h('div', { class: 'director-errors' }, JSON.stringify(props.errors)),
  }),
}))
vi.mock('../../src/dashboard/widgetRegistry.js', async () => {
  const { DASHBOARD_CONTEXT_KEY } = await import('../../src/dashboard/dashboardContext.js')
  const probe = defineComponent({
    setup() {
      const context = inject(DASHBOARD_CONTEXT_KEY)
      return () => h('div', [
        h('button', { onClick: context.refresh }, 'refresh'),
        h('span', { class: 'pools' }, JSON.stringify(context.pools.value)),
        h('span', { class: 'analytics' }, JSON.stringify(context.analyticsJobs.value)),
        h('span', { class: 'catalog' }, JSON.stringify(context.aggregate.value.catalogStatusSummary)),
        h('span', { class: 'catalog-statuses' }, JSON.stringify(context.aggregate.value.catalogStatuses)),
      ])
    },
  })
  return { getWidgetDefinition: () => ({ component: probe }) }
})

const { default: DashboardGrid } = await import('../../src/dashboard/DashboardGrid.vue')

function deferred() {
  let resolve
  const promise = new Promise(done => { resolve = done })
  return { promise, resolve }
}

describe('DashboardGrid refresh', () => {
  it('reports an unreachable director as unavailable in the catalog summary', async () => {
    fetchSnapshot.mockReset()
    fetchAnalytics.mockReset()
    fetchSnapshot.mockImplementation(({ director }) => director === 'healthy'
      ? Promise.resolve({ director, catalogStatus: { status: 'ok' } })
      : Promise.reject(new Error('Connection failed')))
    const app = createApp(DashboardGrid, {
      dashboard: {
        id: 'test',
        widgets: [{ id: 'one', type: 'catalog-status', layout: { x: 0, y: 0, w: 3, h: 3 } }],
      },
      activeDirectors: ['healthy', 'offline'],
    })
    app.use(createPinia())
    app.component('q-icon', defineComponent({ setup: () => () => null }))
    const host = document.createElement('div')
    document.body.appendChild(host)
    app.mount(host)
    try {
      await vi.waitFor(() => {
        const summary = JSON.parse(host.querySelector('.catalog').textContent)
        expect(summary.status).toBe('warning')
        expect(summary.unavailableDirectors).toBe(1)
        expect(summary.directors).toBe(2)
        const statuses = JSON.parse(host.querySelector('.catalog-statuses').textContent)
        expect(statuses).toContainEqual(expect.objectContaining({
          director: 'offline', status: 'unavailable', errors: ['Connection failed'],
        }))
      })
    } finally {
      app.unmount()
      host.remove()
    }
  })

  it('keeps slow pool and analytics results after a fast refresh', async () => {
    const slowSnapshot = deferred()
    const slowAnalytics = deferred()
    fetchSnapshot.mockReset()
    fetchAnalytics.mockReset()
    fetchSnapshot.mockReturnValueOnce(slowSnapshot.promise)
      .mockResolvedValueOnce({ director: 'dir', pools: [] })
    fetchAnalytics.mockReturnValueOnce(slowAnalytics.promise)
    const app = createApp(DashboardGrid, {
      dashboard: {
        id: 'test',
        widgets: [{ id: 'one', type: 'analytics-summary', layout: { x: 0, y: 0, w: 3, h: 3 } }],
      },
      activeDirectors: ['dir'],
    })
    app.use(createPinia())
    app.component('q-icon', defineComponent({ setup: () => () => null }))
    const host = document.createElement('div')
    document.body.appendChild(host)
    app.mount(host)
    try {
      host.querySelector('button').click()
      await vi.waitFor(() => expect(fetchSnapshot).toHaveBeenCalledTimes(2))
      slowSnapshot.resolve({ director: 'dir', pools: [{ name: 'PoolA' }] })
      slowAnalytics.resolve({ jobs: [{ id: 42 }], directorErrors: [] })
      await vi.waitFor(() => {
        expect(host.querySelector('.pools').textContent).toContain('PoolA')
        expect(host.querySelector('.analytics').textContent).toContain('42')
      })
    } finally {
      app.unmount()
      host.remove()
    }
  })

  it('discards older pool and analytics results after a director change', async () => {
    const oldSnapshot = deferred()
    const oldAnalytics = deferred()
    fetchSnapshot.mockReset()
    fetchAnalytics.mockReset()
    fetchSnapshot.mockReturnValueOnce(oldSnapshot.promise)
      .mockResolvedValueOnce({ director: 'new', pools: [{ name: 'NewPool' }] })
    fetchAnalytics.mockReturnValueOnce(oldAnalytics.promise)
      .mockResolvedValueOnce({ jobs: [{ id: 99 }], directorErrors: [] })
    const directors = ref(['old'])
    const dashboard = {
      id: 'test',
      widgets: [{ id: 'one', type: 'analytics-summary', layout: { x: 0, y: 0, w: 3, h: 3 } }],
    }
    const app = createApp(defineComponent({
      setup: () => () => h(DashboardGrid, { dashboard, activeDirectors: directors.value }),
    }))
    app.use(createPinia())
    app.component('q-icon', defineComponent({ setup: () => () => null }))
    const host = document.createElement('div')
    document.body.appendChild(host)
    app.mount(host)
    try {
      directors.value = ['new']
      await nextTick()
      await vi.waitFor(() => {
        expect(host.querySelector('.pools').textContent).toContain('NewPool')
        expect(host.querySelector('.analytics').textContent).toContain('99')
      })
      oldSnapshot.resolve({ director: 'old', pools: [{ name: 'OldPool' }] })
      oldAnalytics.resolve({ jobs: [{ id: 1 }], directorErrors: [] })
      await new Promise(resolve => setTimeout(resolve, 0))
      expect(host.querySelector('.pools').textContent).toContain('NewPool')
      expect(host.querySelector('.pools').textContent).not.toContain('OldPool')
      expect(host.querySelector('.analytics').textContent).not.toContain('"id":1')
    } finally {
      app.unmount()
      host.remove()
    }
  })

  it('shows partial analytics errors even without a summary widget and clears them after refresh', async () => {
    fetchSnapshot.mockReset()
    fetchAnalytics.mockReset()
    fetchSnapshot.mockResolvedValue({ director: 'healthy', pools: [] })
    fetchAnalytics.mockResolvedValueOnce({
      jobs: [{ id: 42 }],
      directorErrors: [{ director: 'offline', message: 'Connection failed' }],
    }).mockResolvedValueOnce({ jobs: [{ id: 42 }, { id: 43 }], directorErrors: [] })
    const directors = ref(['healthy', 'offline'])
    const dashboard = {
      id: 'test',
      widgets: [{ id: 'one', type: 'analytics-treemap', layout: { x: 0, y: 0, w: 3, h: 3 } }],
    }
    const app = createApp(defineComponent({
      setup: () => () => h(DashboardGrid, { dashboard, activeDirectors: directors.value }),
    }))
    app.use(createPinia())
    app.component('q-icon', defineComponent({ setup: () => () => null }))
    const host = document.createElement('div')
    document.body.appendChild(host)
    app.mount(host)
    try {
      await vi.waitFor(() => {
        expect(host.querySelector('.analytics').textContent).toContain('42')
        expect(host.querySelector('.director-errors').textContent).toContain('Connection failed')
      })
      directors.value = ['healthy']
      await vi.waitFor(() => {
        expect(host.querySelector('.analytics').textContent).toContain('43')
        expect(host.querySelector('.director-errors').textContent).toBe('[]')
      })
    } finally {
      app.unmount()
      host.remove()
    }
  })
})
