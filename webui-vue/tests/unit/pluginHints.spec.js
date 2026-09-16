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
import { normalizePluginHintsResponse } from '../../src/stores/pluginHints.js'

describe('normalizePluginHintsResponse', () => {
  it('converts lowercase dird JSON keys to the camelCase hint shape', () => {
    expect(normalizePluginHintsResponse({
      hints: [{
        id: 'vmware',
        displayname: 'VMware',
        manualurl: 'TasksAndConcepts/Plugins.html#vmwareplugin',
        optionseparator: ':',
        note: 'Some note',
        supportlevel: 'bareos',
        aliases: ['vmware', 'bareos-fd-vmware'],
        options: [
          { name: 'vcserver', status: 'required', description: 'vCenter host', source: 'plugin-doc' },
        ],
      }],
    })).toEqual({
      vmware: {
        displayName: 'VMware',
        manualUrl: 'https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#vmwareplugin',
        optionSeparator: ':',
        note: 'Some note',
        supportLevel: 'bareos',
        aliases: ['vmware', 'bareos-fd-vmware'],
        options: [
          { name: 'vcserver', status: 'required', description: 'vCenter host', source: 'plugin-doc' },
        ],
      },
    })
  })

  it('expands a relative manual url against the docs.bareos.org base', () => {
    const hints = normalizePluginHintsResponse({
      hints: [{ id: 'bpipe', manualurl: 'TasksAndConcepts/Plugins.html#bpipe' }],
    })

    expect(hints.bpipe.manualUrl).toBe('https://docs.bareos.org/master/TasksAndConcepts/Plugins.html#bpipe')
  })

  it('keeps an already-absolute manual url unchanged', () => {
    const hints = normalizePluginHintsResponse({
      hints: [{ id: 'custom', manualurl: 'https://example.test/docs#custom' }],
    })

    expect(hints.custom.manualUrl).toBe('https://example.test/docs#custom')
  })

  it('defaults missing optional fields', () => {
    const hints = normalizePluginHintsResponse({ hints: [{ id: 'bare' }] })

    expect(hints.bare).toEqual({
      displayName: '',
      manualUrl: '',
      optionSeparator: ':',
      note: '',
      supportLevel: 'bareos',
      aliases: [],
      options: [],
    })
  })

  it('skips hint entries without an id', () => {
    expect(normalizePluginHintsResponse({
      hints: [{ displayname: 'No id' }, { id: 'valid' }],
    })).toEqual({ valid: expect.objectContaining({ displayName: '' }) })
  })

  it('returns an empty object for a missing or malformed hints array', () => {
    expect(normalizePluginHintsResponse(null)).toEqual({})
    expect(normalizePluginHintsResponse({})).toEqual({})
    expect(normalizePluginHintsResponse({ hints: 'not-an-array' })).toEqual({})
  })
})
