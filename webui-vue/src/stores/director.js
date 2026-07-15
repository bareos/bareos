/**
 * Pinia store: Bareos Director WebSocket connection.
 *
 * Protocol:
 *   1. Call connect(credentials) — opens WS and sends the session setup.
 *   2. Call call(command) — returns a Promise<dict> resolved when the
 *      director responds.  Rejects on timeout (30 s) or WS error.
 *   3. Call disconnect() on logout.
 *
 * The WebSocket URL is read from VITE_DIRECTOR_WS_URL.  When not set the URL
 * is derived from the current page location so that the connection goes
 * through the Apache reverse-proxy at <app-base>/ws.
 */

import { defineStore } from 'pinia'
import { computed, ref, shallowRef, watch } from 'vue'
import {
  DEFAULT_DIRECTOR_NAME,
  useAuthStore,
} from './auth.js'
import { createDirectorCommandClient } from '../composables/directorAggregate.js'
import {
  createDirectorCommandSession,
  DIRECTOR_COMMAND_TIMEOUT_MS,
  DIRECTOR_WS_URL,
  API_BASE_URL,
} from '../utils/directorCommandSocket.js'

const WS_URL = DIRECTOR_WS_URL
const CMD_TIMEOUT_MS = DIRECTOR_COMMAND_TIMEOUT_MS
const RAW_CMD_TIMEOUT_MS = 300_000
const DIRECTOR_LIST_TIMEOUT_MS = 5_000
const KEEPALIVE_INTERVAL_MS = 20_000
const RECONNECT_DELAY_MS = 1_000

