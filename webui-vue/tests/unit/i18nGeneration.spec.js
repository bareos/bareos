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

import { readFileSync } from 'node:fs'

import { describe, expect, it } from 'vitest'

import {
  buildExpectedCatalogs,
  catalogPath,
  checkJsonCatalogs,
  collectVueMessageIds,
  localeManifestPath,
  projectRoot,
  validateCatalogs,
} from '../../scripts/generate-webui-i18n.mjs'

describe('webui i18n generation', () => {
  it('keeps every JSON catalog synchronized with Vue source messages', () => {
    expect(() => checkJsonCatalogs()).not.toThrow()
  })

  it('collects literal and dynamically stored message IDs', () => {
    const messageIds = collectVueMessageIds()

    expect(messageIds).toContain('All known plugin hints')
    expect(messageIds).toContain('Configuration Status')
    expect(messageIds).toContain('Try the subscription release')
    expect(messageIds).toContain('Virtual Full')
    expect(messageIds).toContain('Action On Purge')
    expect(messageIds).toContain('in drive {drive}')
  })

  it('uses Vue-owned catalogs with no PHP WebUI dependency', () => {
    const generator = readFileSync(
      `${projectRoot}/scripts/generate-webui-i18n.mjs`,
      'utf8',
    )
    expect(generator).not.toMatch(/module\/Application|legacyLanguage|readLegacy/)
    expect(localeManifestPath).toBe(`${projectRoot}/src/i18n/locales/locales.json`)
  })

  it('keeps locale files sorted and validates placeholders', () => {
    const expected = buildExpectedCatalogs()
    expect(validateCatalogs(expected)).toEqual([])

    for (const { value: locale } of expected.manifest.locales) {
      const catalog = JSON.parse(readFileSync(catalogPath(locale), 'utf8'))
      expect(Object.keys(catalog)).toEqual(expected.messageIds)
    }

    const invalid = structuredClone(expected)
    invalid.catalogs.de_DE['Cancel failed: {message}'] = 'Abbruch fehlgeschlagen'
    expect(validateCatalogs(invalid)).toContain(
      'de_DE: "Cancel failed: {message}" has placeholders {}; expected {message}'
    )
  })

  it('keeps only Vue terms while preserving seeded legacy translations', () => {
    const expected = buildExpectedCatalogs()

    expect(expected.catalogs.de_DE.Restore).toBe('Wiederherstellen')
    expect(expected.catalogs.de_DE.Schedules).toBe('Zeitpläne')
    expect(expected.catalogs.de_DE['Configuration Status']).toBe('')
    expect(expected.messageIds).not.toContain(
      'bconsole (batch-mode), please handle with care.',
    )
  })
})
