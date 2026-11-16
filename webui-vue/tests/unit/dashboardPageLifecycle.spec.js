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

import { createApp, defineComponent, h } from 'vue'
import { createPinia, setActivePinia } from 'pinia'
import { createI18n } from 'vue-i18n'
import { createMemoryHistory, createRouter } from 'vue-router'
import { describe, expect, it, vi } from 'vitest'
import { useDirectorStore } from '../../src/stores/director.js'

vi.mock('quasar', () => ({ useQuasar: () => ({ screen: { lt: { sm: false } } }) }))
vi.mock('../../src/dashboard/DashboardGrid.vue', () => ({ default: defineComponent({ setup: () => () => null }) }))
vi.mock('../../src/dashboard/WidgetPickerDialog.vue', () => ({ default: defineComponent({ setup: () => () => null }) }))
vi.mock('../../src/dashboard/WidgetConfigDialog.vue', () => ({ default: defineComponent({ setup: () => () => null }) }))
const { default: DashboardPage } = await import('../../src/pages/DashboardPage.vue')

describe('dashboard page lifecycle', () => {
  it('does not start an interval when Director loading finishes after unmount', async () => {
    const pinia = createPinia()
    setActivePinia(pinia)
    let resolveLoading
    useDirectorStore().fetchAvailableDirectors = vi.fn(() => new Promise(resolve => { resolveLoading = resolve }))
    const router = createRouter({
      history: createMemoryHistory(),
      routes: [{ path: '/dashboard/:slug?', component: DashboardPage }],
    })
    await router.push('/dashboard')
    await router.isReady()
    const app = createApp(DashboardPage)
    app.use(pinia)
    app.use(router)
    app.use(createI18n({ legacy: false, locale: 'en', messages: { en: {} }, missingWarn: false, fallbackWarn: false }))
    const wrapper = defineComponent({ setup: (_, { slots, attrs }) => () => h('div', attrs, slots.default?.()) })
    for (const name of ['q-banner', 'q-btn', 'q-card', 'q-card-actions', 'q-card-section', 'q-dialog', 'q-icon', 'q-input', 'q-page', 'q-route-tab', 'q-separator', 'q-space', 'q-tabs']) {
      app.component(name, wrapper)
    }
    const interval = vi.spyOn(globalThis, 'setInterval')
    const host = document.createElement('div')
    app.mount(host)
    app.unmount()
    resolveLoading()
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(interval).not.toHaveBeenCalled()
    interval.mockRestore()
  })
})
