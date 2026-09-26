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

export const VOLUME_BULK_ACTIONS = [
  { id: 'pool', label: 'Move to pool', icon: 'drive_file_move', param: 'pool' },
  { id: 'enable', label: 'Enable', icon: 'check_circle' },
  { id: 'disable', label: 'Disable', icon: 'pause_circle' },
  { id: 'frompool', label: 'Reset from pool', icon: 'settings_backup_restore' },
  { id: 'comment', label: 'Set comment', icon: 'comment', param: 'comment' },
  { id: 'prune', label: 'Prune', icon: 'content_cut' },
  { id: 'purge', label: 'Purge', icon: 'delete_sweep', destructive: true },
  { id: 'truncate', label: 'Truncate', icon: 'layers_clear', destructive: true },
  { id: 'delete', label: 'Delete from catalog', icon: 'delete_forever', destructive: true },
]

export function findVolumeBulkAction(id) {
  return VOLUME_BULK_ACTIONS.find(action => action.id === id) ?? null
}

/**
 * Returns the bconsole command for one volume, or null when the action
 * does not apply to this volume (e.g. truncate of a non-purged volume).
 */
export function buildVolumeBulkCommand(actionId, volume, params = {}) {
  const name = quoteDirectorString(volume?.volumename ?? volume?.name ?? '')
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
      if (volume?.volstatus !== 'Purged') {
        return null
      }
      return `truncate volstatus=Purged volume=${name} yes`
    case 'delete':
      return `delete volume=${name} yes`
    default:
      throw new Error(`Unknown volume action: ${actionId}`)
  }
}

export function buildVolumeBulkPlan(actionId, volumes, params = {}) {
  return (volumes ?? []).map(volume => ({
    volume,
    command: buildVolumeBulkCommand(actionId, volume, params),
  }))
}
