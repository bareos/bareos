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

import { createApp, defineComponent, h, ref } from 'vue'
import { describe, expect, it, vi } from 'vitest'
import { DASHBOARD_CONTEXT_KEY } from '../../src/dashboard/dashboardContext.js'

const mocks = vi.hoisted(() => ({
  fetchJobs: vi.fn(),
  createClient: vi.fn(),
  switchDirector: vi.fn(),
  push: vi.fn(),
}))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({ useRouter: () => ({ push: mocks.push }) }))
vi.mock('quasar', () => ({ useQuasar: () => ({ notify: vi.fn() }) }))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: director => ({ director, password: 'secret' }) }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ locale: 'en', relativeTime: false }),
}))
vi.mock('../../src/composables/jobsAggregate.js', () => ({ fetchAggregatedTroubleJobs: mocks.fetchJobs }))
vi.mock('../../src/composables/directorAggregate.js', () => ({ createDirectorCommandClient: mocks.createClient }))
vi.mock('../../src/composables/useDirectorSession.js', () => ({ switchActiveDirector: mocks.switchDirector }))
const { default: TroubleViewWidget } = await import('../../src/dashboard/widgets/TroubleViewWidget.vue')

describe('trouble view', () => {
  it.each([true, false])('reports failed Director log fetches with healthy lines: %s', async (hasLines) => {
    mocks.fetchJobs.mockResolvedValue({
      jobs: [{ id: 7, name: 'Job A', director: 'A' }, { id: 8, name: 'Job B', director: 'B' }],
      totalJobs: 2, truncated: false, directorErrors: [],
    })
    mocks.createClient.mockImplementation(async ({ director }) => ({
      call: director === 'A'
        ? async () => { throw new Error('log unavailable') }
        : async () => ({ joblog: hasLines ? [{ jobid: 8, logtext: 'Error: B failed' }] : [] }),
      disconnect: vi.fn(),
    }))
    const app = createApp(TroubleViewWidget)
    app.provide(DASHBOARD_CONTEXT_KEY, {
      activeDirectors: ref(['A', 'B']), refreshToken: ref(0),
    })
    const wrapper = defineComponent({
      setup: (_, { slots, attrs }) => () => h('div', attrs, slots.default?.()),
    })
    for (const name of ['q-btn', 'q-banner', 'q-scroll-area', 'q-icon', 'q-list', 'q-item', 'q-item-section', 'q-item-label']) {
      app.component(name, wrapper)
    }
    const host = document.createElement('div')
    app.mount(host)
    try {
      await vi.waitFor(() => expect(host.textContent).toContain('A: log unavailable'))
      if (hasLines) expect(host.textContent).toContain('B failed')
      expect(host.textContent).not.toContain('No errors or warnings in the last 24 hours.')
    } finally {
      app.unmount()
    }
  })

  it('associates duplicate job IDs with the correct Director and job', async () => {
    mocks.fetchJobs.mockResolvedValue({
      jobs: [
        { id: 7, name: 'Job A', director: 'A' },
        { id: 7, name: 'Job B', director: 'B' },
      ],
      totalJobs: 2, truncated: false, directorErrors: [],
    })
    mocks.createClient.mockImplementation(async ({ director }) => ({
      call: async () => ({ joblog: [{ jobid: 7, logtext: `Error: ${director} failed` }] }),
      disconnect: vi.fn(),
    }))
    mocks.switchDirector.mockResolvedValue()
    mocks.push.mockResolvedValue()
    const app = createApp(TroubleViewWidget)
    app.provide(DASHBOARD_CONTEXT_KEY, {
      activeDirectors: ref(['A', 'B']), refreshToken: ref(0),
    })
    const wrapper = defineComponent({
      setup: (_, { slots, attrs }) => () => h('div', attrs, slots.default?.()),
    })
    for (const name of ['q-btn', 'q-banner', 'q-scroll-area', 'q-icon', 'q-list', 'q-item', 'q-item-section', 'q-item-label']) {
      app.component(name, wrapper)
    }
    const host = document.createElement('div')
    app.mount(host)
    try {
      await vi.waitFor(() => expect(host.textContent).toContain('A failed'))
      expect(host.textContent).toContain('Job A (#7) · A')
      expect(host.textContent).toContain('Job B (#7) · B')
      const line = [...host.querySelectorAll('.log-line-text')].find(el => el.textContent.includes('A failed'))
      line.parentElement.parentElement.click()
      await vi.waitFor(() => expect(mocks.switchDirector).toHaveBeenCalledWith('A'))
      expect(mocks.push).toHaveBeenCalledWith(expect.objectContaining({
        params: { id: 7 }, query: expect.objectContaining({ director: 'A' }),
      }))
    } finally {
      app.unmount()
    }
  })
})
