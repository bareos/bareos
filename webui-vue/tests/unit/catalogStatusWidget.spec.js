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
import { aggregateDirectorDashboardSnapshots } from '../../src/composables/directorAggregate.js'

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-chartjs', () => ({
  Doughnut: defineComponent({ setup: () => () => h('canvas') }),
}))

const { default: CatalogStatusWidget } = await import(
  '../../src/dashboard/widgets/CatalogStatusWidget.vue'
)

describe('catalog status widget', () => {
  it('shows incomplete coverage and identifies the failing director', () => {
    const app = createApp(CatalogStatusWidget)
    app.component('q-badge', defineComponent({
      props: ['label'],
      setup: (props, { slots }) => () => h('div', [props.label, slots.default?.()]),
    }))
    app.component('q-tooltip', defineComponent({
      setup: (_, { slots }) => () => h('div', slots.default?.()),
    }))
    app.component('q-spinner', defineComponent({ setup: () => () => null }))
    const aggregate = aggregateDirectorDashboardSnapshots([
      { director: 'healthy', catalogStatus: {
        director: 'healthy', status: 'ok', tables: [{ name: 'job', bytes: 42 }],
      } },
      { director: 'offline', catalogStatus: {
        director: 'offline', status: 'unavailable', errors: ['Connection failed'],
      } },
    ])
    app.provide(DASHBOARD_CONTEXT_KEY, { aggregate: ref(aggregate), loading: ref(false) })
    const host = document.createElement('div')
    app.mount(host)
    try {
      expect(host.querySelector('canvas')).not.toBeNull()
      expect(host.textContent).toContain('warning')
      expect(host.textContent).toContain('offline: Connection failed')
    } finally {
      app.unmount()
    }
  })
})
