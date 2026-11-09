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
import { sanitizeTerminalOutput } from '../../src/utils/terminalSanitize.js'

describe('sanitizeTerminalOutput', () => {
  it('keeps plain text, newlines, carriage returns and tabs', () => {
    expect(sanitizeTerminalOutput('a\tb\r\nc\n')).toBe('a\tb\r\nc\n')
  })

  it('keeps SGR color sequences', () => {
    const text = '\x1B[1;94mdir\x1B[0m \x1B[97;44mx\x1B[39m \x1B[m'
    expect(sanitizeTerminalOutput(text)).toBe(text)
  })

  it('drops screen and cursor control sequences', () => {
    expect(sanitizeTerminalOutput('a\x1B[2Jb\x1B[Hc\x1B[10;5Hd\x1B[Ke'))
      .toBe('abcde')
    expect(sanitizeTerminalOutput('x\x1B[?1049hy')).toBe('xy')
  })

  it('drops OSC, DCS and APC sequences', () => {
    expect(sanitizeTerminalOutput('a\x1B]0;evil\x07b')).toBe('ab')
    expect(sanitizeTerminalOutput('a\x1B]2;evil\x1B\\b')).toBe('ab')
    expect(sanitizeTerminalOutput('a\x1BPq#0\x1B\\b')).toBe('ab')
    expect(sanitizeTerminalOutput('a\x1B_x\x1B\\b')).toBe('ab')
  })

  it('drops other escapes, C0/C1 controls and DEL', () => {
    expect(sanitizeTerminalOutput('a\x1Bcb\x1B7c')).toBe('abc')
    expect(sanitizeTerminalOutput('a\x07b\x08c\x00d\x7Fe\x9Bf')).toBe('abcdef')
  })

  it('drops a non-SGR CSI with a final byte that is not m', () => {
    expect(sanitizeTerminalOutput('\x1B[5nok')).toBe('ok')
    expect(sanitizeTerminalOutput('\x1B[1;2;3tok')).toBe('ok')
  })

  it('handles empty and nullish input', () => {
    expect(sanitizeTerminalOutput('')).toBe('')
    expect(sanitizeTerminalOutput(null)).toBe('')
    expect(sanitizeTerminalOutput(undefined)).toBe('')
  })
})
