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
   along with this program. If not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
   USA.
 */

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h, nextTick, ref } from 'vue'

const { director, dialog, auth, switchDirector } = vi.hoisted(() => ({
  director: { call: vi.fn(), rawCall: vi.fn(), status: 'connected', isConnected: true },
  dialog: vi.fn(),
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
vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({
    refreshInterval: 3600,
    getTableFilter: (_key, fallback) => fallback,
    setTableFilter: vi.fn(),
    getTableHiddenColumns: (_key, fallback) => fallback,
    setTableHiddenColumns: vi.fn(),
  }),
}))
vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('quasar', () => ({ useQuasar: () => ({ notify: vi.fn(), dialog }) }))

const { default: AutochangerPanel } = await import(
  '../../src/components/AutochangerPanel.vue'
)

const apps = []
const hosts = []
let interval

function deferred() {
  let resolve
  const promise = new Promise(done => { resolve = done })
  return { promise, resolve }
}

function mountPanel(storage) {
  const host = document.createElement('div')
  document.body.appendChild(host)
  hosts.push(host)
  const app = createApp(defineComponent({
    setup: () => () => h(AutochangerPanel, { storage: storage.value }),
  }))
  const stub = defineComponent({
    inheritAttrs: false,
    setup(_, { slots }) {
      return () => h('div', slots.default?.())
    },
  })
  for (const name of [
    'q-space', 'q-banner', 'q-card', 'q-card-section', 'q-input',
    'q-icon', 'q-tooltip', 'q-badge', 'q-select', 'q-checkbox',
    'q-separator', 'q-inner-loading', 'q-card-actions', 'q-menu',
    'q-list', 'q-item', 'q-item-section', 'q-item-label', 'q-spinner',
    'q-skeleton', 'router-link',
  ]) app.component(name, stub)
  app.component('q-td', defineComponent({
    inheritAttrs: false,
    setup(_, { attrs, slots }) {
      return () => h('div', attrs, slots.default?.())
    },
  }))
  app.component('q-scroll-area', defineComponent({
    setup(_, { slots, expose }) {
      expose({ setScrollPosition: vi.fn() })
      return () => h('div', slots.default?.())
    },
  }))
  app.component('q-btn', defineComponent({
    inheritAttrs: false,
    props: { label: String, disable: Boolean },
    setup(props, { attrs, slots }) {
      return () => h('button', { ...attrs, disabled: props.disable }, [
        props.label, slots.default?.(),
      ])
    },
  }))
  app.component('q-dialog', defineComponent({
    props: { modelValue: Boolean },
    setup(props, { slots }) {
      return () => props.modelValue
        ? h('div', { 'data-test-dialog': '' }, slots.default?.())
        : null
    },
  }))
  app.directive('close-popup', {})
  app.component('q-table', defineComponent({
    props: { rows: Array },
    setup(props, { slots }) {
      return () => h('div', props.rows.map(row => h('div', {
        'data-test-row': `${row.type}-${row.slotnr}`,
      }, [
        slots['body-cell-drag']?.({ row, value: row.slotnr }),
        h('div', {
          'data-test-drop-target': row.type === 'drive' ? row.slotnr : undefined,
        }, slots['body-cell-content']?.({ row, value: row.content })),
        slots['body-cell-mr_volname']?.({ row, value: row.mr_volname }),
        slots['body-cell-pr_name']?.({ row, value: row.pr_name }),
        slots['body-cell-actions']?.({ row }),
      ])))
    },
  }))
  apps.push(app)
  app.mount(host)
  return { app, host }
}

beforeEach(() => {
  director.call.mockReset()
  director.rawCall.mockReset()
  dialog.mockReset()
  switchDirector.mockReset()
  auth.user.director = 'test-dir'
})

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
  if (interval) {
    for (const result of interval.mock.results) clearInterval(result.value)
    interval.mockRestore()
    interval = undefined
  }
})

