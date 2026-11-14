/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is free software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import { createApp, h, reactive, ref } from 'vue'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import RestorePage from '../../src/pages/RestorePage.vue'

const { ensureScopeDirector, directorCall, fetchAvailableDirectors } = vi.hoisted(() => ({
  ensureScopeDirector: vi.fn(),
  directorCall: vi.fn(),
  fetchAvailableDirectors: vi.fn(),
}))
const testRoute = reactive({
  path: '/restore',
  query: { client: 'client-a', director: 'prod-a' },
})

vi.mock('vue-router', () => ({
  useRoute: () => testRoute,
  useRouter: () => ({ replace: vi.fn(), push: vi.fn() }),
}))
vi.mock('vue-i18n', async importOriginal => ({
  ...await importOriginal(),
  useI18n: () => ({ t: value => value }),
}))
vi.mock('quasar', () => ({
  useQuasar: () => ({
    notify: vi.fn(),
    platform: { has: { touch: false } },
    screen: { lt: { sm: false } },
  }),
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([{ label: 'prod-a', value: 'prod-a' }]),
    selectedDirectorsModel: ref(['prod-a']),
    activeDirectors: ref(['prod-a']),
    isCommonScope: ref(false),
    scopeLabel: ref('prod-a'),
    syncSelectedDirectors: vi.fn(),
    ensureScopeDirector,
  }),
}))
vi.mock('../../src/stores/auth.js', () => ({
  useAuthStore: () => ({ getCredentials: () => ({ password: 'test' }) }),
}))
vi.mock('../../src/stores/director.js', () => ({
  useDirectorStore: () => ({
    isConnected: true,
    fetchAvailableDirectors,
    call: directorCall,
  }),
}))
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ locale: 'en', directorName: 'prod-a' }),
}))
vi.mock('../../src/components/DirectorErrorsBanner.vue', () => ({
  default: {
    props: ['errors'],
    setup: props => () => h('div', props.errors.map(error => error.message).join(' ')),
  },
}))
vi.mock('../../src/components/JobLevelBadge.vue', () => ({ default: { render: () => h('span') } }))
vi.mock('../../src/components/PluginRestoreInfoPanel.vue', () => ({ default: { render: () => h('div') } }))

const containerStub = {
  setup: (_, { slots }) => () => h('div', slots.default?.()),
}
const selectStub = {
  props: ['options', 'modelValue', 'disable'],
  emits: ['update:modelValue'],
  setup: (props, { emit }) => () => h('select', {
    value: props.modelValue ?? '',
    disabled: props.disable,
    onChange: event => emit('update:modelValue', event.target.value),
  }, (props.options ?? []).map(option => h('option', {
    value: option.value,
  }, option.label))),
}

describe('Restore source director failures', () => {
  let app
  let root

  beforeEach(() => {
    vi.clearAllMocks()
    testRoute.query = { client: 'client-a', director: 'prod-a' }
    fetchAvailableDirectors.mockResolvedValue([])
    directorCall.mockResolvedValue({ clients: [{ name: 'client-a' }] })
    ensureScopeDirector
      .mockResolvedValueOnce()
      .mockRejectedValueOnce(new Error('No session for prod-a'))

    app = createApp(RestorePage)
    app.config.warnHandler = () => {}
    for (const name of [
      'QPage', 'QCard', 'QCardSection', 'QItem', 'QItemSection',
      'QItemLabel', 'QCheckbox', 'QAvatar', 'QInput', 'QBtn', 'QSpace',
      'QIcon', 'QTooltip', 'QSpinner', 'QTable', 'QTh', 'QTr', 'QTd',
      'QBanner', 'QBadge', 'QSeparator', 'QScrollArea', 'QDialog',
      'QMarkupTable', 'QCardActions', 'QRadio',
    ]) {
      app.component(name, containerStub)
    }
    app.component('QSelect', selectStub)
    root = document.createElement('div')
    document.body.append(root)
  })

  afterEach(() => {
    app?.unmount()
    root?.remove()
  })

  it('reports a missing session and stops before loading stale restore options', async () => {
    app.mount(root)
    await vi.waitFor(() => {
      expect(root.textContent).toContain('No session for prod-a')
    })

    expect(ensureScopeDirector).toHaveBeenNthCalledWith(1, 'prod-a')
    expect(ensureScopeDirector).toHaveBeenNthCalledWith(2, 'prod-a')
    expect(directorCall).toHaveBeenCalledTimes(1)
    expect(directorCall).toHaveBeenCalledWith('list clients')
  })

  it.each(['reject', 'resolve'])(
    'preserves a newer source when the old connection promise settles: %s',
    async outcome => {
      let resolveConnection
      let rejectConnection
      const oldConnection = new Promise((resolve, reject) => {
        resolveConnection = resolve
        rejectConnection = reject
      })
      ensureScopeDirector.mockReset()
        .mockResolvedValueOnce()
        .mockImplementationOnce(() => oldConnection)
        .mockResolvedValue()
      directorCall.mockImplementation(async command => {
        if (command === 'list clients') {
          return { clients: [{ name: 'client-a' }, { name: 'client-b' }] }
        }
        if (command === '.jobs type=R') {
          return { jobs: [{ name: 'RestoreFiles' }] }
        }
        if (command.startsWith('llist backups')) {
          return { backups: [{ jobid: 2, name: 'backup-b', level: 'F', jobstatus: 'T' }] }
        }
        return {}
      })
      app.mount(root)
      await vi.waitFor(() => {
        expect(ensureScopeDirector).toHaveBeenCalledTimes(2)
      })

      const source = root.querySelector('[data-testid="restore-source-client"]')
      source.value = 'prod-a:client-b'
      source.dispatchEvent(new Event('change', { bubbles: true }))
      await vi.waitFor(() => {
        expect(root.querySelector('[data-testid="restore-job"]').value).toBe('RestoreFiles')
        expect(directorCall.mock.calls.some(([command]) => command.startsWith('llist backups')))
          .toBe(true)
      })
      const callsBefore = directorCall.mock.calls.length
      if (outcome === 'reject') {
        rejectConnection(new Error('Obsolete source connection failed'))
      } else {
        resolveConnection()
      }
      await oldConnection.catch(() => {})
      await new Promise(resolve => setTimeout(resolve, 0))

      expect(source.value).toBe('prod-a:client-b')
      expect(root.querySelector('[data-testid="restore-target-client"]').value).toBe('client-b')
      expect(root.querySelector('[data-testid="restore-job"]').value).toBe('RestoreFiles')
      expect(root.querySelector('[data-testid="restore-backup-job"]').disabled).toBe(false)
      expect(root.textContent).not.toContain('Obsolete source connection failed')
      expect(directorCall).toHaveBeenCalledTimes(callsBefore)
    },
  )
})
