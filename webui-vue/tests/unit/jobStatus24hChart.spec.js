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

import { describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, ref } from 'vue'
import { createI18n } from 'vue-i18n'
import { createMemoryHistory, createRouter } from 'vue-router'
import JobsPast24hWidget from '../../src/dashboard/widgets/JobsPast24hWidget.vue'
import JobStatus24hChartWidget from '../../src/dashboard/widgets/JobStatus24hChartWidget.vue'
import { DASHBOARD_CONTEXT_KEY } from '../../src/dashboard/dashboardContext.js'
import { getContrastTextColor } from '../../src/dashboard/piePalette.js'
import {
  JOB_STATUS_24H_CHART_SEGMENTS,
  buildJobStatus24hChartSegments,
} from '../../src/dashboard/jobStatus24hChart.js'

vi.mock('vue-chartjs', () => ({
  Doughnut: defineComponent({
    props: ['options'],
    setup: props => () => h('canvas', {
      'data-center': JSON.stringify(props.options.plugins.centerText.lines),
      onClick: () => props.options.onClick(null, [{ index: 0 }]),
    }),
  }),
}))

describe('job status chart coverage', () => {
  it('keeps the 24-hour filter when drilling into a chart segment', async () => {
    const router = createRouter({
      history: createMemoryHistory(),
      routes: [{ path: '/jobs', name: 'jobs', component: { template: '<div />' } }],
    })
    await router.push('/jobs')
    await router.isReady()
    const app = createApp(JobStatus24hChartWidget)
    app.use(router)
    app.use(createI18n({ legacy: false, locale: 'en', messages: { en: {} }, missingWarn: false, fallbackWarn: false }))
    app.provide(DASHBOARD_CONTEXT_KEY, {
      aggregate: ref({ jobsPast24hStatusCounts: { T: 1 } }),
      loading: ref(false),
    })
    const host = document.createElement('div')
    app.mount(host)
    try {
      host.querySelector('canvas').click()
      await vi.waitFor(() => expect(router.currentRoute.value.query).toEqual({ days: '1', status: 'T' }))
    } finally {
      app.unmount()
    }
  })

  it.each([
    [{ A: 2 }, null, 'No running, waiting, successful, warning, or failed jobs'],
    [{ T: 1, A: 2 }, ['1', 'Shown statuses'], null],
  ])('does not present displayed statuses as all jobs', (counts, center, message) => {
    const app = createApp(JobStatus24hChartWidget)
    app.use(createRouter({ history: createMemoryHistory(), routes: [] }))
    app.use(createI18n({ legacy: false, locale: 'en', messages: { en: {} }, missingWarn: false, fallbackWarn: false }))
    app.provide(DASHBOARD_CONTEXT_KEY, {
      aggregate: ref({ jobsPast24hStatusCounts: counts }),
      loading: ref(false),
    })
    const host = document.createElement('div')
    app.mount(host)
    try {
      if (message) expect(host.textContent).toContain(message)
      else expect(JSON.parse(host.querySelector('canvas').dataset.center)).toEqual(center)
    } finally {
      app.unmount()
    }
  })
})

describe('JOB_STATUS_24H_CHART_SEGMENTS', () => {
  it('defines the fixed status order and colors used by the chart', () => {
    expect(JOB_STATUS_24H_CHART_SEGMENTS).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8' },
      { status: 'C', labelKey: 'Waiting', color: '#9e9e9e' },
      { status: 'T', labelKey: 'Successful', color: '#28a745' },
      { status: 'W', labelKey: 'Warning', color: '#ffc107' },
      { status: 'f,E', labelKey: 'Failed', color: '#dc3545' },
    ])
  })

})

describe('chart label contrast', () => {
  it('chooses the higher-contrast text for orange, dark, and light backgrounds', () => {
    expect(getContrastTextColor('#e06c2e')).toBe('#222')
    expect(getContrastTextColor('#2e2eb8')).toBe('#fff')
    expect(getContrastTextColor('#ffffff')).toBe('#222')
    expect(getContrastTextColor('#888888')).toBe('#000')
  })
})

describe('buildJobStatus24hChartSegments', () => {
  it('keeps the fixed status order while filtering zero-count slices', () => {
    expect(buildJobStatus24hChartSegments({
      T: 4,
      R: 2,
      f: 0,
      C: 1,
      W: 0,
    })).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8', count: 2 },
      { status: 'C', labelKey: 'Waiting', color: '#9e9e9e', count: 1 },
      { status: 'T', labelKey: 'Successful', color: '#28a745', count: 4 },
    ])
  })

  it('coerces numeric strings and ignores missing or invalid counts', () => {
    expect(buildJobStatus24hChartSegments({
      R: '3',
      C: undefined,
      T: 'not-a-number',
      W: null,
      f: 2,
    })).toEqual([
      { status: 'R', labelKey: 'Running', color: '#17a2b8', count: 3 },
      { status: 'f,E', labelKey: 'Failed', color: '#dc3545', count: 2 },
    ])
  })

  describe('JobsPast24hWidget', () => {
    it('shows and links both failed statuses', async () => {
      const router = createRouter({
        history: createMemoryHistory(),
        routes: [{ path: '/jobs', name: 'jobs', component: { template: '<div />' } }],
      })
      await router.push('/jobs')
      await router.isReady()
      const app = createApp(JobsPast24hWidget)
      app.use(router)
      app.use(createI18n({ legacy: false, locale: 'en', messages: { en: {} }, missingWarn: false, fallbackWarn: false }))
      app.provide(DASHBOARD_CONTEXT_KEY, {
        aggregate: ref({ jobsPast24hStatusCounts: { f: 2, E: 3 } }),
      })
      const host = document.createElement('div')
      app.mount(host)
      try {
        const failed = [...host.querySelectorAll('a')].find(link => link.textContent.includes('Failed'))
        expect(failed.textContent).toContain('5')
        expect(router.resolve(failed.getAttribute('href')).query.status).toBe('f,E')
        expect(router.resolve(failed.getAttribute('href')).query.days).toBe('1')
      } finally {
        app.unmount()
      }
    })
  })

  it('combines error-terminated and fatal jobs in the failed segment and link', () => {
    expect(buildJobStatus24hChartSegments({ f: 2, E: 3 })).toEqual([
      { status: 'f,E', labelKey: 'Failed', color: '#dc3545', count: 5 },
    ])
  })
})
