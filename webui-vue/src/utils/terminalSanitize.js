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

/*
 * Director output is shown in xterm.js with its SGR colors intact, but it
 * also carries names and text that users or clients control. Only SGR
 * sequences, newlines, carriage returns and tabs may reach the terminal;
 * every other escape sequence or control character is dropped, so such
 * text cannot move the cursor, clear the screen, retitle the terminal or
 * make it answer back into the console input.
 */
const TERMINAL_CONTROL_RE = new RegExp(
  [
    // OSC, terminated by BEL or ST (or the end of the chunk)
    '\\x1B\\][^\\x07\\x1B]*(?:\\x07|\\x1B\\\\|$)',
    // DCS, SOS, PM and APC, terminated by ST (or the end of the chunk)
    '\\x1B[PX^_][^\\x1B]*(?:\\x1B\\\\|$)',
    // CSI
    '\\x1B\\[[0-?]*[ -/]*[@-~]?',
    // any other escape sequence
    '\\x1B[\\s\\S]?',
    // C0 controls except \t \n \r, DEL and the 8-bit C1 controls
    '[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F-\\x9F]',
  ].join('|'),
  'g',
)

const SGR_RE = /^\x1B\[[0-9;:]*m$/

export function sanitizeTerminalOutput(text) {
  return String(text ?? '').replace(TERMINAL_CONTROL_RE, match =>
    SGR_RE.test(match) ? match : '',
  )
}
