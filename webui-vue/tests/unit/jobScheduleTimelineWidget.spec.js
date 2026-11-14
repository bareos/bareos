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

import { afterEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, nextTick, ref } from 'vue'

const { fetchTimelineData } = vi.hoisted(() => ({
  fetchTimelineData: vi.fn(),
}))

vi.mock('../../src/composables/combinedTimeline.js', () => ({
  fetchCombinedTimelineData: fetchTimelineData,
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: () => ({ username: 'test', password: 'test' }) }),
}))
vi.mock('vue-i18n', () => ({
  useI18n: () => ({ t: key => key }),
}))
vi.mock('../../src/components/CombinedJobScheduleTimeline.vue', async () => {
  const { defineComponent, h } = await import('vue')
  return {
    default: defineComponent({
      props: {
        actualJobs: { type: Array, default: () => [] },
        scheduledRuns: { type: Array, default: () => [] },
        error: { type: String, default: '' },
      },
      emits: ['range-change', 'refresh'],
      setup(props, { emit }) {
        return () => h('div', [
          h('button', {
            'data-testid': 'range',
            onClick: () => emit('range-change', {
              jobDays: 7,
              schedulerRange: { from: -7, to: 1 },
            }),
          }),
          h('button', {
            'data-testid': 'refresh',
            onClick: () => emit('refresh'),
          }),
          h('span', { 'data-testid': 'jobs' }, props.actualJobs.map(job => job.name).join(',')),
          h('span', { 'data-testid': 'schedules' }, props.scheduledRuns.map(run => run.job).join(',')),
          h('span', { 'data-testid': 'error' }, props.error),
        ])
      },
    }),
  }
})

const { default: JobScheduleTimelineWidget } = await import(
  '../../src/dashboard/widgets/JobScheduleTimelineWidget.vue'
)
const { DASHBOARD_CONTEXT_KEY } = await import('../../src/dashboard/dashboardContext.js')

const apps = []
const hosts = []

function deferred() {
  let resolve
  const promise = new Promise((resolvePromise) => {
    resolve = resolvePromise
  })
  return { promise, resolve }
}

function mountWidget(activeDirectors) {
  const host = document.createElement('div')
  document.body.appendChild(host)
  hosts.push(host)

  const app = createApp(JobScheduleTimelineWidget)
  app.provide(DASHBOARD_CONTEXT_KEY, {
    activeDirectors,
    refreshToken: ref(0),
  })
  apps.push(app)
  app.mount(host)
  return host
}

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
  fetchTimelineData.mockReset()
})

describe('JobScheduleTimelineWidget director scope', () => {
  it('clears and ignores results from an obsolete director scope', async () => {
    const activeDirectors = ref(['director-a'])
    const initialRequest = deferred()
    const obsoleteRequest = deferred()
    const currentRequest = deferred()
    fetchTimelineData
      .mockReturnValueOnce(initialRequest.promise)
      .mockReturnValueOnce(obsoleteRequest.promise)
      .mockReturnValueOnce(currentRequest.promise)
    const host = mountWidget(activeDirectors)

    host.querySelector('[data-testid="range"]').click()
    await vi.waitFor(() => expect(fetchTimelineData).toHaveBeenCalledTimes(1))
    initialRequest.resolve({
      actualJobs: [{ name: 'Job A' }],
      scheduledRuns: [{ job: 'Schedule A' }],
      errors: [],
    })
    await vi.waitFor(() => expect(host.querySelector('[data-testid="jobs"]').textContent).toBe('Job A'))

    host.querySelector('[data-testid="refresh"]').click()
    await vi.waitFor(() => expect(fetchTimelineData).toHaveBeenCalledTimes(2))
    activeDirectors.value = ['director-b']
    await nextTick()
    expect(host.querySelector('[data-testid="jobs"]').textContent).toBe('')
    expect(host.querySelector('[data-testid="schedules"]').textContent).toBe('')

    obsoleteRequest.resolve({
      actualJobs: [{ name: 'Stale Job A' }],
      scheduledRuns: [{ job: 'Stale Schedule A' }],
      errors: [],
    })
    await vi.waitFor(() => expect(fetchTimelineData).toHaveBeenCalledTimes(3))
    expect(fetchTimelineData.mock.calls[2][1]).toEqual(['director-b'])
    expect(host.querySelector('[data-testid="jobs"]').textContent).toBe('')

    currentRequest.resolve({
      actualJobs: [{ name: 'Job B' }],
      scheduledRuns: [{ job: 'Schedule B' }],
      errors: [],
    })
    await vi.waitFor(() => expect(host.querySelector('[data-testid="jobs"]').textContent).toBe('Job B'))
    expect(host.querySelector('[data-testid="schedules"]').textContent).toBe('Schedule B')
    expect(host.querySelector('[data-testid="error"]').textContent).toBe('')
  })
})
