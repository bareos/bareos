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
import { resolveDirectorColors } from '../../src/utils/directorColors.js'

describe('resolveDirectorColors', () => {
  it('returns stable colors for the same director name', () => {
    expect(resolveDirectorColors('bareos-dir')).toEqual(
      resolveDirectorColors('bareos-dir')
    )
  })

  it('starts the configured palette with the old primary color', () => {
    expect(resolveDirectorColors('bareos-dir', [
      'bareos-dir',
      'bareos-dir-copy',
    ])).toEqual({
      background: '#1976D2',
      text: '#FFFFFF',
      border: '#1565C0',
    })
  })

  it('returns distinct colors for different configured director names', () => {
    expect(resolveDirectorColors('bareos-dir', [
      'bareos-dir',
      'bareos-dir-copy',
    ])).not.toEqual(
      resolveDirectorColors('bareos-dir-copy', [
        'bareos-dir',
        'bareos-dir-copy',
      ])
    )
  })

  it('returns a fallback palette for empty names', () => {
    expect(resolveDirectorColors('')).toMatchObject({
      background: '#ECEFF1',
      text: '#37474F',
      border: '#B0BEC5',
    })
  })
})
