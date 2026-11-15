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

vi.mock('vue-i18n', () => ({
  useI18n: () => ({
    t: (key, values) => key.replace(/\{(\w+)\}/g, (_, name) => values?.[name] ?? ''),
  }),
}))

const { default: PoolDefaultsCheckDialog } = await import(
  '../../src/components/PoolDefaultsCheckDialog.vue'
)

const apps = []
const hosts = []

function renderDialog(volumes, pools) {
  const host = document.createElement('div')
  document.body.appendChild(host)
  hosts.push(host)
  const app = createApp(PoolDefaultsCheckDialog, {
    modelValue: true, volumes, pools, runCommand: vi.fn(),
  })
  const stub = defineComponent({
    setup(_, { attrs, slots }) {
      return () => h('div', attrs, slots.default?.())
    },
  })
  for (const name of [
    'q-dialog', 'q-card', 'q-card-section', 'q-icon', 'q-space', 'q-btn',
    'q-banner', 'q-card-actions', 'q-td', 'q-linear-progress', 'q-list',
    'q-item', 'q-item-section', 'q-item-label',
  ]) app.component(name, stub)
  app.component('q-table', defineComponent({
    props: { rows: Array },
    setup(props) {
      return () => h('div', props.rows.map(row => h('span', row.volume.volumename)))
    },
  }))
  apps.push(app)
  app.mount(host)
  return host
}

afterEach(() => {
  apps.splice(0).forEach(app => app.unmount())
  hosts.splice(0).forEach(host => host.remove())
})

describe('PoolDefaultsCheckDialog', () => {
  const pool = { director: 'dir-a', name: 'Full', recycle: 1 }
  const matching = { director: 'dir-a', pool: 'Full', volumename: 'Good', recycle: 1 }
  const missing = { director: 'dir-b', pool: 'Full', volumename: 'Unknown', recycle: 1 }
  const different = { director: 'dir-a', pool: 'Full', volumename: 'Changed', recycle: 0 }

  it('does not claim success when a volume has no matching pool', () => {
    const host = renderDialog([matching, missing], [pool])
    expect(host.querySelector('[data-testid="pool-defaults-clean"]')).toBeNull()
    expect(host.textContent).toContain('Volumes without a matching pool: 1')
  })

  it('counts only compared volumes while preserving detected drift', () => {
    const host = renderDialog([matching, missing, different], [pool])
    expect(host.textContent).toContain('1 of 2 volumes deviate from their pool.')
    expect(host.textContent).toContain('Volumes without a matching pool: 1')
    expect(host.textContent).toContain('Changed')
  })

  it('reports all matching volumes only when every pool is present', () => {
    const host = renderDialog([matching], [pool])
    expect(host.querySelector('[data-testid="pool-defaults-clean"]')?.textContent)
      .toContain('All 1 volumes match')
    expect(host.textContent).not.toContain('Comparison is incomplete')
  })
})
