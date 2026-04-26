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
import { buildLabelBarcodesCommand } from '../../src/utils/autochanger.js'

describe('autochanger helpers', () => {
  it('builds a label barcodes command with encrypt before slots', () => {
    expect(buildLabelBarcodesCommand({
      storage: 'Autochanger',
      pool: 'Full',
      drive: 2,
      slots: '1-10',
      encrypted: true,
    })).toBe(
      'label barcodes storage="Autochanger" pool="Full" drive=2 encrypt slots=1-10 yes'
    )
  })

  it('omits optional encrypt and slots arguments when disabled', () => {
    expect(buildLabelBarcodesCommand({
      storage: 'Autochanger',
      pool: 'Incremental',
      drive: 0,
      slots: '  ',
      encrypted: false,
    })).toBe(
      'label barcodes storage="Autochanger" pool="Incremental" drive=0 yes'
    )
  })
})
