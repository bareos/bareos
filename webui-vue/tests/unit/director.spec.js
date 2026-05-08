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

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { createPinia, setActivePinia } from 'pinia'
import { useAuthStore } from '../../src/stores/auth.js'
import { useDirectorStore } from '../../src/stores/director.js'

class FakeWebSocket {
  static instances = []
  static CONNECTING = 0
  static OPEN = 1
  static CLOSING = 2
  static CLOSED = 3

  constructor(url) {
    this.url = url
    this.readyState = FakeWebSocket.CONNECTING
    this.sent = []
    this.onopen = null
    this.onmessage = null
    this.onerror = null
    this.onclose = null
    FakeWebSocket.instances.push(this)
  }

  send(payload) {
    this.sent.push(payload)
  }

  open() {
    this.readyState = FakeWebSocket.OPEN
    this.onopen?.()
  }

  close() {
    this.readyState = FakeWebSocket.CLOSED
    this.onclose?.()
  }
}

describe('director store', () => {
  beforeEach(() => {
    sessionStorage.clear()
    setActivePinia(createPinia())
    FakeWebSocket.instances = []
    vi.stubGlobal('WebSocket', FakeWebSocket)
    vi.useFakeTimers()
})
  afterEach(() => {
    vi.useRealTimers()
  })

  it('sends the selected director during websocket auth', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()

    expect(socket.sent).toHaveLength(1)
    expect(JSON.parse(socket.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })
  })

  it('loads the available directors from the proxy', async () => {
    const director = useDirectorStore()

    const loading = director.fetchAvailableDirectors()
    const socket = FakeWebSocket.instances[0]
    socket.open()

    expect(JSON.parse(socket.sent[0])).toEqual({ type: 'list_directors' })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'director_list',
        directors: ['bareos-dir', 'bareos-dir-2'],
      }),
    })

    await expect(loading).resolves.toEqual(['bareos-dir', 'bareos-dir-2'])
    expect(director.availableDirectors).toEqual(['bareos-dir', 'bareos-dir-2'])
  })

  it('deduplicates concurrent director list requests', async () => {
    const director = useDirectorStore()

    const firstLoading = director.fetchAvailableDirectors()
    const secondLoading = director.fetchAvailableDirectors()

    expect(FakeWebSocket.instances).toHaveLength(1)

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'director_list',
        directors: ['bareos-dir', 'bareos-dir-2'],
      }),
    })

    await expect(firstLoading).resolves.toEqual(['bareos-dir', 'bareos-dir-2'])
    await expect(secondLoading).resolves.toEqual(['bareos-dir', 'bareos-dir-2'])
  })

  it('reuses the cached director list after it has been loaded', async () => {
    const director = useDirectorStore()

    const initialLoading = director.fetchAvailableDirectors()
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'director_list',
        directors: ['bareos-dir', 'bareos-dir-2'],
      }),
    })
    await initialLoading

    await expect(director.fetchAvailableDirectors()).resolves.toEqual([
      'bareos-dir',
      'bareos-dir-2',
    ])
    expect(FakeWebSocket.instances).toHaveLength(1)
  })

  it('reloads the director list when forced', async () => {
    const director = useDirectorStore()

    const firstLoading = director.fetchAvailableDirectors()
    const firstSocket = FakeWebSocket.instances[0]
    firstSocket.open()
    firstSocket.onmessage?.({
      data: JSON.stringify({
        type: 'director_list',
        directors: ['bareos-dir'],
      }),
    })
    await firstLoading

    const secondLoading = director.fetchAvailableDirectors({ forceReload: true })
    const secondSocket = FakeWebSocket.instances[1]
    secondSocket.open()
    secondSocket.onmessage?.({
      data: JSON.stringify({
        type: 'director_list',
        directors: ['bareos-dir', 'bareos-dir-2'],
      }),
    })

    await expect(secondLoading).resolves.toEqual(['bareos-dir', 'bareos-dir-2'])
    expect(FakeWebSocket.instances).toHaveLength(2)
  })

  it('stores director transport from auth_ok', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'cleartext',
      }),
    })

    expect(director.status).toBe('connected')
    expect(director.transport).toBe('cleartext')
  })

  it('sends keepalive pings after authentication', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'cleartext',
      }),
    })

    vi.advanceTimersByTime(20_000)

    expect(JSON.parse(socket.sent[1])).toEqual({ type: 'ping' })
  })

  it('reconnects automatically after an unexpected close', () => {
    const auth = useAuthStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir', 'secret')
    director.connect(auth.getCredentials())

    const firstSocket = FakeWebSocket.instances[0]
    firstSocket.open()
    firstSocket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'cleartext',
      }),
    })

    firstSocket.close()
    vi.advanceTimersByTime(1_000)

    const secondSocket = FakeWebSocket.instances[1]
    expect(secondSocket).toBeDefined()
    secondSocket.open()

    expect(JSON.parse(secondSocket.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })
  })

  it('uses default director connection values when optional settings are omitted', () => {
    const director = useDirectorStore()

    director.connect({
      username: 'admin',
      password: 'secret',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()

    expect(JSON.parse(socket.sent[0])).toEqual({
      type: 'auth',
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })
  })

  it('waits for a successful connection result', async () => {
    const director = useDirectorStore()

    const connected = director.connectAndWait({
      username: 'admin',
      password: 'secret',
      director: 'bareos-dir',
    })

    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        transport: 'cleartext',
      }),
    })

    await expect(connected).resolves.toBe(true)
  })

  it('allows raw commands to run longer than the normal command timeout', async () => {
    const auth = useAuthStore()
    const director = useDirectorStore()

    auth.login('admin', 'bareos-dir', 'secret')

    const rawPromise = director.rawCall('messages')
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        director: 'bareos-dir',
      }),
    })

    expect(JSON.parse(socket.sent[1])).toEqual({
      type: 'command',
      id: '1',
      command: 'messages',
      stream: true,
    })

    vi.advanceTimersByTime(30_000)

    let settled = false
    void rawPromise.then(() => { settled = true }).catch(() => { settled = true })
    await Promise.resolve()
    expect(settled).toBe(false)

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'done',
        prompt: 'main',
      }),
    })

    await expect(rawPromise).resolves.toBe('done')
  })

  it('streams raw command output chunks before the command completes', async () => {
    const auth = useAuthStore()
    const director = useDirectorStore()
    const chunks = []

    auth.login('admin', 'bareos-dir', 'secret')

    const rawPromise = director.rawCall('messages', {
      onChunk: (chunk) => chunks.push(chunk),
    })
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        director: 'bareos-dir',
      }),
    })

    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'part one\n',
        prompt: 'more',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: 'part two\n',
        prompt: 'main',
      }),
    })

    await expect(rawPromise).resolves.toBe('part one\npart two\n')
    expect(chunks).toEqual(['part one\n', 'part two\n'])
  })

  it('reports tracked raw command lifecycle states including waiting prompts', async () => {
    const auth = useAuthStore()
    const director = useDirectorStore()
    const states = []

    auth.login('admin', 'bareos-dir', 'secret')

    const rawPromise = director.rawCall('label barcodes', {
      onStateChange: (state) => states.push(state),
    })
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        director: 'bareos-dir',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'command_state',
        id: '1',
        status: 'running',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'command_state',
        id: '1',
        status: 'waiting_for_input',
        prompt: 'select',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '1: yes\n2: no\n',
        prompt: 'select',
      }),
    })

    await expect(rawPromise).resolves.toBe('1: yes\n2: no\n')
    expect(states).toEqual([
      { status: 'running', prompt: '', message: '' },
      { status: 'waiting_for_input', prompt: 'select', message: '' },
    ])
  })

  it('treats the raw "other" prompt as a completed command', async () => {
    const auth = useAuthStore()
    const director = useDirectorStore()
    const states = []

    auth.login('admin', 'bareos-dir', 'secret')

    const rawPromise = director.rawCall('mount storage="File" slot=1 drive=0', {
      onStateChange: (state) => states.push(state),
    })
    const socket = FakeWebSocket.instances[0]
    socket.open()
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'auth_ok',
        director: 'bareos-dir',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'command_state',
        id: '1',
        status: 'running',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'command_state',
        id: '1',
        status: 'completed',
        prompt: 'other',
      }),
    })
    socket.onmessage?.({
      data: JSON.stringify({
        type: 'raw_response',
        id: '1',
        text: '',
        prompt: 'other',
      }),
    })

    await expect(rawPromise).resolves.toBe('')
    expect(states).toEqual([
      { status: 'running', prompt: '', message: '' },
      { status: 'completed', prompt: 'other', message: '' },
    ])
  })
})
