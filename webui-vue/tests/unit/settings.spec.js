/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

import { beforeEach, describe, expect, it } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { nextTick } from 'vue'
import { useSettingsStore } from '../../src/stores/settings.js'

describe('settings store', () => {
  beforeEach(() => {
    localStorage.clear()
    setActivePinia(createPinia())
  })

  it('persists the last username and selected director in local storage', async () => {
    const settings = useSettingsStore()

    settings.loginUsername = 'alice'
    settings.directorName = 'prod-dir'

    await nextTick()

    expect(JSON.parse(localStorage.getItem('bareos_settings'))).toEqual(
      expect.objectContaining({
        loginUsername: 'alice',
        directorName: 'prod-dir',
      })
    )
  })

  it('restores the last username and selected director from local storage', () => {
    localStorage.setItem('bareos_settings', JSON.stringify({
      loginUsername: 'alice',
      directorName: 'prod-dir',
    }))

    setActivePinia(createPinia())
    const settings = useSettingsStore()

    expect(settings.loginUsername).toBe('alice')
    expect(settings.directorName).toBe('prod-dir')
  })

  it('persists and restores the selected dashboard directors', async () => {
    const settings = useSettingsStore()

    settings.setSelectedDirectors(['prod-dir', 'prod-dir-2', 'prod-dir'])

    await nextTick()

    expect(JSON.parse(localStorage.getItem('bareos_settings'))).toEqual(
      expect.objectContaining({
        selectedDirectors: ['prod-dir', 'prod-dir-2'],
      })
    )

    localStorage.setItem('bareos_settings', JSON.stringify({
      selectedDirectors: ['alpha-dir', 'beta-dir'],
    }))

    setActivePinia(createPinia())
    const restored = useSettingsStore()
    expect(restored.selectedDirectors).toEqual(['alpha-dir', 'beta-dir'])
  })

  it('persists and restores table rows per page settings', async () => {
    const settings = useSettingsStore()

    settings.setTableRowsPerPage('jobs.list', 42)
    settings.setTableRowsPerPage('storages.pools', 18)

    await nextTick()

    expect(JSON.parse(localStorage.getItem('bareos_settings'))).toEqual(
      expect.objectContaining({
        tableRowsPerPage: {
          'jobs.list': 42,
          'storages.pools': 18,
        },
      })
    )

    localStorage.setItem('bareos_settings', JSON.stringify({
      tableRowsPerPage: {
        'jobs.list': 33,
        'storages.pools': 11,
      },
    }))

    setActivePinia(createPinia())
    const restored = useSettingsStore()
    expect(restored.getTableRowsPerPage('jobs.list', 15)).toBe(33)
    expect(restored.getTableRowsPerPage('storages.pools', 15)).toBe(11)
  })

  it('persists and restores the restore merge defaults', async () => {
    const settings = useSettingsStore()

    expect(settings.restoreMergeJobs).toBe(true)
    expect(settings.restoreMergeFilesets).toBe(true)

    settings.setRestoreMergeDefaults({ mergeJobs: true, mergeFilesets: false })

    await nextTick()

    expect(JSON.parse(localStorage.getItem('bareos_settings'))).toEqual(
      expect.objectContaining({
        restoreMergeJobs: true,
        restoreMergeFilesets: false,
      })
    )

    localStorage.setItem('bareos_settings', JSON.stringify({
      restoreMergeJobs: false,
      restoreMergeFilesets: false,
    }))

    setActivePinia(createPinia())
    const restored = useSettingsStore()
    expect(restored.restoreMergeJobs).toBe(false)
    expect(restored.restoreMergeFilesets).toBe(false)
  })

  it('never keeps the fileset default enabled without related jobs', async () => {
    localStorage.setItem('bareos_settings', JSON.stringify({
      restoreMergeJobs: false,
      restoreMergeFilesets: true,
    }))

    setActivePinia(createPinia())
    const settings = useSettingsStore()
    expect(settings.restoreMergeFilesets).toBe(false)

    settings.setRestoreMergeDefaults({ mergeJobs: false, mergeFilesets: true })
    expect(settings.restoreMergeFilesets).toBe(false)

    settings.restoreMergeJobs = true
    settings.restoreMergeFilesets = true
    settings.restoreMergeJobs = false

    await nextTick()

    expect(settings.restoreMergeFilesets).toBe(false)
  })

  it('exportSettings() excludes loginUsername', () => {
    const settings = useSettingsStore()
    settings.loginUsername = 'alice'

    const snapshot = settings.exportSettings()

    expect(snapshot).not.toHaveProperty('loginUsername')
    expect(snapshot).toEqual(expect.objectContaining({
      refreshInterval: settings.refreshInterval,
      darkMode: settings.darkMode,
      directorName: settings.directorName,
    }))
  })

  it('importSettings() applies a previously exported snapshot', () => {
    const settings = useSettingsStore()
    settings.refreshInterval = 60
    settings.darkMode = true
    settings.setSelectedDirectors(['dir-a', 'dir-b'])
    settings.setTableRowsPerPage('jobs.list', 42)
    const snapshot = settings.exportSettings()

    // Change everything, then restore from the snapshot.
    settings.refreshInterval = 5
    settings.darkMode = false
    settings.setSelectedDirectors(['other-dir'])
    settings.setTableRowsPerPage('jobs.list', 5)

    settings.importSettings(snapshot)

    expect(settings.refreshInterval).toBe(60)
    expect(settings.darkMode).toBe(true)
    expect(settings.selectedDirectors).toEqual(['dir-a', 'dir-b'])
    expect(settings.getTableRowsPerPage('jobs.list', 0)).toBe(42)
  })

  it('importSettings() leaves unspecified fields untouched and rejects non-object data', () => {
    const settings = useSettingsStore()
    settings.darkMode = true

    settings.importSettings({ refreshInterval: 45 })

    expect(settings.refreshInterval).toBe(45)
    expect(settings.darkMode).toBe(true)

    expect(() => settings.importSettings(null)).toThrow()
    expect(() => settings.importSettings(['not', 'an', 'object'])).toThrow()
  })
})
