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
import { createApp, defineComponent, h, reactive, ref } from 'vue'

const route = reactive({ query: { storage: 'Tape', director: 'A' } })
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('vue-router', () => ({
  useRoute: () => route,
  useRouter: () => ({ replace: vi.fn().mockResolvedValue() }),
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ user: { director: 'A' }, getCredentials: () => ({ password: 'test' }) }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({
    availableDirectors: ['A', 'B'], fetchAvailableDirectors: vi.fn().mockResolvedValue(),
  }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({
    directorName: 'A',
    getTableRowsPerPage: (_key, fallback) => fallback,
    setTableRowsPerPage: vi.fn(),
    getTableFilter: (_key, fallback) => fallback,
    setTableFilter: vi.fn(),
    getTableHiddenColumns: (_key, fallback) => fallback,
    setTableHiddenColumns: vi.fn(),
  }),
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([]),
    activeDirectors: ref(['A', 'B']),
    isCommonScope: ref(true),
    syncSelectedDirectors: vi.fn(),
  }),
}))
vi.mock('../../src/composables/storagesAggregate.js', () => ({
  fetchDirectorStorages: vi.fn(),
  fetchAggregatedStorages: vi.fn().mockResolvedValue({
    storages: [
      { name: 'Tape', director: 'A', scopeKey: 'A:Tape', enabled: true },
      { name: 'Tape', director: 'B', scopeKey: 'B:Tape', enabled: true },
    ],
    directorErrors: [],
  }),
}))
vi.mock('../../src/components/StorageDetailsPanel.vue', () => ({
  default: defineComponent({
    props: { storage: Object },
    setup: props => () => h('div', { 'data-test-selected': '' }, props.storage?.director),
  }),
}))

const { default: StoragesPage } = await import('../../src/pages/StoragesPage.vue')
const { fetchAggregatedStorages } = await import('../../src/composables/storagesAggregate.js')
const defaultStorageResult = {
  storages: [
    { name: 'Tape', director: 'A', scopeKey: 'A:Tape', enabled: true },
    { name: 'Tape', director: 'B', scopeKey: 'B:Tape', enabled: true },
  ],
  directorErrors: [],
}
let app
let host
afterEach(() => {
  app?.unmount()
  host?.remove()
  fetchAggregatedStorages.mockReset().mockResolvedValue(defaultStorageResult)
})

function deferred() {
  let resolve
  let reject
  const promise = new Promise((complete, fail) => { resolve = complete; reject = fail })
  return { promise, resolve, reject }
}

describe('StoragesPage route selection', () => {
  beforeEach(() => {
    route.query = { storage: 'Tape', director: 'A' }
  })

  it('updates the selected storage when only its director changes', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A'))
    route.query.director = 'B'
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('B'))
  })

  it('exposes storage names as keyboard-accessible selection buttons', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.component('q-table', defineComponent({
      props: { rows: Array },
      setup: (props, { slots }) => () => h('div', props.rows.map(row => (
        slots['body-cell-name']?.({ row, value: row.name })
      ))),
    }))
    app.mount(host)

    await vi.waitFor(() => expect(host.querySelector('[data-testid="storages-row-name"]')).not.toBeNull())
    const storageName = host.querySelector('[data-testid="storages-row-name"]')
    expect(storageName.tagName).toBe('BUTTON')
    expect(storageName.tabIndex).toBe(0)

    storageName.click()
    await vi.waitFor(() => expect(host.querySelector('[data-testid="storages-detail-hint"]')).not.toBeNull())
  })

  it('marks the selected storage row through the table row-class function', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.component('q-table', defineComponent({
      props: { rows: Array, tableRowClassFn: Function },
      setup: props => () => h('div', props.rows.map(row => (
        h('div', { class: props.tableRowClassFn?.(row), 'data-test-row': row.director })
      ))),
    }))
    app.mount(host)

    await vi.waitFor(() => expect(host.querySelector('[data-test-row="A"]')).not.toBeNull())
    expect(host.querySelector('[data-test-row="A"]').className).toBe('storages-row-selected')
    expect(host.querySelector('[data-test-row="B"]').className).toBe('')
  })

  it('ignores refresh results from a previous director scope', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A'))

    const staleA = deferred()
    const currentB = deferred()
    fetchAggregatedStorages.mockImplementation((_credentials, directors) => (
      directors[0] === 'A' ? staleA.promise : currentB.promise
    ))
    route.query = { storage: 'Tape', director: 'A', scopeDirector: 'A' }
    await vi.waitFor(() => expect(fetchAggregatedStorages).toHaveBeenCalledWith(
      expect.anything(),
      ['A']
    ))
    route.query = { storage: 'Tape', director: 'B', scopeDirector: 'B' }
    await vi.waitFor(() => expect(fetchAggregatedStorages).toHaveBeenCalledWith(
      expect.anything(),
      ['B']
    ))

    currentB.resolve({
      storages: [{ name: 'Tape', director: 'B', scopeKey: 'B:Tape', enabled: true }],
      directorErrors: [],
    })
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('B'))
    staleA.resolve({
      storages: [{ name: 'Tape', director: 'A', scopeKey: 'A:Tape', enabled: true }],
      directorErrors: [],
    })
    await Promise.resolve()
    await Promise.resolve()

    expect(host.querySelector('[data-test-selected]')?.textContent).toBe('B')
  })

  it('removes old scope rows and details while the new scope loads or fails', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.component('q-table', defineComponent({
      props: { rows: Array },
      setup: props => () => h('div', { 'data-test-rows': '' }, props.rows.map(row => row.director).join(',')),
    }))
    app.component('q-banner', defineComponent({
      setup: (_props, { slots }) => () => h('div', slots.default?.()),
    }))
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A'))
    expect(host.querySelector('[data-test-rows]')?.textContent).toBe('A,B')

    const pending = deferred()
    fetchAggregatedStorages.mockReturnValue(pending.promise)
    route.query = { storage: 'Tape', director: 'A', scopeDirector: 'B' }
    await vi.waitFor(() => expect(fetchAggregatedStorages).toHaveBeenCalledWith(expect.anything(), ['B']))
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')).toBeNull())
    expect(host.querySelector('[data-test-rows]')?.textContent ?? '').toBe('')

    pending.reject(new Error('Director B unavailable'))
    await vi.waitFor(() => expect(host.textContent).toContain('Director B unavailable'))
    expect(host.querySelector('[data-test-selected]')).toBeNull()
    expect(host.querySelector('[data-test-rows]')?.textContent ?? '').toBe('')
  })

  it('keeps the selected storage during a refresh of the same scope', async () => {
    host = document.createElement('div')
    document.body.appendChild(host)
    app = createApp(StoragesPage)
    app.config.warnHandler = () => {}
    app.mount(host)
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A'))

    const pending = deferred()
    fetchAggregatedStorages.mockReturnValue(pending.promise)
    route.query.scopeDirector = ''
    await vi.waitFor(() => expect(fetchAggregatedStorages).toHaveBeenCalledTimes(2))
    expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A')
    pending.resolve(defaultStorageResult)
    await vi.waitFor(() => expect(host.querySelector('[data-test-selected]')?.textContent).toBe('A'))
  })
})
