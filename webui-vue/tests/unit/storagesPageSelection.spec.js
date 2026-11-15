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
let app
let host
afterEach(() => {
  app?.unmount()
  host?.remove()
})

describe('StoragesPage route selection', () => {
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
})
