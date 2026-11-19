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

import { quoteDirectorString } from './directorStrings.js'

export const MAX_COMMENT_LENGTH = 1024

/** Comments are single-line: bconsole commands end at a newline. */
export function sanitizeComment(value) {
  return String(value ?? '')
    .replace(/[\r\n\t]+/g, ' ')
    .trim()
    .slice(0, MAX_COMMENT_LENGTH)
}

export function buildVolumeCommentCommand(volumeName, comment) {
  return `update volume=${quoteDirectorString(volumeName)} comment=${quoteDirectorString(sanitizeComment(comment))}`
}

export function buildJobCommentCommand(jobId, comment) {
  const id = Number.parseInt(String(jobId), 10)
  if (!Number.isInteger(id) || id <= 0) {
    throw new Error(`Invalid job id: ${jobId}`)
  }
  return `update jobid=${id} comment=${quoteDirectorString(sanitizeComment(comment))}`
}

/*
 * Status changes are offered as named actions with the statuses they apply
 * to, instead of a free choice of any status: the director accepts almost
 * any change, but only these are safe routine operations. Recycle, Purged
 * and Append are left to purge/prune, recycling and labelling.
 */
export const VOLUME_BULK_ACTIONS = [
  { id: 'pool', label: 'Move to pool', icon: 'drive_file_move', param: 'pool' },
  { id: 'enable', label: 'Enable', icon: 'check_circle' },
  { id: 'disable', label: 'Disable', icon: 'pause_circle' },
  { id: 'frompool', label: 'Reset from pool', icon: 'settings_backup_restore' },
  { id: 'comment', label: 'Set comment', icon: 'comment', param: 'comment' },
  { id: 'prune', label: 'Prune', icon: 'content_cut' },
  {
    id: 'status-used', label: 'Stop appending (mark Used)', icon: 'do_not_disturb_on',
    group: 'status', requiresStatus: ['Append'], volstatus: 'Used',
  },
  {
    id: 'status-readonly', label: 'Protect as read-only', icon: 'lock',
    group: 'status', requiresStatus: ['Full', 'Used'], volstatus: 'Read-Only',
  },
  {
    id: 'status-archive', label: 'Archive', icon: 'inventory_2',
    group: 'status', requiresStatus: ['Full', 'Used'], volstatus: 'Archive',
  },
  {
    id: 'status-unprotect', label: 'Remove protection (mark Used)', icon: 'lock_open',
    group: 'status', requiresStatus: ['Read-Only', 'Archive'], volstatus: 'Used',
  },
  {
    id: 'status-clearerror', label: 'Clear error (mark Used)', icon: 'healing',
    group: 'status', requiresStatus: ['Error'], volstatus: 'Used',
  },
  { id: 'purge', label: 'Purge', icon: 'delete_sweep', destructive: true },
  {
    id: 'truncate', label: 'Truncate', icon: 'layers_clear', destructive: true,
    requiresStatus: ['Purged'],
  },
  { id: 'delete', label: 'Delete from catalog', icon: 'delete_forever', destructive: true },
]

export function findVolumeBulkAction(id) {
  return VOLUME_BULK_ACTIONS.find(action => action.id === id) ?? null
}

/** Whether an action applies to a volume with the given volstatus. */
export function isVolumeActionApplicable(action, volstatus) {
  if (!action) {
    return false
  }
  return !action.requiresStatus || action.requiresStatus.includes(volstatus)
}

/**
 * Returns the bconsole command for one volume, or null when the action
 * does not apply to this volume (e.g. truncate of a non-purged volume).
 */
export function buildVolumeBulkCommand(actionId, volume, params = {}) {
  const name = quoteDirectorString(volume?.volumename ?? volume?.name ?? '')
  const action = findVolumeBulkAction(actionId)
  if (action && !isVolumeActionApplicable(action, volume?.volstatus)) {
    return null
  }
  if (action?.volstatus) {
    return `update volume=${name} volstatus=${action.volstatus}`
  }
  switch (actionId) {
    case 'pool':
      if (!params.pool) {
        throw new Error('A target pool is required.')
      }
      return `update volume=${name} pool=${quoteDirectorString(params.pool)}`
    case 'enable':
      return `update volume=${name} enabled=yes`
    case 'disable':
      return `update volume=${name} enabled=no`
    case 'frompool':
      return `update volume=${name} frompool=yes`
    case 'comment':
      return buildVolumeCommentCommand(volume?.volumename ?? volume?.name ?? '', params.comment)
    case 'prune':
      return `prune volume=${name} yes`
    case 'purge':
      return `purge volume=${name}`
    case 'truncate':
      return `truncate volstatus=Purged volume=${name} yes`
    case 'delete':
      return `delete volume=${name} yes`
    default:
      throw new Error(`Unknown volume action: ${actionId}`)
  }
}

/**
 * All bconsole commands for one volume, in execution order. Empty when the
 * action does not apply to this volume.
 *
 * Moving a volume to another pool only rewrites its PoolId, so the volume
 * keeps the *previous* pool's retention and limits. With
 * `params.applyPoolDefaults` the move is followed by `frompool=yes`, which
 * copies the target pool's defaults onto the volume.
 */
export function buildVolumeBulkCommands(actionId, volume, params = {}) {
  const command = buildVolumeBulkCommand(actionId, volume, params)
  if (!command) {
    return []
  }

  if (actionId === 'pool' && params.applyPoolDefaults) {
    return [command, buildVolumeBulkCommand('frompool', volume, params)]
  }

  return [command]
}

export function buildVolumeBulkPlan(actionId, volumes, params = {}) {
  return (volumes ?? []).map(volume => ({
    volume,
    commands: buildVolumeBulkCommands(actionId, volume, params),
  }))
}
