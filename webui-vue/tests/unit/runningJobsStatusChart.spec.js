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

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key === 'Total' ? 'Gesamt' : key }) }))
vi.mock('vue-router', () => ({ useRouter: () => ({ push: vi.fn() }) }))
vi.mock('vue-chartjs', () => ({
  Doughnut: defineComponent({
    props: ['data', 'options', 'ariaLabel'],
    setup: props => () => h('canvas', { 'aria-label': props.ariaLabel }),
  }),
}))

const { default: RunningJobsStatusChartWidget } = await import(
  '../../src/dashboard/widgets/RunningJobsStatusChartWidget.vue'
)

describe('running jobs chart accessibility', () => {
  it('exposes a localized total and each bucket count on the canvas', () => {
    const app = createApp(RunningJobsStatusChartWidget)
    app.component('q-spinner', defineComponent({ setup: () => () => null }))
    app.provide(DASHBOARD_CONTEXT_KEY, {
      aggregate: ref({ runningJobs: [
        { runtimeStatus: 'Running' },
        { runtimeStatus: 'Running' },
        { runtimeStatus: 'is waiting execution' },
      ] }),
      loading: ref(false),
    })
    const host = document.createElement('div')
    app.mount(host)
    try {
      const label = host.querySelector('canvas').getAttribute('aria-label')
      expect(label).toContain('Gesamt: 3')
      expect(label).toContain('Running: 2')
      expect(label).toContain('is waiting execution: 1')
    } finally {
      app.unmount()
    }
  })
})
