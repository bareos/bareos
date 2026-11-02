/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
   License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
import assert from 'node:assert/strict'
import { test } from 'node:test'
import { createRenderer } from 'vue'

for (const scenario of [
  { name: 'missing token', search: '', status: 403, tokenError: true },
  { name: 'invalid token', search: '?token=wrong', status: 403, tokenError: true },
  { name: 'other upgrade failure', search: '?token=valid', status: 200 },
  { name: 'unreachable service', search: '?token=valid', networkError: true },
]) {
  test(`Reports ${scenario.name} instead of waiting`, async t => {
    let socket
    class FakeWebSocket {
      static CLOSING = 2
      constructor() {
        this.readyState = 0
        socket = this
      }
      addEventListener() {}
    }
    for (const [key, value] of Object.entries({
      WebSocket: FakeWebSocket,
      location: { search: scenario.search, protocol: 'http:', host: 'localhost:19101' },
      fetch: async path => {
        assert.ok(path.startsWith('/ws?token='))
        if (scenario.networkError) throw new TypeError('Network failure')
        return { status: scenario.status }
      },
    })) {
      const original = Object.getOwnPropertyDescriptor(globalThis, key)
      Object.defineProperty(globalThis, key, { configurable: true, writable: true, value })
      t.after(() => {
        if (original) Object.defineProperty(globalThis, key, original)
        else delete globalThis[key]
      })
    }
    const { useSetupWs } = await import(`./useSetupWs.js?test=${encodeURIComponent(scenario.name)}`)
    const renderer = createRenderer({
      createComment: () => ({}),
      insert() {},
      remove() {},
    })
    let state
    const app = renderer.createApp({
      setup() {
        state = useSetupWs()
        return () => null
      },
    })
    app.mount({})
    t.after(() => app.unmount())
    const checking = socket.onerror()
    socket.onclose()
    await checking
    assert.equal(state.connected.value, false)
    if (scenario.tokenError) {
      assert.match(state.connectionError.value, /token is missing or invalid/)
      assert.match(state.connectionError.value, /complete URL/)
    } else {
      assert.match(state.connectionError.value, /Cannot connect/)
      assert.doesNotMatch(state.connectionError.value, /token is missing or invalid/)
    }
    assert.deepEqual(state.messages.value, [])
  })
}
