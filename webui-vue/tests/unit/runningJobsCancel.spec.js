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
import { createPinia, setActivePinia } from 'pinia'
import { describe, expect, it, vi } from 'vitest'
import { DASHBOARD_CONTEXT_KEY } from '../../src/dashboard/dashboardContext.js'
import { useAuthStore } from '../../src/stores/auth.js'
import { useDirectorStore } from '../../src/stores/director.js'
import { switchActiveDirector } from '../../src/composables/useDirectorSession.js'

const { notify } = vi.hoisted(() => ({ notify: vi.fn() }))
vi.mock('vue-i18n', async (importOriginal) => ({
  ...(await importOriginal()),
  useI18n: () => ({ t: key => key }),
}))
vi.mock('vue-router', () => ({ useRouter: () => ({ push: vi.fn() }) }))
vi.mock('quasar', () => ({
  useQuasar: () => ({
    notify,
    dialog: () => ({ onOk: callback => callback() }),
  }),
}))
vi.mock('../../src/dashboard/widgets/RunningJobRow.vue', () => ({
  default: defineComponent({
    props: ['job'],
    emits: ['cancel'],
    setup: (props, { emit }) => () => h('button', { onClick: () => emit('cancel', props.job) }, 'cancel'),
  }),
}))
const { default: RunningJobsWidget } = await import('../../src/dashboard/widgets/RunningJobsWidget.vue')

class FakeWebSocket {
  static instances = []
  static OPEN = 1
  constructor() {
    this.readyState = 0
    this.sent = []
    FakeWebSocket.instances.push(this)
  }
  send(payload) { this.sent.push(JSON.parse(payload)) }
  open() { this.readyState = 1; this.onopen?.() }
  close() { this.readyState = 3; this.onclose?.() }
}

describe('running-job cancellation', () => {
  it('survives a shared session switch while the cancel command is pending', async () => {
    notify.mockReset()
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    const pinia = createPinia()
    setActivePinia(pinia)
    const auth = useAuthStore()
    auth.login('admin', 'a', 'secret')
    auth.loginDirector('admin', 'b', 'secret', { setCurrent: false })
    const director = useDirectorStore()
    let rejectShared
    director.call = vi.fn(() => new Promise((_, reject) => { rejectShared = reject }))
    director.disconnect = vi.fn(() => rejectShared?.(new Error('Disconnected')))
    director.connectAndWait = vi.fn().mockResolvedValue(true)
    const app = createApp(RunningJobsWidget)
    app.use(pinia)
    app.provide(DASHBOARD_CONTEXT_KEY, {
      aggregate: ref({ runningJobs: [{ id: 17, name: 'job', director: 'a', scopeKey: 'a:17', status: 'R' }] }),
      activeDirectors: ref(['a']),
      refresh: vi.fn(),
    })
    app.component('q-virtual-scroll', defineComponent({
      props: ['items'],
      setup: (props, { slots }) => () => h('div', props.items.map(item => slots.default?.({ item }))),
    }))
    for (const name of ['q-list', 'q-item', 'q-item-section', 'q-item-label', 'q-btn', 'q-icon', 'q-badge']) {
      app.component(name, defineComponent({ setup: (_, { slots }) => () => h('div', slots.default?.()) }))
    }
    const host = document.createElement('div')
    app.mount(host)
    try {
      host.querySelector('button').click()
      await vi.waitFor(() => expect(
        FakeWebSocket.instances.length || director.call.mock.calls.length
      ).toBeGreaterThan(0))
      const socket = FakeWebSocket.instances[0]
      if (socket) {
        socket.open()
        socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
        await vi.waitFor(() => expect(socket.sent.some(item => item.type === 'command')).toBe(true))
      }
      const request = socket?.sent.find(item => item.type === 'command')
      await switchActiveDirector('b')
      if (socket) {
        socket.onmessage?.({ data: JSON.stringify({ type: 'response', id: request.id, data: {} }) })
      }
      await vi.waitFor(() => expect(notify).toHaveBeenCalledWith(
        expect.objectContaining({ type: 'positive' })
      ))
      expect(notify).not.toHaveBeenCalledWith(expect.objectContaining({ type: 'negative' }))
    } finally {
      app.unmount()
      vi.unstubAllGlobals()
    }
  })

  it('uses isolated commands for every job in a cancelled group', async () => {
    notify.mockReset()
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    const pinia = createPinia()
    setActivePinia(pinia)
    const auth = useAuthStore()
    auth.login('admin', 'a', 'secret')
    auth.loginDirector('admin', 'b', 'secret', { setCurrent: false })
    const director = useDirectorStore()
    director.disconnect = vi.fn()
    director.connectAndWait = vi.fn().mockResolvedValue(true)
    const app = createApp(RunningJobsWidget)
    app.use(pinia)
    app.provide(DASHBOARD_CONTEXT_KEY, {
      aggregate: ref({ runningJobs: [1, 2].map(id => ({
        id, name: 'job', director: 'a', scopeKey: `a:${id}`, runtimeStatus: 'is waiting execution',
      })) }),
      activeDirectors: ref(['a']),
      refresh: vi.fn(),
    })
    app.component('q-virtual-scroll', defineComponent({
      props: ['items'],
      setup: (props, { slots }) => () => h('div', props.items.map(item => slots.default?.({ item }))),
    }))
    app.component('q-btn', defineComponent({
      setup: (_, { attrs }) => () => h('button', attrs),
    }))
    for (const name of ['q-list', 'q-item', 'q-item-section', 'q-item-label', 'q-icon', 'q-badge']) {
      app.component(name, defineComponent({ setup: (_, { slots, attrs }) => () => h('div', attrs, slots.default?.()) }))
    }
    const host = document.createElement('div')
    app.mount(host)
    try {
      host.querySelector('[title="Switch to grouped queued view"]').click()
      await vi.waitFor(() => expect(host.querySelector('[title="Cancel all in group"]')).not.toBeNull())
      host.querySelector('[title="Cancel all in group"]').click()
      for (let index = 0; index < 2; index += 1) {
        await vi.waitFor(() => expect(FakeWebSocket.instances.length).toBe(index + 1))
        const socket = FakeWebSocket.instances[index]
        socket.open()
        socket.onmessage?.({ data: JSON.stringify({ type: 'auth_ok' }) })
        await vi.waitFor(() => expect(socket.sent.some(item => item.type === 'command')).toBe(true))
        const request = socket.sent.find(item => item.type === 'command')
        if (index === 0) await switchActiveDirector('b')
        socket.onmessage?.({ data: JSON.stringify({ type: 'response', id: request.id, data: {} }) })
      }
      await vi.waitFor(() => expect(notify).toHaveBeenCalledWith(
        expect.objectContaining({ type: 'positive' })
      ))
      expect(auth.getCredentials().director).toBe('b')
      expect(director.disconnect).toHaveBeenCalledTimes(1)
    } finally {
      app.unmount()
      vi.unstubAllGlobals()
    }
  })
})
