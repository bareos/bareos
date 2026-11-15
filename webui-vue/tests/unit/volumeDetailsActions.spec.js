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

import { afterEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, reactive } from 'vue'

const mocks = vi.hoisted(() => ({ call: vi.fn(), switchDirector: vi.fn() }))
const route = reactive({ params: { name: 'Tape' }, query: { director: 'A' } })
const auth = reactive({ user: { director: 'A' } })
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({
  useRoute: () => route,
  useRouter: () => ({ push: vi.fn().mockResolvedValue() }),
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => auth,
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({ call: mocks.call, isConnected: true }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({
    directorName: 'A',
    getTableRowsPerPage: (_key, fallback) => fallback,
    setTableRowsPerPage: vi.fn(),
    getTableSort: (_key, fallback) => fallback,
    setTableSort: vi.fn(),
  }),
}))
vi.mock('../../src/composables/useDirectorSession.js', () => ({
  switchActiveDirector: mocks.switchDirector,
}))
vi.mock('../../src/components/VolumeActionsMenu.vue', () => ({
  default: defineComponent({
    emits: ['select'],
    setup(_props, { emit }) {
      return () => h('button', { 'data-test-open': '', onClick: () => emit('select', 'delete') })
    },
  }),
}))
vi.mock('../../src/components/VolumeBulkDialog.vue', () => ({
  default: defineComponent({
    props: { modelValue: Boolean, volumes: Array, runCommand: Function },
    setup(props) {
      return () => h('button', {
        'data-test-action': '',
        'data-open': String(props.modelValue),
        onClick: () => { mocks.snapshot = props.volumes[0]; mocks.run = props.runCommand },
      })
    },
  }),
}))

const { default: VolumeDetailsPage } = await import('../../src/pages/VolumeDetailsPage.vue')
let app
let host

afterEach(() => {
  app?.unmount()
  host?.remove()
  mocks.call.mockReset()
  mocks.switchDirector.mockReset()
  route.query.director = 'A'
  route.params.name = 'Tape'
  auth.user.director = 'A'
})

describe('VolumeDetailsPage action identity', () => {
  it('rejects remaining commands after navigating to another director', async () => {
    let finishFirst
    mocks.call.mockImplementation(command => {
      if (command === 'first') return new Promise(resolve => { finishFirst = resolve })
      if (command === 'second') return Promise.resolve()
      if (command.startsWith('llist volume=')) {
        return Promise.resolve({ volumes: [{ volumename: 'Tape', volstatus: 'Full' }] })
      }
      if (command === 'llist pools') return Promise.resolve({ pools: [] })
      return Promise.resolve({ jobs: [], volumes: [] })
    })
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(VolumeDetailsPage)
    app.config.warnHandler = () => {}
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-open]')).not.toBeNull())
    host.querySelector('[data-test-open]').click()
    await vi.waitFor(() => expect(host.querySelector('[data-test-action]')?.dataset.open).toBe('true'))
    host.querySelector('[data-test-action]').click()
    const first = mocks.run(mocks.snapshot, 'first')
    await vi.waitFor(() => expect(mocks.call).toHaveBeenCalledWith('first'))
    route.query.director = 'B'
    finishFirst()
    await first
    await vi.waitFor(() => expect(host.querySelector('[data-test-action]')?.dataset.open).toBe('false'))
    await expect(mocks.run(mocks.snapshot, 'second')).rejects.toThrow()
    expect(mocks.call).not.toHaveBeenCalledWith('second')
    route.query.director = 'A'
    await expect(mocks.run(mocks.snapshot, 'second')).rejects.toThrow()
    expect(mocks.call).not.toHaveBeenCalledWith('second')
  })

  it('does not send a command if navigation occurs during director switching', async () => {
    let finishSwitch
    mocks.call.mockImplementation(command => {
      if (command.startsWith('llist volume=')) {
        return Promise.resolve({ volumes: [{ volumename: 'Tape', volstatus: 'Full' }] })
      }
      if (command === 'llist pools') return Promise.resolve({ pools: [] })
      return Promise.resolve({ jobs: [], volumes: [] })
    })
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(VolumeDetailsPage)
    app.config.warnHandler = () => {}
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-open]')).not.toBeNull())
    host.querySelector('[data-test-open]').click()
    await vi.waitFor(() => expect(host.querySelector('[data-test-action]')?.dataset.open).toBe('true'))
    host.querySelector('[data-test-action]').click()
    auth.user.director = 'B'
    mocks.switchDirector.mockReturnValue(new Promise(resolve => { finishSwitch = resolve }))
    const command = mocks.run(mocks.snapshot, 'dangerous command')
    await vi.waitFor(() => expect(mocks.switchDirector).toHaveBeenCalledWith('A'))
    route.query.director = 'B'
    finishSwitch()
    await expect(command).rejects.toThrow()
    expect(mocks.call).not.toHaveBeenCalledWith('dangerous command')
  })
})
