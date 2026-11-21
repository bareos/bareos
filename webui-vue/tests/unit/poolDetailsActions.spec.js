/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
   License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

import { afterEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, reactive } from 'vue'

const mocks = vi.hoisted(() => ({
  call: vi.fn(),
  switchDirector: vi.fn(),
  runCommand: null,
  volume: null,
}))
const route = reactive({ params: { name: 'Pool' }, query: { director: 'A' } })
const auth = reactive({ user: { director: 'A' } })
const settings = {
  directorName: 'A',
  locale: 'en',
  relativeTime: false,
  getTableRowsPerPage: (_key, fallback) => fallback,
  setTableRowsPerPage: vi.fn(),
  getTableSort: (_key, fallback) => fallback,
  setTableSort: vi.fn(),
  getTableFlag: (_key, fallback) => fallback,
  setTableFlag: vi.fn(),
}

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({ useRoute: () => route }))
vi.mock('quasar', () => ({ useQuasar: () => ({ notify: vi.fn() }) }))
vi.mock('../../src/stores/auth.js', () => ({ useAuthStore: () => auth }))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({ call: mocks.call, isConnected: true }),
}))
vi.mock('../../src/stores/settings.js', () => ({ useSettingsStore: () => settings }))
vi.mock('../../src/composables/useDirectorSession.js', () => ({
  switchActiveDirector: mocks.switchDirector,
}))
vi.mock('../../src/components/PoolDefaultsCheckDialog.vue', () => ({
  default: defineComponent({
    props: { volumes: Array, runCommand: Function },
    setup(props) {
      return () => h('button', {
        'data-test-defaults-action': '',
        'data-volume': props.volumes[0]?.volumename,
        onClick: () => {
          mocks.runCommand = props.runCommand
          mocks.volume = props.volumes[0]
        },
      })
    },
  }),
}))

const { default: PoolDetailsPage } = await import('../../src/pages/PoolDetailsPage.vue')
let app
let host

afterEach(() => {
  app?.unmount()
  host?.remove()
  mocks.call.mockReset()
  mocks.switchDirector.mockReset()
  route.params.name = 'Pool'
  route.query.director = 'A'
  auth.user.director = 'A'
  settings.directorName = 'A'
  mocks.runCommand = null
  mocks.volume = null
})

describe('PoolDetailsPage pool-default actions', () => {
  it('does not send remaining volume commands after the route changes director', async () => {
    let finishFirst
    mocks.call.mockImplementation(command => {
      if (command === 'first') return new Promise(resolve => { finishFirst = resolve })
      if (command.startsWith('list pool=')) {
        return Promise.resolve({ pools: [{ name: 'Pool', pooltype: 'Backup' }] })
      }
      if (command.startsWith('llist volumes pool=')) {
        return Promise.resolve({
          volumes: [{ volumename: 'V1', volstatus: 'Full', pool: 'Pool' }],
        })
      }
      return Promise.resolve({})
    })
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(PoolDetailsPage)
    app.config.warnHandler = () => {}
    app.mount(host)

    await vi.waitFor(() => expect(host.querySelector(
      '[data-test-defaults-action][data-volume="V1"]'
    )).not.toBeNull())
    host.querySelector('[data-test-defaults-action]').click()
    const first = mocks.runCommand(mocks.volume, 'first')
    await vi.waitFor(() => expect(mocks.call).toHaveBeenCalledWith('first'))

    route.query.director = 'B'
    auth.user.director = 'B'
    finishFirst()
    await first

    await expect(mocks.runCommand(mocks.volume, 'second')).rejects.toThrow()
    expect(mocks.call).not.toHaveBeenCalledWith('second')
  })
})
