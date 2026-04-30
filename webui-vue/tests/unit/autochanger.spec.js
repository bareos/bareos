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
  buildExportCommand,
  buildImportCommand,
  buildLabelBarcodesCommand,
  formatInDriveLabel,
  shouldRefreshAutochangerTables,
} from '../../src/utils/autochanger.js'

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

  it('builds an export command with an optional destination slot', () => {
    expect(buildExportCommand({
      storage: 'Autochanger',
      srcSlot: 4,
      dstSlot: 91,
    })).toBe(
      'export storage="Autochanger" srcslots=4 dstslots=91'
    )

    expect(buildExportCommand({
      storage: 'Autochanger',
      srcSlot: 4,
    })).toBe(
      'export storage="Autochanger" srcslots=4'
    )
  })

  it('builds an import command with optional source and destination slots', () => {
    expect(buildImportCommand({
      storage: 'Autochanger',
      srcSlot: 91,
      dstSlot: 4,
    })).toBe(
      'import storage="Autochanger" srcslots=91 dstslots=4'
    )

    expect(buildImportCommand({
      storage: 'Autochanger',
      srcSlot: 91,
    })).toBe(
      'import storage="Autochanger" srcslots=91'
    )
  })

  it('keeps autochanger tables refreshing while command output is streaming', () => {
    expect(shouldRefreshAutochangerTables({
      selectedStorage: 'Autochanger',
      slotsLoading: false,
      commandRunning: true,
    })).toBe(true)
  })

  it('falls back to a concrete drive label when the translation key is missing', () => {
    expect(formatInDriveLabel((key) => key, 7)).toBe('in drive 7')
  })

  it('skips autochanger refreshes without a storage or while slots are loading', () => {
    expect(shouldRefreshAutochangerTables({
      selectedStorage: '',
      slotsLoading: false,
      commandRunning: false,
    })).toBe(false)

    expect(shouldRefreshAutochangerTables({
      selectedStorage: 'Autochanger',
      slotsLoading: true,
      commandRunning: false,
    })).toBe(false)
  })
})
