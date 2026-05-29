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
  collectVueMessageIds,
  generateWebUiI18n,
  generatedLocalesPath,
  generatedMessagesPath,
} from '../../scripts/generate-webui-i18n.mjs'

describe('webui i18n generation', () => {
  it('keeps generated locales in sync with legacy locale definitions', () => {
    const { localesSource } = generateWebUiI18n()

    expect(readFileSync(generatedLocalesPath, 'utf8')).toBe(localesSource)
  })

  it('keeps generated messages in sync with legacy catalogs and vue msgids', () => {
    const { messagesSource } = generateWebUiI18n()

    expect(readFileSync(generatedMessagesPath, 'utf8')).toBe(messagesSource)
  })

  it('collects newer vue-only msgids into the generated catalog', () => {
    const vueMessageIds = collectVueMessageIds()

    expect(vueMessageIds).toContain('All known plugin hints')
    expect(vueMessageIds).toContain('Catalog Maintenance')
    expect(vueMessageIds).toContain('Configuration Status')
  })
})
