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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h } from 'vue'

vi.mock('vue-i18n', () => ({
  useI18n: () => ({ t: key => key }),
}))
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: vi.fn() }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ locale: 'en-EN' }),
}))

const { default: CombinedJobScheduleTimeline } = await import(
  '../../src/components/CombinedJobScheduleTimeline.vue'
)

const apps = []
const hosts = []

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
  vi.useRealTimers()
})

describe('CombinedJobScheduleTimeline', () => {
  it('keeps partial timeline data visible alongside director errors', () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-05-27T12:00:00'))
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)

    const app = createApp(CombinedJobScheduleTimeline, {
      actualJobs: [{
        id: 17,
        name: 'DailyBackup',
        client: 'client-a',
        director: 'director-a',
        status: 'T',
        starttime: '2026-05-27 08:00:00',
        endtime: '2026-05-27 08:10:00',
      }],
      error: 'director-b: connection failed',
      directors: ['director-a', 'director-b'],
    })
    const componentStub = defineComponent({
      inheritAttrs: false,
      setup(_, { attrs, slots }) {
        return () => h('span', attrs, slots.default?.())
      },
    })
    for (const name of ['q-btn', 'q-chip', 'q-space', 'q-banner', 'q-inner-loading', 'q-tooltip']) {
      app.component(name, componentStub)
    }

    apps.push(app)
    app.mount(host)

    expect(host.querySelector('.text-negative')?.textContent).toContain(
      'director-b: connection failed'
    )
    expect(host.querySelector('[data-testid="combined-timeline-job-bar"]'))
      .not.toBeNull()
  })
})
