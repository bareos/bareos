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
import { createApp, defineComponent, h, ref } from 'vue'

const mocks = vi.hoisted(() => ({
  ensure: vi.fn(),
  call: vi.fn(),
  error: vi.fn(),
  fetchSingle: vi.fn(),
  fetchAggregate: vi.fn(),
  auth: { user: { director: 'B' } },
}))
const activeDirectors = ref(['B'])

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: {} }),
  useRouter: () => ({ replace: vi.fn(), push: vi.fn() }),
}))
vi.mock('quasar', () => ({ useQuasar: () => ({ platform: { has: { touch: false } } }) }))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({
    user: mocks.auth.user,
    getCredentials: () => ({ password: 'test-password' }),
  }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({
    availableDirectors: ['A', 'B'],
    isConnected: true,
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
    activeDirectors,
    isCommonScope: ref(false),
    syncSelectedDirectors: vi.fn(),
    ensureScopeDirector: mocks.ensure,
    ensureSingleScopeDirector: vi.fn(),
  }),
}))
vi.mock('../../src/composables/storagesAggregate.js', () => ({
  fetchDirectorPoolsState: mocks.fetchSingle,
  fetchAggregatedPoolsState: mocks.fetchAggregate,
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

beforeEach(() => {
  activeDirectors.value = ['B']
  mocks.auth.user.director = 'B'
  mocks.ensure.mockImplementation(async director => { mocks.auth.user.director = director })
  mocks.fetchSingle.mockResolvedValue({ pools: [], volumes: [] })
})
afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
  mocks.ensure.mockReset()
  mocks.call.mockReset()
  mocks.error.mockReset()
  mocks.fetchSingle.mockReset()
  mocks.fetchAggregate.mockReset()
})

function mountPage() {
  const host = document.createElement('div')
  document.body.appendChild(host)
  hosts.push(host)
  const app = createApp(PoolsPage)
  app.config.warnHandler = () => {}
  app.component('q-table', defineComponent({
    props: { rows: Array, loading: Boolean },
    setup(props) {
      return () => h('div', { 'data-test-table': '', 'data-loading': String(props.loading) },
        props.rows.map(row => h('span', row.name ?? row.volumename)))
    },
  }))
  apps.push(app)
  app.mount(host)
  return host
}

describe('PoolsPage volume commands', () => {
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

  it('rejects a volume command if another director connects first', async () => {
    let finishSwitch
    mocks.ensure.mockImplementation(() => new Promise(resolve => { finishSwitch = resolve }))
    const host = mountPage()
    host.querySelector('[data-testid="run-command"]').click()
    expect(mocks.ensure).toHaveBeenCalledWith('A')
    mocks.auth.user.director = 'B'
    finishSwitch()
    await vi.waitFor(() => expect(mocks.error).toHaveBeenCalled())
    expect(mocks.call).not.toHaveBeenCalled()
  })
})

describe('PoolsPage refresh', () => {
  it('does not replace the current scope with an older refresh', async () => {
    let resolveOld
    const old = new Promise(resolve => { resolveOld = resolve })
    activeDirectors.value = ['A']
    mocks.fetchSingle.mockImplementation((_call, director) => (
      director === 'A' ? old : Promise.resolve({
        pools: [{ name: 'CurrentPool', director: 'B' }],
        volumes: [],
      })
    ))
    const host = mountPage()
    await vi.waitFor(() => expect(mocks.fetchSingle).toHaveBeenCalledTimes(1))
    activeDirectors.value = ['B']
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentPool'))
    resolveOld({ pools: [{ name: 'OldPool', director: 'A' }], volumes: [] })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).toContain('CurrentPool')
    expect(host.textContent).not.toContain('OldPool')
  })

  it('keeps a new refresh loading when an old aggregate request completes', async () => {
    let resolveOld
    let resolveCurrent
    activeDirectors.value = ['A', 'B']
    mocks.fetchAggregate.mockReturnValue(new Promise(resolve => { resolveOld = resolve }))
    mocks.fetchSingle.mockReturnValue(new Promise(resolve => { resolveCurrent = resolve }))
    const host = mountPage()
    await vi.waitFor(() => expect(mocks.fetchAggregate).toHaveBeenCalledTimes(1))
    activeDirectors.value = ['B']
    await vi.waitFor(() => expect(mocks.fetchSingle).toHaveBeenCalledTimes(1))
    resolveOld({
      pools: [{ name: 'OldPool', director: 'A' }], volumes: [], directorErrors: [],
    })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).not.toContain('OldPool')
    expect(host.querySelectorAll('[data-test-table]')).toHaveLength(0)
    resolveCurrent({ pools: [{ name: 'CurrentPool', director: 'B' }], volumes: [] })
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentPool'))
  })

  it('ignores a failure from an earlier scope', async () => {
    let rejectOld
    activeDirectors.value = ['A']
    mocks.fetchSingle.mockImplementation((_call, director) => (
      director === 'A'
        ? new Promise((_, reject) => { rejectOld = reject })
        : Promise.resolve({ pools: [{ name: 'CurrentPool', director: 'B' }], volumes: [] })
    ))
    const host = mountPage()
    await vi.waitFor(() => expect(mocks.fetchSingle).toHaveBeenCalledTimes(1))
    activeDirectors.value = ['B']
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentPool'))
    rejectOld(new Error('Old scope failed'))
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).not.toContain('Old scope failed')
    expect(host.textContent).toContain('CurrentPool')
  })
})
