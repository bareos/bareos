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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import { createApp, h } from 'vue'
import { createPinia, setActivePinia } from 'pinia'
import { createMemoryHistory, createRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { afterEach, describe, expect, it, vi } from 'vitest'
import MainLayout from '../../src/layouts/MainLayout.vue'
import RestorePage from '../../src/pages/RestorePage.vue'
import { i18n } from '../../src/i18n/index.js'
import { useAuthStore } from '../../src/stores/auth.js'
import { useDirectorStore } from '../../src/stores/director.js'
import { useReleaseInfoStore } from '../../src/stores/releaseInfo.js'
import { useSettingsStore } from '../../src/stores/settings.js'

vi.mock('quasar', () => ({
  useQuasar: () => ({
    dark: { set: vi.fn() },
    notify: vi.fn(),
    platform: { has: { touch: false } },
    screen: { lt: { sm: false, md: false } },
  }),
}))

const containerStub = {
  setup: (_, { slots }) => () => h('div', slots.default?.()),
}
const buttonStub = {
  props: ['label'],
  setup: (props, { slots }) => () => h('button', [props.label, slots.default?.()]),
}
const selectStub = {
  props: ['options', 'modelValue'],
  setup: props => () => h('select', {
    'data-selected': props.modelValue ?? '',
  }, (props.options ?? []).map(option => h('option', {
    value: option.value,
  }, option.label))),
}

describe('Restore source director login', () => {
  let app
  let root

  afterEach(() => {
    app?.unmount()
    root?.remove()
    vi.restoreAllMocks()
    sessionStorage.clear()
    localStorage.clear()
  })

  it('keeps an actionable login banner when the source session disappears', async () => {
    sessionStorage.clear()
    localStorage.clear()
    const pinia = createPinia()
    setActivePinia(pinia)
    const auth = useAuthStore()
    const director = useDirectorStore()
    const settings = useSettingsStore()
    auth.login('admin', 'prod-a', 'test')
    auth.loginDirector('admin', 'prod-b', 'test', { setCurrent: false })
    settings.directorName = 'prod-a'
    settings.setSelectedDirectors(['prod-a'])
    director.status = 'connected'
    director.availableDirectors = ['prod-a', 'prod-b']
    vi.spyOn(director, 'fetchAvailableDirectors').mockResolvedValue(director.availableDirectors)
    vi.spyOn(director, 'refreshDirectorConnections').mockResolvedValue()
    vi.spyOn(useReleaseInfoStore(), 'refresh').mockResolvedValue()
    const hasDirectorSession = vi.spyOn(auth, 'hasDirectorSession')
    const directorCall = vi.spyOn(director, 'call').mockImplementation(async command => {
      if (command === 'list clients') {
        auth.removeDirector('prod-a')
        return { clients: [{ name: 'client-a' }] }
      }
      return {}
    })

    const router = createRouter({
      history: createMemoryHistory(),
      routes: [
        { path: '/restore', name: 'restore', component: RestorePage },
        { path: '/login', name: 'login', component: containerStub },
        { path: '/:pathMatch(.*)*', component: containerStub },
      ],
    })
    await router.push('/restore?client=client-a&director=prod-a')
    app = createApp(MainLayout)
    app.config.globalProperties.$q = useQuasar()
    app.use(pinia)
    app.use(router)
    app.use(i18n)
    app.directive('ripple', {})
    app.directive('close-popup', {})
    for (const name of [
      'QLayout', 'QDrawer', 'QHeader', 'QToolbar', 'QTabs', 'QRouteTab',
      'QMenu', 'QList', 'QPageContainer', 'QFooter', 'QPage', 'QCard',
      'QCardSection', 'QItem', 'QItemSection', 'QItemLabel', 'QCheckbox',
      'QAvatar', 'QInput', 'QSpace', 'QIcon', 'QTooltip', 'QSpinner',
      'QTable', 'QTh', 'QTr', 'QTd', 'QBanner', 'QBadge', 'QChip',
      'QSeparator', 'QScrollArea', 'QDialog', 'QMarkupTable',
      'QCardActions', 'QRadio',
    ]) {
      app.component(name, containerStub)
    }
    app.component('QBtn', buttonStub)
    app.component('QSelect', selectStub)
    root = document.createElement('div')
    document.body.append(root)
    app.mount(root)

    await vi.waitFor(() => {
      expect(hasDirectorSession).toHaveBeenCalledWith('prod-a')
      expect(root.querySelectorAll('.bg-warning')).toHaveLength(1)
      expect(root.querySelector('.bg-warning').textContent).toContain('Log in to prod-a.')
    })
    expect(root.textContent).not.toContain('Please log in to director "prod-a" first.')
    expect(root.querySelector('[data-testid="restore-source-client"]').dataset.selected)
      .toBe('prod-a:client-a')
    for (const testId of ['restore-backup-job', 'restore-target-client', 'restore-job']) {
      const select = root.querySelector(`[data-testid="${testId}"]`)
      expect(select.options).toHaveLength(0)
      expect(select.dataset.selected).toBe('')
    }
    expect(directorCall.mock.calls.filter(([command]) => command !== 'status director'))
      .toEqual([['list clients']])

    const returnTo = router.currentRoute.value.fullPath
    root.querySelector('.bg-warning button[aria-label="Login: prod-a"]').click()
    await vi.waitFor(() => {
      expect(router.currentRoute.value.name).toBe('login')
    })
    expect(router.currentRoute.value.query).toEqual({
      mode: 'add',
      director: 'prod-a',
      returnTo,
    })
  })
})
