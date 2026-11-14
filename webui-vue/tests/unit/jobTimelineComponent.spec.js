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

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, nextTick, ref } from 'vue'

const { createDirectorCommandClient, directorState } = vi.hoisted(() => ({
  createDirectorCommandClient: vi.fn(),
  directorState: { isConnected: true },
}))

vi.mock('../../src/composables/directorAggregate.js', () => ({
  createDirectorCommandClient: (...args) => createDirectorCommandClient(...args),
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: () => ({ username: 'admin', password: 'secret' }) }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => directorState,
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({
    jobsTimelineViewMode: 'week',
    locale: 'en-EN',
    setJobsTimelineViewMode: vi.fn(),
  }),
}))
vi.mock('vue-i18n', () => ({
  useI18n: () => ({
    t: (key, values) => values
      ? key.replace(/\{(\w+)\}/g, (_, name) => values[name])
      : key,
  }),
}))
vi.mock('vue-router', () => ({
  useRouter: () => ({ push: vi.fn() }),
}))

const { default: JobTimeline } = await import(
  '../../src/components/JobTimeline.vue'
)

function deferred() {
  let resolve
  const promise = new Promise((resolvePromise) => {
    resolve = resolvePromise
  })
  return { promise, resolve }
}

const requests = []
const apps = []
const hosts = []

beforeEach(() => {
  requests.length = 0
  directorState.isConnected = true
  createDirectorCommandClient.mockReset()
  createDirectorCommandClient.mockImplementation(() => {
    const jobs = deferred()
    const client = {
      call: vi.fn(command => command.startsWith('llist jobs')
        ? jobs.promise
        : Promise.resolve({ running: [] })),
      disconnect: vi.fn(),
    }
    requests.push({ jobs, client })
    return Promise.resolve(client)
  })
})

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
})

function completeRequest(request, name, clientName) {
  const end = new Date(Date.now() - 60_000)
  const start = new Date(Date.now() - 120_000)
  const timestamp = date => date.toISOString().slice(0, 19).replace('T', ' ')
  request.jobs.resolve({
    jobs: [{
      jobid: name === 'NewJob' ? '2' : '1',
      name,
      client: clientName,
      fileset: 'Daily',
      jobstatus: 'T',
      starttime: timestamp(start),
      endtime: timestamp(end),
    }],
  })
}

describe('JobTimeline refreshes', () => {
  it('ignores a slower response from an obsolete client filter', async () => {
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)
    const clientFilter = ref('')
    const Root = defineComponent({
      setup: () => () => h(JobTimeline, {
        directors: ['director-a'],
        clientFilter: clientFilter.value,
      }),
    })
    const stub = defineComponent({
      inheritAttrs: false,
      setup(_, { attrs, slots }) {
        return () => h('div', attrs, slots.default?.())
      },
    })
    const app = createApp(Root)
    for (const name of [
      'q-card', 'q-card-section', 'q-space', 'q-btn', 'q-btn-toggle',
      'q-banner', 'q-checkbox', 'q-separator', 'q-inner-loading', 'q-tooltip',
    ]) {
      app.component(name, stub)
    }
    app.component('router-link', defineComponent({
      setup(_, { attrs, slots }) {
        return () => h('a', attrs, slots.default?.())
      },
    }))
    apps.push(app)
    app.mount(host)

    await vi.waitFor(() => expect(requests).toHaveLength(1))
    clientFilter.value = 'client-b'
    await nextTick()
    await vi.waitFor(() => expect(requests).toHaveLength(2))

    completeRequest(requests[1], 'NewJob', 'client-b')
    await vi.waitFor(() => expect(requests[1].client.disconnect).toHaveBeenCalled())
    await vi.waitFor(() => expect(
      host.querySelector('[role="img"][aria-label^="NewJob"]')
    ).not.toBeNull())

    completeRequest(requests[0], 'OldJob', 'client-a')
    await nextTick()
    await new Promise(resolve => setTimeout(resolve, 0))

    expect(host.querySelector('[role="img"][aria-label^="NewJob"]')).not.toBeNull()
    expect(host.querySelector('[role="img"][aria-label^="OldJob"]')).toBeNull()
  })
})
