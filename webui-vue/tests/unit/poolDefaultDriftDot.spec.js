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

import { describe, expect, it, vi } from 'vitest'
import { createApp, defineComponent, h } from 'vue'

vi.mock('vue-i18n', () => ({
  useI18n: () => ({
    t: (key, values) => {
      const translations = {
        Yes: 'Ja',
        No: 'Nein',
        unlimited: 'unbegrenzt',
        unset: 'nicht gesetzt',
      }
      return (translations[key] ?? key).replace(/\{(\w+)\}/g, (_, name) => values?.[name] ?? '')
    },
  }),
}))

const { default: PoolDefaultDriftDot } = await import(
  '../../src/components/PoolDefaultDriftDot.vue'
)

describe('PoolDefaultDriftDot', () => {
  it('localizes pool values in the accessible tooltip', () => {
    const host = document.createElement('div')
    document.body.appendChild(host)
    const app = createApp(PoolDefaultDriftDot, {
      poolName: 'Full',
      entries: [
        { field: { label: 'Recycle', format: 'boolean' }, poolValue: 1 },
        { field: { label: 'Other', format: 'boolean' }, poolValue: 0 },
        { field: { label: 'Max Jobs', format: 'countOrUnlimited' }, poolValue: 0 },
        { field: { label: 'Retention', format: 'duration' }, poolValue: 0 },
      ],
    })
    const stub = defineComponent({
      setup(_, { attrs, slots }) {
        return () => h('span', attrs, slots.default?.())
      },
    })
    app.component('q-icon', stub)
    app.component('q-tooltip', stub)
    app.mount(host)
    const anchor = host.querySelector('[role="img"]')
    const tooltip = anchor.getAttribute('aria-label')
    expect(tooltip).toContain('Recycle: Ja')
    expect(tooltip).toContain('Other: Nein')
    expect(tooltip).toContain('Max Jobs: unbegrenzt')
    expect(tooltip).toContain('Retention: nicht gesetzt')
    expect(anchor.tabIndex).toBe(0)
    anchor.focus()
    expect(document.activeElement).toBe(anchor)
    app.unmount()
    host.remove()
  })
})
