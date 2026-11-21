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
import { createApp, defineComponent, h } from 'vue'
import VolumeStatusBar from '../../src/components/VolumeStatusBar.vue'
import VolumeStatusBadge from '../../src/components/VolumeStatusBadge.vue'

const apps = []
const hosts = []

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
})

describe('VolumeStatusBar filters', () => {
  it('uses dark text on a Full volume badge', () => {
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)
    const app = createApp(VolumeStatusBadge, { status: 'Full' })
    app.component('q-badge', defineComponent({
      props: { color: String, textColor: String, label: String },
      setup: props => () => h('span', {
        'data-color': props.color,
        'data-text-color': props.textColor,
      }, props.label),
    }))
    apps.push(app)
    app.mount(host)

    expect(host.querySelector('span').dataset).toMatchObject({ color: 'warning', textColor: 'black' })
  })

  it('uses contrasting foregrounds for light and dark status segments', () => {
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)
    const app = createApp(VolumeStatusBar, {
      volumes: [{ volstatus: 'Full' }, { volstatus: 'Archive' }],
    })
    app.component('q-tooltip', { template: '<span><slot /></span>' })
    apps.push(app)
    app.mount(host)

    expect(host.querySelector('[data-testid="volume-status-bar-Full"]').style.color).toBe('rgb(0, 0, 0)')
    expect(host.querySelector('[data-testid="volume-status-bar-Archive"]').style.color).toBe('rgb(255, 255, 255)')
  })

  it('exposes focusable status buttons with their selection state', () => {
    const select = vi.fn()
    const host = document.createElement('div')
    document.body.appendChild(host)
    hosts.push(host)
    const app = createApp(VolumeStatusBar, {
      volumes: [{ volstatus: 'Full' }, { volstatus: 'Append' }],
      active: ['Full'],
      onSelect: select,
    })
    app.component('q-tooltip', { template: '<span><slot /></span>' })
    apps.push(app)
    app.mount(host)

    const buttons = [...host.querySelectorAll('button')]
    expect(buttons).toHaveLength(4)
    const fullButtons = buttons.filter(button => button.getAttribute('aria-label')?.includes('Full'))
    expect(fullButtons).toHaveLength(2)
    for (const button of fullButtons) {
      expect(button.getAttribute('type')).toBe('button')
      expect(button.getAttribute('aria-pressed')).toBe('true')
      button.focus()
      expect(document.activeElement).toBe(button)
      button.click()
    }
    expect(select.mock.calls).toEqual([['Full'], ['Full']])
    expect(buttons.filter(button => button.getAttribute('aria-pressed') === 'false')).toHaveLength(2)
  })
})