export const useDirectorStore = defineStore('director', () => {
  const ws       = shallowRef(null)
  // 'disconnected' | 'connecting' | 'authenticating' | 'connected' | 'error'
  const status   = ref('disconnected')
  const errorMsg = ref(null)
  const transport = ref(null)
  const availableDirectors = ref([])
  const availableDirectorsLoaded = ref(false)
  const directorConnections = ref({})

  const isConnected = computed(() => status.value === 'connected')

  let _keepaliveTimer = null
  let _reconnectTimer = null
  let _manualDisconnect = false
  let _hasAuthenticated = false
  let _lastCredentials = null
  let _availableDirectorsPromise = null
  let _activeSession = null
  let _currentConnectAttempt = null
  let _directorConnectionsAttempt = null

  // ── internal helpers ────────────────────────────────────────────────────────

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
      _activeSession?.send({ type: 'ping' })
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
      let settled = false
      const finish = (handler, value) => {
        if (settled) {
          return
        }
        settled = true
        clearTimeout(timer)
        _availableDirectorsPromise = null
        handler(value)
      }
      const timer = setTimeout(() => {
        finish(reject, new Error('Timed out while loading directors'))
      }, DIRECTOR_LIST_TIMEOUT_MS)

      fetch(`${API_BASE_URL}/directors`, {
        method: 'GET',
        credentials: 'same-origin',
        cache: 'no-store',
      }).then(async (response) => {
        const payload = await response.json().catch(() => null)
        if (!response.ok || !payload || payload.type !== 'director_list' || !Array.isArray(payload.directors)) {
          finish(reject, new Error(payload?.message ?? 'Failed to load directors'))
          return
        }

        availableDirectors.value = [...payload.directors]
        availableDirectorsLoaded.value = true
        finish(resolve, [...availableDirectors.value])
      }).catch(() => {
        finish(reject, new Error(`Cannot connect to proxy at ${WS_URL}`))
      })
    })

    return _availableDirectorsPromise
  }

  function normaliseDirectorNames(directors) {
    return [...new Set(
      (Array.isArray(directors) ? directors : [])
        .map(value => String(value ?? '').trim())
        .filter(Boolean)
    )]
  }

  function updateDirectorConnection(name, data) {
    directorConnections.value = {
      ...directorConnections.value,
      [name]: {
        ...(directorConnections.value[name] ?? {}),
        director: name,
        ...data,
      },
    }
  }

  async function refreshDirectorConnections(directors) {
    const auth = useAuthStore()
    const credentials = auth.getCredentials()
    const targetDirectors = normaliseDirectorNames(directors)
    const missingDirectors = new Set(auth.missingDirectorSessions(targetDirectors))

    if (!credentials?.password || targetDirectors.length === 0) {
      return {}
    }

    const attemptToken = Symbol('director-connections-refresh')
    _directorConnectionsAttempt = attemptToken

    for (const directorName of targetDirectors) {
      updateDirectorConnection(directorName, {
        status: missingDirectors.has(directorName) ? 'login_required' : 'checking',
        transport: null,
        errorMsg: missingDirectors.has(directorName)
          ? `Please log in to director "${directorName}" first.`
          : null,
        checkedAt: null,
      })
    }

    const authenticatedDirectors = targetDirectors.filter(
      directorName => !missingDirectors.has(directorName)
    )

    const results = await Promise.all(authenticatedDirectors.map(async (directorName) => {
      let client = null

      try {
        client = await createDirectorCommandClient({
          ...credentials,
          director: directorName,
        })

        return {
          director: directorName,
          status: 'connected',
          transport: client.transport ?? null,
          errorMsg: null,
          checkedAt: Date.now(),
        }
      } catch (error) {
        return {
          director: directorName,
          status: 'error',
          transport: null,
          errorMsg: error?.message ?? `Could not connect to ${directorName}`,
          checkedAt: Date.now(),
        }
      } finally {
        client?.disconnect()
      }
    }))

    if (_directorConnectionsAttempt !== attemptToken) {
      return directorConnections.value
    }

    for (const result of results) {
      updateDirectorConnection(result.director, result)
    }

    return directorConnections.value
  }

  // ── public API ──────────────────────────────────────────────────────────────

  /**
   * Open a WebSocket connection for the current authenticated session.
   *
   * @param {object} credentials
   * @param {string} credentials.username
   * @param {string} credentials.password
   * @param {string} credentials.director  configured director id
   */
  function connect(credentials) {
    if (status.value === 'connecting' || status.value === 'authenticating') {
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
    const attemptToken = Symbol('director-connect')
    _currentConnectAttempt = attemptToken

    createDirectorCommandSession(_lastCredentials, {
      wsUrl: WS_URL,
      commandTimeoutMs: CMD_TIMEOUT_MS,
      onOpen: () => {
        if (_currentConnectAttempt === attemptToken) {
          status.value = 'authenticating'
        }
      },
      onAuthOk: (_message, session) => {
        if (_currentConnectAttempt !== attemptToken) {
          session.close('Superseded')
          return
        }
        _activeSession = session
        ws.value = session.socket
        status.value = 'connected'
        errorMsg.value = null
        transport.value = session.transport
        _hasAuthenticated = true
        _startKeepalive()
        _clearReconnect()
        _manualDisconnect = false
      },
      onSocketError: (error, session, { authenticated }) => {
        if (_activeSession?.socket !== session.socket && _currentConnectAttempt !== attemptToken) {
          return
        }
        _clearKeepalive()
        errorMsg.value = error.message
        if (authenticated) {
          status.value = 'error'
        }
      },
      onClose: (session) => {
        if (_activeSession?.socket !== session.socket && _currentConnectAttempt !== attemptToken) {
          return
        }
        _clearKeepalive()
        _activeSession = null
        if (_currentConnectAttempt === attemptToken) {
          _currentConnectAttempt = null
        }
        ws.value = null
        if (status.value === 'connected' || status.value === 'authenticating') {
          status.value = 'disconnected'
        }
        if (!_manualDisconnect) {
          _scheduleReconnect()
        }
      },
    }).then((session) => {
      if (_currentConnectAttempt !== attemptToken) {
        session.close('Superseded')
      }
    }).catch((error) => {
      if (_currentConnectAttempt !== attemptToken) {
        return
      }
      _currentConnectAttempt = null
      _activeSession = null
      if (status.value !== 'connected') {
        ws.value = null
        status.value = 'error'
        errorMsg.value = error.message
        transport.value = null
      }
    })
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
    _currentConnectAttempt = null
    _activeSession?.close('Disconnected')
    _activeSession = null
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
    return _activeSession?.call(command) ?? Promise.reject(new Error('Not connected to director'))
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
          type: 'session',
          mode: 'raw',
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
    directorConnections,
    isConnected,
    connect,
    connectAndWait,
    disconnect,
    fetchAvailableDirectors,
    refreshDirectorConnections,
    call,
    rawCall,
  }
})
