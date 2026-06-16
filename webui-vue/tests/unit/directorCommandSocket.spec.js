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
import { defaultDirectorWsUrl } from '../../src/utils/directorCommandSocket.js'

describe('defaultDirectorWsUrl', () => {
  it('uses /ws when served from root', () => {
    window.history.replaceState({}, '', '/')
    const wsUrl = new URL(defaultDirectorWsUrl())
    expect(wsUrl.protocol).toBe('ws:')
    expect(wsUrl.pathname).toBe('/ws')
  })

  it('uses app base path when served from a subdirectory', () => {
    window.history.replaceState({}, '', '/bareos-webui-vue/')
    const wsUrl = new URL(defaultDirectorWsUrl())
    expect(wsUrl.protocol).toBe('ws:')
    expect(wsUrl.pathname).toBe('/bareos-webui-vue/ws')
  })

  it('strips filename-like path segments before appending ws', () => {
    window.history.replaceState({}, '', '/bareos-webui-vue/index.html')
    const wsUrl = new URL(defaultDirectorWsUrl())
    expect(wsUrl.protocol).toBe('ws:')
    expect(wsUrl.pathname).toBe('/bareos-webui-vue/ws')
  })
})
