// @vitest-environment node

/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { Terminal } from '@xterm/xterm'
import { describe, expect, it } from 'vitest'
import { createConsoleResponseWriter } from '../../src/composables/consoleResponseWriter.js'

async function renderResponse(parts) {
  const emulator = new Terminal({ cols: 10, rows: 4, scrollback: 0 })
  const write = data => new Promise(resolve => emulator.write(data, resolve))
  const terminal = {
    get terminal() { return emulator },
    clearInputLine: () => write('\r\x1B[K'),
    write,
  }
  const writer = createConsoleResponseWriter(terminal, () => write('> '))

  for (let i = 0; i < parts.length; i++) {
    await writer.write(parts[i], {
      response: true,
      streaming: i < parts.length - 1,
    })
  }

  const lines = Array.from(
    { length: 4 },
    (_, row) => emulator.buffer.active.getLine(row).translateToString(true),
  )
  emulator.dispose()
  return lines
}

describe('createConsoleResponseWriter', () => {
  it('preserves output split exactly at the right margin', async () => {
    const unsplit = await renderResponse(['1234567890X'])
    const split = await renderResponse(['1234567890', 'X'])

    expect(split).toEqual(unsplit)
  })

  it('preserves output split one column before the right margin', async () => {
    const unsplit = await renderResponse(['123456789X'])
    const split = await renderResponse(['123456789', 'X'])

    expect(split).toEqual(unsplit)
  })
})
