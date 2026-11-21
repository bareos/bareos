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

import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'
import { beforeEach, describe, expect, it, vi } from 'vitest'
import { useBackupRestore } from '../../src/composables/useBackupRestore.js'
import { useDashboardStore } from '../../src/stores/dashboards.js'
import { useSettingsStore } from '../../src/stores/settings.js'

describe('combined dashboard and settings restore', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  async function restore(data) {
    const notify = vi.fn()
    const onRestored = vi.fn()
    const q = { notify, dialog: () => ({ onOk: callback => callback() }) }
    const file = new File([JSON.stringify(data)], 'backup.json', { type: 'application/json' })
    useBackupRestore({ t: key => key, $q: q, onRestored })
      .onRestoreFileSelected({ target: { files: [file], value: 'backup.json' } })
    await vi.waitFor(() => expect(notify).toHaveBeenCalled())
    await nextTick()
    return { notify, onRestored }
  }

  it('leaves both stores and saved dashboards unchanged when settings are malformed', async () => {
    const dashboards = useDashboardStore()
    const settings = useSettingsStore()
    const originalId = dashboards.addDashboard('Original')
    settings.refreshInterval = 30
    await nextTick()
    const before = localStorage.getItem('bareos_dashboards')
    const { notify, onRestored } = await restore({
      bareosWebuiBackup: 1,
      dashboards: {
        bareosDashboardsBackup: 1,
        dashboards: [{ id: 'imported', name: 'Imported', widgets: [] }],
      },
      settings: { refreshInterval: 45, directorName: { toString: null } },
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'negative' }))
    expect(onRestored).not.toHaveBeenCalled()
    expect(dashboards.dashboards.some(d => d.id === originalId)).toBe(true)
    expect(localStorage.getItem('bareos_dashboards')).toBe(before)
    expect(settings.refreshInterval).toBe(30)
  })

  it('leaves settings unchanged when dashboard data is malformed', async () => {
    const settings = useSettingsStore()
    settings.refreshInterval = 30
    const { notify } = await restore({
      bareosWebuiBackup: 1,
      dashboards: { bareosDashboardsBackup: 1, dashboards: [] },
      settings: { refreshInterval: 45 },
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'negative' }))
    expect(settings.refreshInterval).toBe(30)
  })

  it.each([null, false, 0, ''])('rejects a present invalid dashboards value %s', async dashboards => {
    const settings = useSettingsStore()
    settings.refreshInterval = 30
    const original = useDashboardStore().dashboards.map(d => d.id)
    const { notify, onRestored } = await restore({
      bareosWebuiBackup: 1,
      dashboards,
      settings: { refreshInterval: 45 },
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'negative' }))
    expect(onRestored).not.toHaveBeenCalled()
    expect(settings.refreshInterval).toBe(30)
    expect(useDashboardStore().dashboards.map(d => d.id)).toEqual(original)
  })

  it('rejects invalid settings before changing dashboards', async () => {
    const dashboards = useDashboardStore()
    const before = dashboards.dashboards.map(d => d.id)
    const { notify } = await restore({
      bareosWebuiBackup: 1,
      dashboards: {
        bareosDashboardsBackup: 1,
        dashboards: [{ id: 'imported', name: 'Imported', widgets: [] }],
      },
      settings: null,
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'negative' }))
    expect(dashboards.dashboards.map(d => d.id)).toEqual(before)
  })

  it('restores valid dashboards and settings together', async () => {
    const { notify, onRestored } = await restore({
      bareosWebuiBackup: 1,
      dashboards: {
        bareosDashboardsBackup: 1,
        dashboards: [{ id: 'imported', name: 'Imported', widgets: [] }],
      },
      settings: { refreshInterval: 45 },
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'positive' }))
    expect(onRestored).toHaveBeenCalledOnce()
    expect(useDashboardStore().dashboards.map(d => d.id)).toEqual(['imported'])
    expect(useSettingsStore().refreshInterval).toBe(45)
  })

  it('continues to accept dashboards-only backups without changing settings', async () => {
    const settings = useSettingsStore()
    settings.refreshInterval = 30
    const { notify } = await restore({
      bareosDashboardsBackup: 1,
      dashboards: [{ id: 'imported', name: 'Imported', widgets: [] }],
    })
    expect(notify).toHaveBeenCalledWith(expect.objectContaining({ type: 'positive' }))
    expect(useDashboardStore().dashboards.map(d => d.id)).toEqual(['imported'])
    expect(settings.refreshInterval).toBe(30)
  })
})
