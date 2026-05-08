/**
 * Pinia store: Bareos Director WebSocket connection.
 *
 * Protocol:
 *   1. Call connect(credentials) — opens WS and authenticates.
 *   2. Call call(command) — returns a Promise<dict> resolved when the
 *      director responds.  Rejects on timeout (30 s) or WS error.
 *   3. Call disconnect() on logout.
 *
 * The WebSocket URL is read from VITE_DIRECTOR_WS_URL.  When not set the URL
 * is derived from the current page location so that the connection goes
 * through the Apache reverse-proxy at /ws.
 */

import { defineStore } from 'pinia'
import { ref, computed, watch } from 'vue'
import {
  DEFAULT_DIRECTOR_NAME,
  useAuthStore,
} from './auth.js'

function defaultWsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${window.location.host}/ws`
}
const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || defaultWsUrl()
const CMD_TIMEOUT_MS = 30_000
const RAW_CMD_TIMEOUT_MS = 300_000
const DIRECTOR_LIST_TIMEOUT_MS = 5_000
const KEEPALIVE_INTERVAL_MS = 20_000
const RECONNECT_DELAY_MS = 1_000

export const useDirectorStore = defineStore('director', () => {
  const ws       = ref(null)
  // 'disconnected' | 'connecting' | 'authenticating' | 'connected' | 'error'
  const status   = ref('disconnected')
  const errorMsg = ref(null)
  const transport = ref(null)
  const availableDirectors = ref([])
  const availableDirectorsLoaded = ref(false)

  const isConnected = computed(() => status.value === 'connected')

  // Map of command id → { resolve, reject, timer }
  const _pending = new Map()
  let _cmdId = 0
  let _keepaliveTimer = null
  let _reconnectTimer = null
  let _manualDisconnect = false
  let _hasAuthenticated = false
  let _lastCredentials = null
  let _availableDirectorsPromise = null

  // ── internal helpers ────────────────────────────────────────────────────────

  function _send(obj) {
    ws.value?.send(JSON.stringify(obj))
  }

  function _clearKeepalive() {
    if (_keepaliveTimer) {
      clearInterval(_keepaliveTimer)
      _keepaliveTimer = null
    }
  }

  function _startKeepalive() {
    _clearKeepalive()
    _keepaliveTimer = setInterval(() => {
      if (!ws.value || ws.value.readyState !== WebSocket.OPEN) {
        _clearKeepalive()
        return
      }
      _send({ type: 'ping' })
    }, KEEPALIVE_INTERVAL_MS)
  }

  function _clearReconnect() {
    if (_reconnectTimer) {
      clearTimeout(_reconnectTimer)
      _reconnectTimer = null
    }
  }

  function _scheduleReconnect() {
    const auth = useAuthStore()
    const creds = auth.getCredentials() ?? _lastCredentials

    if (!creds?.password || _reconnectTimer || !_hasAuthenticated || _manualDisconnect) {
      return
    }

    _reconnectTimer = setTimeout(() => {
      _reconnectTimer = null
      connect(creds)
    }, RECONNECT_DELAY_MS)
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
      transport.value = msg.transport ?? null
      _hasAuthenticated = true
      _startKeepalive()
      _clearReconnect()
      _manualDisconnect = false
      return
    }

    if (msg.type === 'pong') {
      return
    }

    if (msg.type === 'auth_error') {
      status.value = 'error'
      errorMsg.value = msg.message ?? 'Authentication failed'
      transport.value = null
      _clearKeepalive()
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

  function fetchAvailableDirectors(options = {}) {
    if (!options.forceReload) {
      if (availableDirectorsLoaded.value) {
        return Promise.resolve([...availableDirectors.value])
      }

      if (_availableDirectorsPromise) {
        return _availableDirectorsPromise
      }
    }

    _availableDirectorsPromise = new Promise((resolve, reject) => {
      const sock = new WebSocket(WS_URL)
      let settled = false
      const finish = (handler, value) => {
        if (settled) {
          return
        }
        settled = true
        clearTimeout(timer)
        _availableDirectorsPromise = null
        sock.close()
        handler(value)
      }
      const timer = setTimeout(() => {
        finish(reject, new Error('Timed out while loading directors'))
      }, DIRECTOR_LIST_TIMEOUT_MS)

      sock.onopen = () => {
        sock.send(JSON.stringify({ type: 'list_directors' }))
      }

      sock.onmessage = (event) => {
        let msg
        try { msg = JSON.parse(event.data) } catch {
          finish(reject, new Error('Invalid director list response'))
          return
        }

        if (msg.type !== 'director_list' || !Array.isArray(msg.directors)) {
          finish(reject, new Error(msg.message ?? 'Failed to load directors'))
          return
        }

        availableDirectors.value = [...msg.directors]
        availableDirectorsLoaded.value = true
        finish(resolve, [...availableDirectors.value])
      }

      sock.onerror = () => {
        finish(reject, new Error(`Cannot connect to proxy at ${WS_URL}`))
      }

      sock.onclose = () => {
        clearTimeout(timer)
      }
    })

    return _availableDirectorsPromise
  }

  // ── public API ──────────────────────────────────────────────────────────────

  /**
   * Open a WebSocket connection and authenticate.
   *
   * @param {object} credentials
   * @param {string} credentials.username
   * @param {string} credentials.password
   * @param {string} credentials.director  configured director id
   */
  function connect(credentials) {
    if (ws.value
        && (ws.value.readyState === WebSocket.OPEN
          || ws.value.readyState === WebSocket.CONNECTING
          || status.value === 'authenticating')) {
      return
    }

    _manualDisconnect = false
    _lastCredentials = {
      username: credentials.username,
      password: credentials.password,
      director: credentials.director ?? DEFAULT_DIRECTOR_NAME,
    }
    _clearReconnect()
    _clearKeepalive()
    status.value = 'connecting'
    errorMsg.value = null
    transport.value = null

    const socket = new WebSocket(WS_URL)
    ws.value = socket

    socket.onopen = () => {
      status.value = 'authenticating'
      _send({
        type:     'auth',
        username:  credentials.username,
        password:  credentials.password,
        director:  credentials.director ?? DEFAULT_DIRECTOR_NAME,
      })
    }

    socket.onmessage = _onMessage

    socket.onerror = () => {
      _clearKeepalive()
      status.value = 'error'
      errorMsg.value = `Cannot connect to proxy at ${WS_URL}`
      _rejectAll('WebSocket error')
    }

    socket.onclose = () => {
      _clearKeepalive()
      if (status.value === 'connected' || status.value === 'authenticating') {
        status.value = 'disconnected'
      }
      _rejectAll('WebSocket closed')
      if (!_manualDisconnect) {
        _scheduleReconnect()
      }
    }
  }

  function connectAndWait(credentials, timeoutMs = 8_000) {
    connect(credentials)

    if (status.value === 'connected') {
      return Promise.resolve(true)
    }

    return new Promise((resolve, reject) => {
      let timer = null
      const stop = watch(status, (currentStatus) => {
        if (currentStatus === 'connected') {
          clearTimeout(timer)
          stop()
          resolve(true)
        } else if (currentStatus === 'error') {
          clearTimeout(timer)
          stop()
          reject(new Error(errorMsg.value ?? 'Could not connect to director'))
        }
      }, { immediate: true })

      timer = setTimeout(() => {
        stop()
        reject(new Error(errorMsg.value ?? 'Could not connect to director'))
      }, timeoutMs)
    })
  }

  /** Close the WebSocket and clean up. */
  function disconnect() {
    _manualDisconnect = true
    _clearReconnect()
    _clearKeepalive()
    _rejectAll('Disconnected')
    ws.value?.close()
    ws.value = null
    status.value = 'disconnected'
    errorMsg.value = null
    transport.value = null
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

  /**
   * Run a single command in raw (non-JSON) mode and return the text output.
   * Opens a temporary WebSocket connection, sends the command, and closes.
   *
   * @param {string} command  e.g. "messages"
   * @returns {Promise<string>}
   */
  function rawCall(command, options = {}) {
    return new Promise((resolve, reject) => {
      const auth = useAuthStore()
      const creds = auth.getCredentials()
      if (!creds) { return reject(new Error('Not logged in')) }
      const onChunk = typeof options.onChunk === 'function' ? options.onChunk : null
      const onStateChange = typeof options.onStateChange === 'function'
        ? options.onStateChange
        : null

      const sock = new WebSocket(WS_URL)
      let cmdId = 1
      const timer = setTimeout(() => {
        sock.close()
        reject(new Error(`Raw command timed out: ${command}`))
      }, RAW_CMD_TIMEOUT_MS)
      let lastStateSignature = ''
      const emitStateChange = (nextState) => {
        if (!onStateChange || !nextState?.status) {
          return
        }

        const prompt = typeof nextState.prompt === 'string' ? nextState.prompt : ''
        const message = typeof nextState.message === 'string' ? nextState.message : ''
        const signature = `${nextState.status}\u0000${prompt}\u0000${message}`
        if (signature === lastStateSignature) {
          return
        }

        lastStateSignature = signature
        onStateChange({
          status: nextState.status,
          prompt,
          message,
        })
      }

      sock.onopen = () => {
        sock.send(JSON.stringify({
          type: 'auth', mode: 'raw',
          username: creds.username,
          password: creds.password,
          director: creds.director ?? DEFAULT_DIRECTOR_NAME,
        }))
      }

      let accumulated = ''

      sock.onmessage = (event) => {
        let msg
        try { msg = JSON.parse(event.data) } catch { return }
        if (msg.type === 'auth_ok') {
          emitStateChange({ status: 'running' })
          sock.send(JSON.stringify({
            type: 'command', id: String(cmdId), command, stream: true,
          }))
        } else if (msg.type === 'auth_error') {
          clearTimeout(timer)
          sock.close()
          reject(new Error(msg.message ?? 'Auth failed'))
        } else if (msg.type === 'command_state' && msg.id === String(cmdId)) {
          emitStateChange({
            status: typeof msg.status === 'string' ? msg.status : '',
            prompt: typeof msg.prompt === 'string' ? msg.prompt : '',
            message: typeof msg.message === 'string' ? msg.message : '',
          })
        } else if (msg.type === 'raw_response' && msg.id === String(cmdId)) {
          accumulated += msg.text ?? ''
          onChunk?.(msg.text ?? '', msg)
          if (!msg.prompt || msg.prompt !== 'more') {
            emitStateChange({
              status: msg.prompt === 'main' || msg.prompt === 'other' || !msg.prompt
                ? 'completed'
                : 'waiting_for_input',
              prompt: typeof msg.prompt === 'string' ? msg.prompt : '',
            })
            clearTimeout(timer)
            sock.close()
            resolve(accumulated)
          }
        } else if (msg.type === 'error' && msg.id === String(cmdId)) {
          clearTimeout(timer)
          emitStateChange({
            status: 'failed',
            message: msg.message ?? 'Director error',
          })
          sock.close()
          reject(new Error(msg.message ?? 'Director error'))
        }
      }

      sock.onerror = () => { clearTimeout(timer); reject(new Error(`WS error`)) }
      sock.onclose = () => { clearTimeout(timer) }
    })
  }

  return {
    status,
    errorMsg,
    transport,
    availableDirectors,
    isConnected,
    connect,
    connectAndWait,
    disconnect,
    fetchAvailableDirectors,
    call,
    rawCall,
  }
})
