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
import { createApp, defineComponent, h, ref } from 'vue'

const mocks = vi.hoisted(() => ({
  ensure: vi.fn(),
  call: vi.fn(),
  error: vi.fn(),
}))

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: {} }),
  useRouter: () => ({ replace: vi.fn(), push: vi.fn() }),
}))
vi.mock('quasar', () => ({ useQuasar: () => ({ platform: { has: { touch: false } } }) }))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ user: { director: 'B' } }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({
    availableDirectors: ['A', 'B'],
    fetchAvailableDirectors: vi.fn().mockResolvedValue(),
    call: mocks.call,
  }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({
    directorName: 'B', selectedDirectors: ['B'],
    getTableFilter: (_key, fallback) => fallback,
    setTableFilter: vi.fn(),
    getTableRowsPerPage: (_key, fallback) => fallback,
    setTableRowsPerPage: vi.fn(),
    getTableHiddenColumns: (_key, fallback) => fallback,
    setTableHiddenColumns: vi.fn(),
    getTableFlag: (_key, fallback) => fallback,
    setTableFlag: vi.fn(),
    getTablePinnedColumns: () => [],
    setTablePinnedColumns: vi.fn(),
  }),
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([]),
    activeDirectors: ref(['B']),
    isCommonScope: ref(false),
    syncSelectedDirectors: vi.fn(),
    ensureScopeDirector: mocks.ensure,
    ensureSingleScopeDirector: vi.fn(),
  }),
}))
vi.mock('../../src/composables/storagesAggregate.js', () => ({
  fetchDirectorPoolsState: vi.fn().mockResolvedValue({ pools: [], volumes: [] }),
  fetchAggregatedPoolsState: vi.fn(),
}))
vi.mock('../../src/components/PoolDefaultsCheckDialog.vue', () => ({
  default: defineComponent({
    props: { runCommand: Function },
    setup(props) {
      return () => h('button', {
        'data-testid': 'run-command',
        onClick: () => props.runCommand(
          { director: 'A', volumename: 'Volume' }, 'test command'
        ).catch(mocks.error),
      })
    },
  }),
}))

const { default: PoolsPage } = await import('../../src/pages/PoolsPage.vue')
const apps = []
const hosts = []

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
  mocks.ensure.mockReset()
  mocks.call.mockReset()
  mocks.error.mockReset()
})

describe('PoolsPage volume commands', () => {
  function mountPage() {
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)
    const app = createApp(PoolsPage)
    app.config.warnHandler = () => {}
    apps.push(app)
    app.mount(host)
    return host
  }

  it('switches to the volume director even in single-director scope', async () => {
    const host = mountPage()
    host.querySelector('[data-testid="run-command"]').click()
    await vi.waitFor(() => expect(mocks.call).toHaveBeenCalledWith('test command'))
    expect(mocks.ensure).toHaveBeenCalledWith('A')
    expect(mocks.ensure.mock.invocationCallOrder[0]).toBeLessThan(mocks.call.mock.invocationCallOrder[0])
  })

  it('does not execute a command if switching directors fails', async () => {
    mocks.ensure.mockRejectedValue(new Error('Switch failed'))
    const host = mountPage()
    host.querySelector('[data-testid="run-command"]').click()
    await vi.waitFor(() => expect(mocks.error).toHaveBeenCalledWith(
      expect.objectContaining({ message: 'Switch failed' })
    ))
    expect(mocks.call).not.toHaveBeenCalled()
  })
})
