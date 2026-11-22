// @vitest-environment jsdom

/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.
 */

import { beforeEach, describe, expect, it, vi } from 'vitest'
import { createConsoleResponseWriter } from '../../src/composables/consoleResponseWriter.js'
import { useConsoleTerminal } from '../../src/composables/useConsoleTerminal.js'

const mocks = vi.hoisted(() => ({ instances: [] }))
vi.mock('@xterm/xterm', () => ({
  Terminal: class {
    constructor() {
      this.writes = []
      this.buffer = { active: { type: 'normal', cursorX: 0, cursorY: 0, baseY: 0 } }
      this.cols = 80
      this.rows = 24
      mocks.instances.push(this)
    }

    loadAddon() {}
    open() {}
    onResize() { return { dispose() {} } }
    registerMarker() { return { line: this.markerLine ?? 0, dispose() {} } }
    write(data, callback) { this.writes.push({ data, callback }) }
    dispose() {}
  },
}))
vi.mock('@xterm/addon-fit', () => ({ FitAddon: class {} }))

describe('console terminal write queue', () => {
  beforeEach(() => {
    mocks.instances.length = 0
    vi.stubGlobal('ResizeObserver', class {
      observe() {}
      disconnect() {}
    })
  })

  it('keeps a response and its prompt redraw together when input changes', async () => {
    const terminal = useConsoleTerminal()
    terminal.mount(document.createElement('div'))
    const xterm = mocks.instances[0]
    const writer = createConsoleResponseWriter(
      terminal, target => target.writeInputLine('> response prompt', ''),
    )
    const response = writer.write('Job queued', { response: true, streaming: false })
    await vi.waitFor(() => expect(xterm.writes).toHaveLength(1))
    const edit = terminal.writeInputLine('> edited', '')

    for (let i = 0; i < 7; i++) {
      await vi.waitFor(() => expect(xterm.writes.length).toBeGreaterThan(i))
      xterm.writes[i].callback()
    }
    await Promise.all([response, edit])
    expect(xterm.writes.map(write => write.data)).toEqual([
      '\r\x1B[K', 'Job queued', '\r\n', '\r\x1B[K', '> response prompt',
      '\x1B[1;1H\x1B[J', '> edited',
    ])
    terminal.dispose()
  })

  it('ignores a pending erase callback from a disposed terminal', async () => {
    const terminal = useConsoleTerminal()
    terminal.mount(document.createElement('div'))
    const old = mocks.instances[0]
    terminal.writeInputLine('old', '')
    terminal.mount(document.createElement('div'))
    const current = mocks.instances[1]
    current.markerLine = 5
    const input = terminal.writeInputLine('new', '')
    current.writes[0].callback()
    current.writes[1].callback()
    await input
    old.writes[0].callback()
    expect(old.writes).toHaveLength(1)
    const clear = terminal.clearInputLine()
    expect(current.writes[2].data).toBe('\x1B[6;1H\x1B[J')
    current.writes[2].callback()
    await clear
    terminal.dispose()
  })
})
