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

import { describe, expect, it } from 'vitest'
import {
  volumeEncryptionKey,
  volumeHasEncryptionKey,
} from '../../src/utils/volumes.js'

describe('volume encryption helpers', () => {
  it('detects an encryption key from the presence flag', () => {
    expect(volumeHasEncryptionKey({ hasencryptionkey: '1' })).toBe(true)
    expect(volumeHasEncryptionKey({ HasEncryptionKey: true })).toBe(true)
    expect(volumeHasEncryptionKey({ hasencryptionkey: '0' })).toBe(false)
  })

  it('detects catalog encryption keys with different field casings', () => {
    expect(volumeEncryptionKey({ EncryptionKey: 'abc123' })).toBe('abc123')
    expect(volumeEncryptionKey({ encryptionkey: 'xyz789' })).toBe('xyz789')
  })

  it('treats empty and missing keys as not encrypted', () => {
    expect(volumeHasEncryptionKey({ encryptionkey: '   ' })).toBe(false)
    expect(volumeHasEncryptionKey({})).toBe(false)
  })
})