describe('AutochangerPanel storage selection', () => {
  it('does not update slots after storage changes during a director switch', async () => {
    const switching = deferred()
    director.call.mockResolvedValue({ contents: [], pools: [], volumes: [] })
    switchDirector.mockReturnValue(switching.promise)
    const storage = ref({ name: 'Tape', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(director.call).toHaveBeenCalled())
    auth.user.director = 'other-dir'
    host.querySelector('[data-testid="autochanger-update-slots"]').click()
    await vi.waitFor(() => expect(switchDirector).toHaveBeenCalledWith('test-dir'))
    storage.value = { name: 'Tape', director: 'other-dir' }
    await nextTick()
    switching.resolve()
    await vi.waitFor(() => expect(host.textContent).toContain(
      'The volume action was cancelled because the selection changed.'
    ))
    expect(director.rawCall).not.toHaveBeenCalled()
  })

  it('rejects an export confirmation after switching away and back', async () => {
    let confirm
    dialog.mockReturnValue({ onOk: callback => { confirm = callback } })
    director.call.mockImplementation(command => {
      if (command.startsWith('status storage=')) {
        return Promise.resolve({
          contents: [{ type: 'slot', slotnr: 3, content: 'full', mr_volname: 'VolumeA' }],
        })
      }
      return Promise.resolve({ pools: [], volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(host.querySelector('[title="Export"]')).not.toBeNull())
    host.querySelector('[title="Export"]').click()
    expect(confirm).toBeTypeOf('function')

    storage.value = { name: 'B', director: 'test-dir' }
    await nextTick()
    storage.value = { name: 'A', director: 'test-dir' }
    await nextTick()
    await confirm()
    expect(director.rawCall).not.toHaveBeenCalled()
  })

  it.each([false, true])('exports only while the confirmed storage remains selected (change: %s)', async (change) => {
    let confirm
    dialog.mockReturnValue({ onOk: callback => { confirm = callback } })
    director.call.mockImplementation(command => {
      if (command.startsWith('status storage=')) {
        return Promise.resolve({
          contents: [{ type: 'slot', slotnr: 3, content: 'full', mr_volname: 'VolumeA' }],
        })
      }
      return Promise.resolve({ pools: [], volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(host.querySelector('[title="Export"]')).not.toBeNull())
    host.querySelector('[title="Export"]').click()
    const pending = confirm()
    if (change) storage.value = { name: 'B', director: 'test-dir' }
    await pending
    if (change) {
      expect(director.rawCall).not.toHaveBeenCalled()
    } else {
      expect(director.rawCall).toHaveBeenCalledWith(
        'export storage="A" srcslots=3',
        expect.any(Object)
      )
    }
  })

  it('keeps an in-flight command log and refresh scoped to its original storage', async () => {
    const rawCommand = deferred()
    let commandCallbacks
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command.startsWith('status storage=')) return Promise.resolve({ contents: [] })
      return Promise.resolve({})
    })
    director.rawCall.mockImplementation((_command, callbacks) => {
      commandCallbacks = callbacks
      return rawCommand.promise
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(host.querySelector('[aria-label="Import all"]')).not.toBeNull())

    host.querySelector('[aria-label="Import all"]').click()
    await vi.waitFor(() => expect(director.rawCall).toHaveBeenCalled())

    storage.value = { name: 'B', director: 'test-dir' }
    await vi.waitFor(() => expect(director.call).toHaveBeenCalledWith(
      'status storage="B" slots'
    ))
    const bRefreshCount = director.call.mock.calls.filter(
      ([command]) => command === 'status storage="B" slots'
    ).length

    commandCallbacks.onChunk('Output from storage A')
    await nextTick()
    expect(host.textContent).not.toContain('Output from storage A')

    rawCommand.resolve({})
    await vi.waitFor(() => expect(host.querySelector('[aria-label="Import all"]').disabled).toBe(false))
    expect(director.call.mock.calls.filter(
      ([command]) => command === 'status storage="B" slots'
    )).toHaveLength(bRefreshCount)
  })

  it('rejects a drive drop started from another storage', async () => {
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command === 'status storage="A" slots') {
        return Promise.resolve({
          contents: [
            { type: 'slot', slotnr: 3, content: 'full', mr_volname: 'VolumeA' },
            { type: 'drive', slotnr: 0, content: 'empty' },
          ],
        })
      }
      if (command === 'status storage="B" slots') {
        return Promise.resolve({
          contents: [{ type: 'drive', slotnr: 0, content: 'empty' }],
        })
      }
      return Promise.resolve({})
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(host.querySelector('[draggable="true"]')).not.toBeNull())

    const dragEvent = new Event('dragstart', { bubbles: true })
    Object.defineProperty(dragEvent, 'dataTransfer', {
      value: { effectAllowed: '', setData: vi.fn() },
    })
    const dragSource = host.querySelector('[draggable="true"]')
    dragSource.dispatchEvent(dragEvent)
    expect(dragEvent.dataTransfer.setData).toHaveBeenCalledWith('text/plain', '3')
    storage.value = { name: 'B', director: 'test-dir' }
    let dropTarget
    await vi.waitFor(() => {
      dropTarget = host.querySelector('[data-test-drop-target="0"] > div')
      expect(dropTarget).not.toBeNull()
    })

    const dropEvent = new Event('drop', { bubbles: true, cancelable: true })
    Object.defineProperty(dropEvent, 'dataTransfer', { value: { dropEffect: '' } })
    dropTarget.dispatchEvent(dropEvent)

    await new Promise(resolve => setTimeout(resolve, 0))
    expect(dropEvent.defaultPrevented).toBe(true)
    expect(director.rawCall).not.toHaveBeenCalled()
  })

  it('ignores a previous storage slot response after changing selection', async () => {
    const oldSlots = deferred()
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command === 'status storage="A" slots') return oldSlots.promise
      if (command === 'status storage="B" slots') {
        return Promise.resolve({
          contents: [{ type: 'slot', slotnr: 1, mr_volname: 'CurrentVolume' }],
        })
      }
      return Promise.resolve({ volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(director.call).toHaveBeenCalledWith(
      'status storage="A" slots'
    ))
    storage.value = { name: 'B', director: 'test-dir' }
    await nextTick()
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentVolume'))
    oldSlots.resolve({
      contents: [{ type: 'slot', slotnr: 1, mr_volname: 'OldVolume' }],
    })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).not.toContain('OldVolume')
    expect(director.call).not.toHaveBeenCalledWith('llist volume="OldVolume"')
  })

  it('does not continue loading or restart refresh after unmount', async () => {
    const oldPools = deferred()
    director.call.mockReturnValue(oldPools.promise)
    interval = vi.spyOn(globalThis, 'setInterval')
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { app } = mountPanel(storage)
    await vi.waitFor(() => expect(director.call).toHaveBeenCalledWith('list pools'))
    app.unmount()
    apps.splice(apps.indexOf(app), 1)
    oldPools.resolve({ pools: [] })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(director.call).not.toHaveBeenCalledWith('status storage="A" slots')
    expect(interval).not.toHaveBeenCalled()
  })

  it('does not accept an earlier request after returning to the same storage', async () => {
    const oldSlots = deferred()
    let aRequests = 0
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command === 'status storage="A" slots') {
        aRequests += 1
        return aRequests === 1 ? oldSlots.promise : Promise.resolve({
          contents: [{ type: 'slot', slotnr: 1, mr_volname: 'CurrentA' }],
        })
      }
      if (command === 'status storage="B" slots') {
        return Promise.resolve({
          contents: [{ type: 'slot', slotnr: 1, mr_volname: 'CurrentB' }],
        })
      }
      return Promise.resolve({ volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(aRequests).toBe(1))
    storage.value = { name: 'B', director: 'test-dir' }
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentB'))
    storage.value = { name: 'A', director: 'test-dir' }
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentA'))
    oldSlots.resolve({
      contents: [{ type: 'slot', slotnr: 1, mr_volname: 'OldA' }],
    })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).toContain('CurrentA')
    expect(host.textContent).not.toContain('OldA')
  })

  it('reloads when the selected storage identity changes in place', async () => {
    const oldSlots = deferred()
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command === 'status storage="A" slots') return oldSlots.promise
      if (command === 'status storage="B" slots') {
        return Promise.resolve({
          contents: [{ type: 'slot', slotnr: 1, mr_volname: 'CurrentB' }],
        })
      }
      return Promise.resolve({ volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(director.call).toHaveBeenCalledWith(
      'status storage="A" slots'
    ))
    storage.value.name = 'B'
    await vi.waitFor(() => expect(host.textContent).toContain('CurrentB'))
    oldSlots.resolve({
      contents: [{ type: 'slot', slotnr: 1, mr_volname: 'OldA' }],
    })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).toContain('CurrentB')
    expect(host.textContent).not.toContain('OldA')
  })

  it('keeps current volume details when an old details request finishes', async () => {
    const oldDetails = deferred()
    let detailRequests = 0
    director.call.mockImplementation(command => {
      if (command === 'list pools') return Promise.resolve({ pools: [] })
      if (command.startsWith('status storage=')) {
        return Promise.resolve({
          contents: [{
            type: 'slot', slotnr: 1, content: 'full', mr_volname: 'SharedVolume',
          }],
        })
      }
      if (command === 'llist volume="SharedVolume"') {
        detailRequests += 1
        return detailRequests === 1
          ? oldDetails.promise
          : Promise.resolve({ volumes: [{ hasencryptionkey: true }] })
      }
      return Promise.resolve({})
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(detailRequests).toBe(1))
    storage.value = { name: 'B', director: 'test-dir' }
    await nextTick()
    await vi.waitFor(() => expect(host.textContent).toContain(
      'Encryption key stored in catalog'
    ))
    oldDetails.resolve({ volumes: [{ hasencryptionkey: false }] })
    await new Promise(resolve => setTimeout(resolve, 0))
    expect(host.textContent).toContain('Encryption key stored in catalog')
  })

  it.each([
    ['[data-testid="autochanger-label-barcodes"]', 'Label barcodes'],
    ['[title="Mount to drive"]', 'Mount Tape'],
    ['[title="Transfer to slot"]', 'Transfer from Slot'],
    ['[title="Label this volume into a pool"]', 'Label Slot'],
  ])('closes the %s dialog on a different storage', async (selector, title) => {
    director.call.mockImplementation(command => {
      if (command === 'list pools') {
        return Promise.resolve({ pools: [{ name: 'PoolA' }] })
      }
      if (command.startsWith('status storage=')) {
        return Promise.resolve({
          contents: [{
            type: 'slot', slotnr: 3, content: 'full', mr_volname: 'VolumeA',
          }],
        })
      }
      return Promise.resolve({ volumes: [] })
    })
    const storage = ref({ name: 'A', director: 'test-dir' })
    const { host } = mountPanel(storage)
    await vi.waitFor(() => expect(host.querySelector(selector)).not.toBeNull())
    host.querySelector(selector).click()
    await nextTick()
    expect(host.querySelector('[data-test-dialog]')?.textContent).toContain(title)
    storage.value = { name: 'A', director: 'test-dir' }
    await nextTick()
    expect(host.querySelector('[data-test-dialog]')?.textContent).toContain(title)
    storage.value = { name: 'B', director: 'test-dir' }
    await nextTick()
    expect(host.querySelector('[data-test-dialog]')).toBeNull()
  })
})
