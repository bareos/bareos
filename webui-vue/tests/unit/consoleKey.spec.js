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
import { applyConsoleKey } from '../../src/stores/consoleSessions.js'

function makeSession(cmd = '', cursorPos = cmd.length) {
  return { cmd, cursorPos }
}

function key(k, opts = {}) {
  return { key: k, ctrlKey: false, altKey: false, ...opts }
}

describe('applyConsoleKey', () => {
  it('inserts a character at the cursor position', () => {
    const s = makeSession('helo', 3)
    applyConsoleKey(s, key('l'))
    expect(s.cmd).toBe('hello')
    expect(s.cursorPos).toBe(4)
  })

  it('appends a character when cursor is at end', () => {
    const s = makeSession('hi')
    applyConsoleKey(s, key('!'))
    expect(s.cmd).toBe('hi!')
    expect(s.cursorPos).toBe(3)
  })

  it('moves cursor left with ArrowLeft', () => {
    const s = makeSession('abc', 3)
    applyConsoleKey(s, key('ArrowLeft'))
    expect(s.cursorPos).toBe(2)
    expect(s.cmd).toBe('abc')
  })

  it('does not move cursor left past 0', () => {
    const s = makeSession('abc', 0)
    applyConsoleKey(s, key('ArrowLeft'))
    expect(s.cursorPos).toBe(0)
  })

  it('moves cursor right with ArrowRight', () => {
    const s = makeSession('abc', 1)
    applyConsoleKey(s, key('ArrowRight'))
    expect(s.cursorPos).toBe(2)
  })

  it('does not move cursor right past end', () => {
    const s = makeSession('abc', 3)
    applyConsoleKey(s, key('ArrowRight'))
    expect(s.cursorPos).toBe(3)
  })

  it('moves cursor to start with Home', () => {
    const s = makeSession('abc', 2)
    applyConsoleKey(s, key('Home'))
    expect(s.cursorPos).toBe(0)
  })

  it('moves cursor to start with Ctrl+A', () => {
    const s = makeSession('abc', 2)
    applyConsoleKey(s, key('a', { ctrlKey: true }))
    expect(s.cursorPos).toBe(0)
  })

  it('moves cursor to end with End', () => {
    const s = makeSession('abc', 0)
    applyConsoleKey(s, key('End'))
    expect(s.cursorPos).toBe(3)
  })

  it('moves cursor to end with Ctrl+E', () => {
    const s = makeSession('abc', 0)
    applyConsoleKey(s, key('e', { ctrlKey: true }))
    expect(s.cursorPos).toBe(3)
  })

  it('deletes character before cursor with Backspace', () => {
    const s = makeSession('abcd', 2)
    applyConsoleKey(s, key('Backspace'))
    expect(s.cmd).toBe('acd')
    expect(s.cursorPos).toBe(1)
  })

  it('does nothing on Backspace at position 0', () => {
    const s = makeSession('abc', 0)
    applyConsoleKey(s, key('Backspace'))
    expect(s.cmd).toBe('abc')
    expect(s.cursorPos).toBe(0)
  })

  it('deletes character at cursor with Delete', () => {
    const s = makeSession('abcd', 1)
    applyConsoleKey(s, key('Delete'))
    expect(s.cmd).toBe('acd')
    expect(s.cursorPos).toBe(1)
  })

  it('does nothing on Delete at end', () => {
    const s = makeSession('abc', 3)
    applyConsoleKey(s, key('Delete'))
    expect(s.cmd).toBe('abc')
    expect(s.cursorPos).toBe(3)
  })

  it('kills to end of line with Ctrl+K', () => {
    const s = makeSession('hello world', 5)
    applyConsoleKey(s, key('k', { ctrlKey: true }))
    expect(s.cmd).toBe('hello')
    expect(s.cursorPos).toBe(5)
  })

  it('kills to start of line with Ctrl+U', () => {
    const s = makeSession('hello world', 5)
    applyConsoleKey(s, key('u', { ctrlKey: true }))
    expect(s.cmd).toBe(' world')
    expect(s.cursorPos).toBe(0)
  })

  it('does not handle Tab or Enter', () => {
    const s = makeSession('abc')
    expect(applyConsoleKey(s, key('Tab'))).toBe(false)
    expect(applyConsoleKey(s, key('Enter'))).toBe(false)
    expect(s.cmd).toBe('abc')
  })

  it('does not insert when altKey is set', () => {
    const s = makeSession('abc')
    expect(applyConsoleKey(s, key('x', { altKey: true }))).toBe(false)
    expect(s.cmd).toBe('abc')
  })
})
