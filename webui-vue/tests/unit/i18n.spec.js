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
import { translate } from '../../src/i18n/index.js'

describe('webui translations', () => {
  it('reuses PHP WebUI translations for shared labels', () => {
    expect(translate('de_DE', 'Restore')).toBe('Wiederherstellen')
    expect(translate('de_DE', 'Schedules')).toBe('Zeitpläne')
    expect(translate('de_DE', 'Storages')).toBe('Speicher')
  })

  it('falls back to the English msgid for Vue-only labels', () => {
    expect(translate('de_DE', 'Advanced connection settings')).toBe('Advanced connection settings')
    expect(translate('de_DE', 'Cancel failed: {message}', { message: 'boom' })).toBe('Cancel failed: boom')
  })
})
