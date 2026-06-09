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
import { toUserVisibleDirectorError } from '../../src/utils/directorErrors.js'

describe('toUserVisibleDirectorError', () => {
  it('normalizes authentication failures', () => {
    expect(toUserVisibleDirectorError('Director: authentication failed: bad password'))
      .toBe('Authentication failed')
  })

  it('normalizes TLS-PSK handshake failures as authentication failures', () => {
    expect(
      toUserVisibleDirectorError(
        'Director: TLS-PSK handshake failed: tlsv1 alert decrypt error'
      )
    ).toBe('Authentication failed')
  })

  it('normalizes generic connection errors', () => {
    expect(toUserVisibleDirectorError('Cannot connect to proxy at ws://example.test/ws'))
      .toBe('Could not connect to director.')
  })

  it('uses the provided fallback messages', () => {
    expect(
      toUserVisibleDirectorError('', {
        authenticationMessage: 'Bad credentials',
        connectionMessage: 'Proxy unavailable',
      })
    ).toBe('Proxy unavailable')
  })
})
