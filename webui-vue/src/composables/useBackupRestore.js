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

import { ref } from 'vue'
import { useDashboardStore } from '../stores/dashboards.js'
import { useSettingsStore } from '../stores/settings.js'
import { useAuthStore } from '../stores/auth.js'

// Keep filenames portable across platforms/filesystems: strip anything but
// alphanumerics, dot, dash, underscore.
function sanitizeForFilename(value) {
  return String(value ?? '').trim().replace(/[^a-zA-Z0-9._-]+/g, '_')
}

/**
 * Shared backup/restore logic for the combined dashboards+settings backup,
 * used by both the Dashboard page (where the actions originated) and the
 * Settings page (so users can find them alongside the other user settings).
 *
 * A backup bundles both the configurable dashboards and the other
 * browser-local user settings (refresh interval, theme, locale, selected
 * directors, table page sizes, restore-merge defaults) into a single file,
 * so restoring on a fresh browser/profile brings back the whole setup.
 */
export function useBackupRestore({ t, $q, onRestored } = {}) {
  const dashStore = useDashboardStore()
  const settings  = useSettingsStore()
  const auth      = useAuthStore()

  const restoreFileInput = ref(null)

  function downloadBackup() {
    const data = {
      bareosWebuiBackup: 1,
      exportedAt: new Date().toISOString(),
      dashboards: dashStore.exportDashboards(),
      settings: settings.exportSettings(),
    }
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
    const url = URL.createObjectURL(blob)
    const stamp = new Date().toISOString().replace(/[:.]/g, '-')
    const username = sanitizeForFilename(auth.user?.username)
    const namePart = username ? `-${username}` : ''
    const a = document.createElement('a')
    a.href = url
    a.download = `bareos-webui-backup${namePart}-${stamp}.json`
    document.body.appendChild(a)
    a.click()
    a.remove()
    URL.revokeObjectURL(url)
  }

  function triggerRestoreFilePicker() {
    restoreFileInput.value?.click()
  }

  function onRestoreFileSelected(event) {
    const file = event.target.files?.[0]
    // Allow re-selecting the same file again later.
    event.target.value = ''
    if (!file) return

    const reader = new FileReader()
    reader.onload = () => {
      let data
      try {
        data = JSON.parse(reader.result)
      } catch {
        $q.notify({ type: 'negative', message: t('Could not parse the selected file as JSON.') })
        return
      }

      // Support both the combined backup format and a dashboards-only backup
      // (as produced by an earlier version of this feature).
      const isCombined = data && typeof data === 'object' && data.bareosWebuiBackup === 1
      const isDashboardsOnly = data && typeof data === 'object' && data.bareosDashboardsBackup === 1
      if (!isCombined && !isDashboardsOnly) {
        $q.notify({ type: 'negative', message: t('This file does not contain a Bareos WebUI backup.') })
        return
      }

      $q.dialog({
        title:   t('Restore Dashboards'),
        message: t('Replace all current dashboards, widgets, and settings with the contents of "{name}"? This cannot be undone.', {
          name: file.name,
        }),
        ok:     { label: t('Restore'), color: 'negative', flat: true },
        cancel: { label: t('Cancel'), flat: true },
      }).onOk(() => {
        try {
          if (isCombined) {
            // Validate the settings payload's shape *before* mutating
            // anything: importDashboards() below already validates fully
            // before committing state, but if we ran it first and then
            // importSettings() threw on a malformed settings object, the
            // dashboards would already have been replaced — leaving a
            // partial restore despite the "this cannot be undone" prompt
            // implying an all-or-nothing operation.
            if (data.settings !== undefined
              && (typeof data.settings !== 'object' || data.settings === null || Array.isArray(data.settings))) {
              throw new Error(t('This file does not contain a settings backup.'))
            }
            if (data.dashboards) dashStore.importDashboards(data.dashboards)
            if (data.settings) settings.importSettings(data.settings)
          } else {
            dashStore.importDashboards(data)
          }
          onRestored?.()
          $q.notify({ type: 'positive', message: t('Dashboards restored.') })
        } catch (err) {
          $q.notify({ type: 'negative', message: err?.message || t('Could not restore dashboards from this file.') })
        }
      })
    }
    reader.onerror = () => {
      $q.notify({ type: 'negative', message: t('Could not read the selected file.') })
    }
    reader.readAsText(file)
  }

  return {
    restoreFileInput,
    downloadBackup,
    triggerRestoreFilePicker,
    onRestoreFileSelected,
  }
}
