/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { expect, it, vi } from 'vitest'
import { createApp, h, ref } from 'vue'
import RestorePage from '../../src/pages/RestorePage.vue'

const call = vi.fn()
vi.mock('vue-router', () => ({
  useRoute: () => ({ query: { client: 'client-fd', director: 'A', jobid: '7' } }),
  useRouter: () => ({ replace: vi.fn().mockResolvedValue() }),
}))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key, locale: ref('en') }) }))
const quasar = { screen: { lt: {} }, notify: vi.fn() }
vi.mock('quasar', () => ({ useQuasar: () => quasar }))
vi.mock('../../src/stores/auth.js', () => ({ useAuthStore: () => ({}) }))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({
    call, isConnected: true, fetchAvailableDirectors: vi.fn().mockResolvedValue(),
  }),
}))
vi.mock('../../src/stores/pluginHints.js', () => ({
  usePluginHintsStore: () => ({ hints: [], ensureLoaded: vi.fn().mockResolvedValue() }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ directorName: 'A' }),
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([]),
    activeDirectors: ref(['A']),
    isCommonScope: ref(false),
    syncSelectedDirectors: vi.fn(),
    ensureScopeDirector: vi.fn().mockResolvedValue(),
  }),
}))

it('retries the failed child listing without rebuilding the BVFS cache', async () => {
  let childAttempts = 0
  call.mockImplementation(async command => {
    if (command === 'list clients') return { clients: [{ name: 'client-fd' }] }
    if (command.startsWith('.bvfs_lsdirs')) {
      if (command.includes('pathid=42')) {
        if (++childAttempts === 1) throw new Error('Temporary listing failure')
        return { directories: [] }
      }
      return { directories: [{ name: 'example', pathid: 42 }] }
    }
    if (command.startsWith('.bvfs_lsfiles')) {
      return { files: command.includes('pathid=42') ? [{ name: 'restored.txt', fileid: 17 }] : [] }
    }
    return {}
  })

  const host = document.createElement('div')
  const app = createApp(RestorePage)
  app.config.warnHandler = () => {}
  app.config.globalProperties.$q = quasar
  app.component('q-btn', {
    props: ['label'],
    setup: props => () => h('button', props.label),
  })
  app.component('q-dialog', () => null)
  app.component('q-table', {
    props: { rows: { type: Array, default: () => [] } },
    setup: (props, { slots }) => () => h('div', props.rows.map(row => slots.body?.({ row }))),
  })
  const button = label => [...host.querySelectorAll('button')].find(el => el.textContent === label)
  app.mount(host)
  try {
    const directory = () => host.querySelector('[data-testid="restore-browser"] .cursor-pointer')
    await vi.waitFor(() => expect(directory()?.textContent).toBe('example/'))
    directory().click()
    await vi.waitFor(() => expect(button('Retry')).toBeDefined())
    button('Retry').click()
    await vi.waitFor(() => {
      expect(host.querySelector('[data-testid="restore-browser"]')?.textContent ?? '')
        .toContain('restored.txt')
    })
    expect(button('example/')).toBeDefined()
    expect(childAttempts).toBe(2)
    expect(call.mock.calls.filter(([command]) => command.startsWith('.bvfs_update'))).toHaveLength(1)
  } finally {
    app.unmount()
  }
})
