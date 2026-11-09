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

/**
 * Thin xterm.js adapter for the Bareos WebUI console.
 *
 * This owns the xterm.js lifecycle (creation, fitting, disposal) and
 * exposes a small write/clear/focus/fit API. It intentionally knows
 * nothing about Director protocol details (ANSI parsing, selection
 * menus, command history) — the caller is responsible for feeding it
 * raw bytes exactly as received from the Director (via the webui-proxy),
 * so that xterm.js itself performs full ANSI/SGR interpretation
 * (colors, backgrounds, reverse video, cursor movement, etc.).
 */

import { Terminal } from '@xterm/xterm'
import { FitAddon } from '@xterm/addon-fit'
import '@xterm/xterm/css/xterm.css'

const DEFAULT_THEME = {
  background: '#1a1a1a',
  foreground: '#e0e0e0',
  cursor: '#80cbc4',
  cursorAccent: '#1a1a1a',
  selectionBackground: '#3b82f6',
}

/**
 * @param {object} [options]
 * @param {object} [options.theme] xterm theme overrides
 * @param {(size: { rows: number, cols: number }) => void} [options.onResize]
 *        called (debounced) whenever the fitted terminal size changes
 * @param {(data: string) => void} [options.onData]
 *        called with raw input xterm receives (keystrokes/paste)
 */
export function useConsoleTerminal(options = {}) {
  const resizeDebounceMs = options.resizeDebounceMs ?? 200

  let terminal = null
  let fitAddon = null
  let resizeObserver = null
  let container = null
  let resizeTimer = null
  let lastReportedSize = null
  let dataDisposable = null
  let resizeDisposable = null

  function reportResizeNow() {
    if (!terminal) {
      return
    }
    const { rows, cols } = terminal
    if (!rows || !cols) {
      return
    }
    if (
      lastReportedSize
      && lastReportedSize.rows === rows
      && lastReportedSize.cols === cols
    ) {
      return
    }
    lastReportedSize = { rows, cols }
    options.onResize?.({ rows, cols })
  }

  function scheduleResizeReport() {
    if (resizeTimer) {
      clearTimeout(resizeTimer)
    }
    resizeTimer = setTimeout(() => {
      resizeTimer = null
      reportResizeNow()
    }, resizeDebounceMs)
  }

  function fit() {
    if (!fitAddon || !container) {
      return
    }
    // A hidden/zero-size container (e.g. an inactive director tab) cannot
    // be fitted — xterm's FitAddon throws/produces bogus dimensions in
    // that case, so skip until the container has real dimensions again.
    if (container.offsetWidth === 0 || container.offsetHeight === 0) {
      return
    }
    fitAddon.fit()
    scheduleResizeReport()
  }

  function mount(el) {
    if (terminal) {
      dispose()
    }

    container = el
    terminal = new Terminal({
      convertEol: true,
      scrollback: 5000,
      fontFamily: '"Courier New", Courier, monospace',
      fontSize: 13,
      cursorBlink: true,
      theme: { ...DEFAULT_THEME, ...(options.theme ?? {}) },
    })
    fitAddon = new FitAddon()
    terminal.loadAddon(fitAddon)
    terminal.open(el)

    if (options.onData) {
      dataDisposable = terminal.onData(options.onData)
    }
    resizeDisposable = terminal.onResize(() => scheduleResizeReport())

    resizeObserver = new ResizeObserver(() => fit())
    resizeObserver.observe(el)

    fit()

    // FitAddon measures the current cell size from the (possibly not yet
    // painted) monospace font. If the font metrics settle after this
    // first measurement — e.g. the browser substitutes a fallback font
    // until it finishes loading — the initial rows/cols can end up
    // slightly too large for the container, and Director-side content
    // sized to that (wrong) row count (like interactive selection menus)
    // then overflows into scrollback. Re-fit once the fonts are known to
    // be ready to correct for this.
    if (document.fonts?.ready) {
      document.fonts.ready.then(() => fit())
    }
  }

  function write(data) {
    terminal?.write(data)
  }

  function clear() {
    terminal?.reset()
  }

  function focus() {
    terminal?.focus()
  }

  function dispose() {
    if (resizeTimer) {
      clearTimeout(resizeTimer)
      resizeTimer = null
    }
    resizeObserver?.disconnect()
    resizeObserver = null
    dataDisposable?.dispose()
    dataDisposable = null
    resizeDisposable?.dispose()
    resizeDisposable = null
    terminal?.dispose()
    terminal = null
    fitAddon = null
    container = null
    lastReportedSize = null
  }

  return {
    mount,
    write,
    clear,
    focus,
    fit,
    dispose,
    get terminal() { return terminal },
  }
}
