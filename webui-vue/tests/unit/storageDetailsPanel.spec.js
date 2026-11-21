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
   along with this program. If not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, nextTick, ref } from 'vue'

const { director, auth, switchDirector } = vi.hoisted(() => ({
  director: { rawCall: vi.fn(), isConnected: true },
  auth: { user: { director: 'test-dir' } },
  switchDirector: vi.fn(),
}))

vi.mock('../../src/stores/director.js', () => ({ useDirectorStore: () => director }))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => auth,
}))
vi.mock('../../src/composables/useDirectorSession.js', () => ({
  switchActiveDirector: switchDirector,
}))
vi.mock('../../src/stores/settings.js', () => ({ useSettingsStore: () => ({}) }))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))

const { default: StorageDetailsPanel } = await import(
  '../../src/components/StorageDetailsPanel.vue'
)

const apps = []
const hosts = []

function deferred() {
  let resolve
  let reject
  const promise = new Promise((ok, fail) => { resolve = ok; reject = fail })
  return { promise, resolve, reject }
}

function mountPanel(storage) {
  const host = document.createElement('div')
  document.body.appendChild(host)
  hosts.push(host)
  const app = createApp(defineComponent({
    setup: () => () => h(StorageDetailsPanel, { storage: storage.value }),
  }))
  const stub = defineComponent({
    setup(_, { attrs, slots }) {
      return () => h('div', attrs, slots.default?.())
    },
  })
  for (const name of [
    'q-card', 'q-card-section', 'q-list', 'q-item', 'q-item-section',
    'q-item-label', 'q-space', 'q-badge',
  ]) app.component(name, stub)
  app.component('q-btn', defineComponent({
    props: { loading: Boolean },
    setup(props, { attrs }) {
      return () => h('button', { ...attrs, 'data-loading': String(props.loading) })
    },
  }))
  apps.push(app)
  app.mount(host)
  return host
}

beforeEach(() => {
  director.rawCall.mockReset()
  switchDirector.mockReset()
  director.isConnected = true
  auth.user.director = 'test-dir'
})
afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
})

describe('StorageDetailsPanel status', () => {
  it('displays configured device names', () => {
    director.rawCall.mockResolvedValue('Status')
    const host = mountPanel(ref({ name: 'A', director: 'test-dir', device: ['Drive1', 'Drive2'] }))
    expect(host.textContent).toContain('Drive1, Drive2')
  })

  it('reloads and discards stale status when the storage name changes in place', async () => {
    const oldRequest = deferred()
    director.rawCall.mockReturnValueOnce(oldRequest.promise).mockResolvedValueOnce('Current status')
    const storage = ref({ name: 'A', director: 'test-dir' })
    const host = mountPanel(storage)
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(1))
    storage.value.name = 'B'
    await vi.waitFor(() => expect(host.textContent).toContain('Current status'))
    oldRequest.resolve('Old status')
    await nextTick()
    expect(director.rawCall).toHaveBeenCalledWith('status storage="B"')
    expect(host.textContent).not.toContain('Old status')
  })

  it('reloads when the storage director changes in place', async () => {
    director.rawCall.mockResolvedValue('Status')
    switchDirector.mockImplementation(async target => {
      auth.user.director = target
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    mountPanel(storage)
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(1))
    storage.value.director = 'other-dir'
    await vi.waitFor(() => expect(switchDirector).toHaveBeenCalledWith(
      'other-dir',
      expect.any(Function)
    ))
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(2))
  })

  it('ignores a previous selection response and preserves current loading', async () => {
    const oldRequest = deferred()
    const currentRequest = deferred()
    director.rawCall.mockReturnValueOnce(oldRequest.promise).mockReturnValueOnce(currentRequest.promise)
    const storage = ref({ name: 'A', director: 'test-dir' })
    const host = mountPanel(storage)
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(1))
    storage.value = { name: 'B', director: 'test-dir' }
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(2))
    oldRequest.resolve('Old status')
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).not.toContain('Old status')
    expect(host.querySelector('[data-testid="storage-details-refresh"]').dataset.loading).toBe('true')
    currentRequest.resolve('Current status')
    await vi.waitFor(() => expect(host.textContent).toContain('Current status'))
  })

  it('ignores an earlier error after the selection has changed or cleared', async () => {
    const oldRequest = deferred()
    director.rawCall.mockReturnValueOnce(oldRequest.promise).mockResolvedValueOnce('Current status')
    const storage = ref({ name: 'A', director: 'test-dir' })
    const host = mountPanel(storage)
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalledTimes(1))
    storage.value = { name: 'B', director: 'test-dir' }
    await vi.waitFor(() => expect(host.textContent).toContain('Current status'))
    storage.value = null
    await nextTick()
    oldRequest.reject(new Error('Old failure'))
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).not.toContain('Old failure')
    expect(host.textContent).not.toContain('Current status')
  })

  it('does not request status for a selection superseded during director switching', async () => {
    const switching = deferred()
    switchDirector.mockReturnValue(switching.promise)
    director.rawCall.mockResolvedValue('Current status')
    const storage = ref({ name: 'A', director: 'other-dir' })
    const host = mountPanel(storage)
    await vi.waitFor(() => expect(switchDirector).toHaveBeenCalledWith(
      'other-dir',
      expect.any(Function)
    ))
    storage.value = { name: 'B', director: 'test-dir' }
    await vi.waitFor(() => expect(host.textContent).toContain('Current status'))
    expect(switchDirector.mock.calls[0][1]()).toBe(false)
    switching.resolve()
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(director.rawCall).toHaveBeenCalledTimes(1)
    expect(director.rawCall).toHaveBeenCalledWith('status storage="B"')
  })

  it('does not send status to a different director after a queued switch', async () => {
    const switching = deferred()
    switchDirector.mockReturnValue(switching.promise)
    const host = mountPanel(ref({ name: 'Tape', director: 'storage-dir' }))
    await vi.waitFor(() => expect(switchDirector).toHaveBeenCalledWith(
      'storage-dir',
      expect.any(Function)
    ))
    switching.promise.then(() => {
      auth.user.director = 'queued-dir'
    })

    switching.resolve()
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(director.rawCall).not.toHaveBeenCalled()
    expect(host.textContent).toContain(
      'The storage Director changed before the status request was sent.'
    )
  })
})
