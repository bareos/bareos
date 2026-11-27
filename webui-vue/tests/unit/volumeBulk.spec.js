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

import { describe, expect, it } from 'vitest'
import {
  MAX_COMMENT_LENGTH,
  buildJobCommentCommand,
  buildVolumeBulkCommand,
  buildVolumeBulkCommands,
  buildVolumeBulkPlan,
  buildVolumeCommentCommand,
  VOLUME_BULK_ACTIONS,
  findVolumeBulkAction,
  isVolumeActionApplicable,
  sanitizeComment,
} from '../../src/utils/volumeBulk.js'

describe('volume bulk helpers', () => {
  it('sanitizes comments to a single trimmed line', () => {
    expect(sanitizeComment('  first\r\nsecond\tthird  ')).toBe('first second third')
    expect(sanitizeComment(null)).toBe('')
    expect(sanitizeComment('x'.repeat(MAX_COMMENT_LENGTH + 10))).toHaveLength(MAX_COMMENT_LENGTH)
  })

  it('builds quoted comment commands', () => {
    expect(buildVolumeCommentCommand('Full-0001', 'say "hi"'))
      .toBe('update volume="Full-0001" comment="say \\"hi\\""')
    expect(buildVolumeCommentCommand('Full-0001', '')).toBe('update volume="Full-0001" comment=""')
    expect(buildJobCommentCommand('42', 'offsite copy')).toBe('update jobid=42 comment="offsite copy"')
  })

  it('rejects invalid job ids', () => {
    expect(() => buildJobCommentCommand('abc', 'x')).toThrow('Invalid job id')
    expect(() => buildJobCommentCommand(0, 'x')).toThrow('Invalid job id')
  })

  it('builds per-volume commands for every action', () => {
    const volume = { volumename: 'Vol-1', volstatus: 'Full' }
    expect(buildVolumeBulkCommand('pool', volume, { pool: 'Scratch' }))
      .toBe('update volume="Vol-1" pool="Scratch"')
    expect(buildVolumeBulkCommand('enable', volume)).toBe('update volume="Vol-1" enabled=yes')
    expect(buildVolumeBulkCommand('disable', volume)).toBe('update volume="Vol-1" enabled=no')
    expect(buildVolumeBulkCommand('frompool', volume)).toBe('update volume="Vol-1" frompool=yes')
    expect(buildVolumeBulkCommand('comment', volume, { comment: 'shelf 3' }))
      .toBe('update volume="Vol-1" comment="shelf 3"')
    expect(buildVolumeBulkCommand('prune', volume)).toBe('prune volume="Vol-1" yes')
    expect(buildVolumeBulkCommand('purge', volume)).toBe('purge volume="Vol-1"')
    expect(buildVolumeBulkCommand('delete', volume)).toBe('delete volume="Vol-1" yes')
  })

  it('only truncates purged volumes', () => {
    expect(buildVolumeBulkCommand('truncate', { volumename: 'Vol-1', volstatus: 'Full' })).toBeNull()
    expect(buildVolumeBulkCommand('truncate', { volumename: 'Vol-2', volstatus: 'Purged' }))
      .toBe('truncate volstatus=Purged volume="Vol-2" yes')
  })

  it('requires a target pool and a known action', () => {
    expect(() => buildVolumeBulkCommand('pool', { volumename: 'Vol-1' })).toThrow('target pool')
    expect(() => buildVolumeBulkCommand('explode', { volumename: 'Vol-1' })).toThrow('Unknown')
  })

  it('marks destructive actions', () => {
    expect(findVolumeBulkAction('purge')?.destructive).toBe(true)
    expect(findVolumeBulkAction('truncate')?.destructive).toBe(true)
    expect(findVolumeBulkAction('delete')?.destructive).toBe(true)
    expect(findVolumeBulkAction('prune')?.destructive).toBeFalsy()
    expect(findVolumeBulkAction('nope')).toBeNull()
  })

  it('builds a plan with skipped entries', () => {
    const volumes = [
      { volumename: 'A', volstatus: 'Purged' },
      { volumename: 'B', volstatus: 'Append' },
    ]
    expect(buildVolumeBulkPlan('truncate', volumes)).toEqual([
      { volume: volumes[0], commands: ['truncate volstatus=Purged volume="A" yes'] },
      { volume: volumes[1], commands: [] },
    ])
    expect(buildVolumeBulkPlan('prune', null)).toEqual([])
  })

  it('chains frompool after a pool move when asked to', () => {
    const volume = { volumename: 'Vol-1' }

    expect(buildVolumeBulkCommands('pool', volume, { pool: 'Scratch' })).toEqual([
      'update volume="Vol-1" pool="Scratch"',
    ])
    expect(buildVolumeBulkCommands('pool', volume, {
      pool: 'Scratch',
      applyPoolDefaults: true,
    })).toEqual([
      'update volume="Vol-1" pool="Scratch"',
      'update volume="Vol-1" frompool=yes',
    ])
  })

  it('does not chain frompool for other actions or skipped volumes', () => {
    expect(buildVolumeBulkCommands('enable', { volumename: 'Vol-1' }, {
      applyPoolDefaults: true,
    })).toEqual(['update volume="Vol-1" enabled=yes'])

    expect(buildVolumeBulkCommands('truncate', {
      volumename: 'Vol-1',
      volstatus: 'Append',
    })).toEqual([])
  })

  it('offers status changes only from the statuses they apply to', () => {
    const cases = [
      ['status-used', 'Append', 'Used'],
      ['status-readonly', 'Full', 'Read-Only'],
      ['status-readonly', 'Used', 'Read-Only'],
      ['status-archive', 'Used', 'Archive'],
      ['status-unprotect', 'Read-Only', 'Used'],
      ['status-unprotect', 'Archive', 'Used'],
      ['status-clearerror', 'Error', 'Used'],
    ]
    for (const [id, from, to] of cases) {
      expect(buildVolumeBulkCommand(id, { volumename: 'V 1', volstatus: from }))
        .toBe(`update volume="V 1" volstatus=${to}`)
    }
    expect(buildVolumeBulkCommand('status-used', { volumename: 'V', volstatus: 'Full' })).toBeNull()
    expect(buildVolumeBulkCommand('status-clearerror', { volumename: 'V', volstatus: 'Append' })).toBeNull()
    expect(buildVolumeBulkCommands('status-archive', { volumename: 'V', volstatus: 'Purged' })).toEqual([])
  })

  it('never offers the dangerous target statuses', () => {
    const targets = VOLUME_BULK_ACTIONS.map(action => action.volstatus).filter(Boolean)
    for (const status of ['Recycle', 'Purged', 'Append', 'Cleaning']) {
      expect(targets).not.toContain(status)
    }
    for (const action of VOLUME_BULK_ACTIONS.filter(entry => entry.volstatus)) {
      expect(action.requiresStatus).not.toContain(action.volstatus)
    }
  })

  it('checks applicability against the required statuses', () => {
    expect(isVolumeActionApplicable(findVolumeBulkAction('enable'), 'Full')).toBe(true)
    expect(isVolumeActionApplicable(findVolumeBulkAction('truncate'), 'Purged')).toBe(true)
    expect(isVolumeActionApplicable(findVolumeBulkAction('truncate'), 'Full')).toBe(false)
    expect(isVolumeActionApplicable(null, 'Full')).toBe(false)
  })
})
