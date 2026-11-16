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
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { createApp, defineComponent, h, nextTick } from 'vue'
import { describe, expect, it, vi } from 'vitest'

vi.mock('vue-i18n', () => ({ useI18n: () => ({ t: key => key }) }))
vi.mock('quasar', () => ({ useQuasar: () => ({ screen: { lt: { sm: false } } }) }))
const { default: WidgetPickerDialog } = await import('../../src/dashboard/WidgetPickerDialog.vue')

describe('WidgetPickerDialog', () => {
  it('restores all widgets when the clearable search emits null', async () => {
    const app = createApp(WidgetPickerDialog, { modelValue: true })
    const wrapper = defineComponent({
      setup: (_, { slots, attrs }) => () => h('div', attrs, slots.default?.()),
    })
    for (const name of ['q-dialog', 'q-card', 'q-card-section', 'q-space', 'q-btn', 'q-icon']) {
      app.component(name, wrapper)
    }
    app.component('q-input', defineComponent({
      emits: ['update:modelValue'],
      setup: (_, { emit }) => () => h('div', [
        h('button', { class: 'search', onClick: () => emit('update:modelValue', 'running') }),
        h('button', { class: 'clear', onClick: () => emit('update:modelValue', null) }),
      ]),
    }))
    const host = document.createElement('div')
    app.mount(host)
    try {
      const count = host.querySelectorAll('.widget-picker-card').length
      expect(count).toBeGreaterThan(1)
      host.querySelector('.search').click()
      await nextTick()
      expect(host.querySelectorAll('.widget-picker-card').length).toBeLessThan(count)
      host.querySelector('.clear').click()
      await nextTick()
      expect(host.querySelectorAll('.widget-picker-card').length).toBe(count)
    } finally {
      app.unmount()
    }
  })
})
