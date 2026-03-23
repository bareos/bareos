/**
 * Pinia store: Bareos Director WebSocket connection.
 *
 * Protocol:
 *   1. Call connect(credentials) — opens WS and authenticates.
 *   2. Call call(command) — returns a Promise<dict> resolved when the
 *      director responds.  Rejects on timeout (30 s) or WS error.
 *   3. Call disconnect() on logout.
 *
 * The WebSocket URL is read from VITE_DIRECTOR_WS_URL (default ws://localhost:8765).
 */

import { defineStore } from 'pinia'
import { ref, computed } from 'vue'

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || 'ws://localhost:8765'
const CMD_TIMEOUT_MS = 30_000

export const useDirectorStore = defineStore('director', () => {
  const ws       = ref(null)
  // 'disconnected' | 'connecting' | 'authenticating' | 'connected' | 'error'
  const status   = ref('disconnected')
  const errorMsg = ref(null)

  const isConnected = computed(() => status.value === 'connected')

  // Map of command id → { resolve, reject, timer }
  const _pending = new Map()
  let _cmdId = 0

  // ── internal helpers ────────────────────────────────────────────────────────

  function _send(obj) {
    ws.value?.send(JSON.stringify(obj))
  }

  function _rejectAll(reason) {
    for (const { reject, timer } of _pending.values()) {
      clearTimeout(timer)
      reject(new Error(reason))
    }
    _pending.clear()
  }

  function _onMessage(event) {
    let msg
    try { msg = JSON.parse(event.data) } catch { return }

    if (msg.type === 'auth_ok') {
      status.value = 'connected'
      errorMsg.value = null
      return
    }

    if (msg.type === 'auth_error') {
      status.value = 'error'
      errorMsg.value = msg.message ?? 'Authentication failed'
      ws.value?.close()
      return
    }

    if (msg.type === 'response' || msg.type === 'error') {
      const entry = _pending.get(msg.id)
      if (!entry) return
      clearTimeout(entry.timer)
      _pending.delete(msg.id)
      if (msg.type === 'error') {
        entry.reject(new Error(msg.message ?? 'Director error'))
      } else {
        entry.resolve(msg.data)
      }
    }
  }

  // ── public API ──────────────────────────────────────────────────────────────

  /**
   * Open a WebSocket connection and authenticate.
   *
   * @param {object} credentials
   * @param {string} credentials.username
   * @param {string} credentials.password
   * @param {string} credentials.director  director resource name
   * @param {string} [credentials.host]    director host (default: localhost)
   * @param {number} [credentials.port]    director port (default: 9101)
   */
  function connect(credentials) {
    if (ws.value && ws.value.readyState === WebSocket.OPEN) return

    status.value = 'connecting'
    errorMsg.value = null

    const socket = new WebSocket(WS_URL)
    ws.value = socket

    socket.onopen = () => {
      status.value = 'authenticating'
      _send({
        type:     'auth',
        username:  credentials.username,
        password:  credentials.password,
        director:  credentials.director,
        host:      credentials.host  ?? 'localhost',
        port:      credentials.port  ?? 9101,
      })
    }

    socket.onmessage = _onMessage

    socket.onerror = () => {
      status.value = 'error'
      errorMsg.value = `Cannot connect to proxy at ${WS_URL}`
      _rejectAll('WebSocket error')
    }

    socket.onclose = () => {
      if (status.value === 'connected' || status.value === 'authenticating') {
        status.value = 'disconnected'
      }
      _rejectAll('WebSocket closed')
    }
  }

  /** Close the WebSocket and clean up. */
  function disconnect() {
    _rejectAll('Disconnected')
    ws.value?.close()
    ws.value = null
    status.value = 'disconnected'
    errorMsg.value = null
  }

  /**
   * Send a director command and return a Promise that resolves with the
   * result dict, or rejects with an Error.
   *
   * @param {string} command  e.g. "list jobs"
   * @returns {Promise<object>}
   */
  function call(command) {
    return new Promise((resolve, reject) => {
      if (!ws.value || ws.value.readyState !== WebSocket.OPEN) {
        return reject(new Error('Not connected to director'))
      }

      const id = String(++_cmdId)

      const timer = setTimeout(() => {
        if (_pending.has(id)) {
          _pending.delete(id)
          reject(new Error(`Command timed out: ${command}`))
        }
      }, CMD_TIMEOUT_MS)

      _pending.set(id, { resolve, reject, timer })
      _send({ type: 'command', id, command })
    })
  }

  return { status, errorMsg, isConnected, connect, disconnect, call }
})
