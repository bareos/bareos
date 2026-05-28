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

export function defaultDirectorWsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${window.location.host}/ws`
}

export const DIRECTOR_WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || defaultDirectorWsUrl()
export const DIRECTOR_COMMAND_AUTH_TIMEOUT_MS = 8_000
export const DIRECTOR_COMMAND_TIMEOUT_MS = 30_000

function rejectPendingCommands(pendingCommands, reason) {
  for (const { reject, timer } of pendingCommands.values()) {
    clearTimeout(timer)
    reject(new Error(reason))
  }
  pendingCommands.clear()
}

export function createDirectorCommandSession(credentials, options = {}) {
  const wsUrl = options.wsUrl ?? DIRECTOR_WS_URL
  const authTimeoutMs = options.authTimeoutMs ?? DIRECTOR_COMMAND_AUTH_TIMEOUT_MS
  const commandTimeoutMs = options.commandTimeoutMs ?? DIRECTOR_COMMAND_TIMEOUT_MS
  const connectErrorMessage = options.connectErrorMessage ?? `Cannot connect to proxy at ${wsUrl}`
  const disconnectErrorMessage = options.disconnectErrorMessage
    ?? `Disconnected from ${credentials.director}`
  const authErrorMessage = options.authErrorMessage
    ?? `Authentication failed for ${credentials.director}`

  return new Promise((resolve, reject) => {
    const socket = new WebSocket(wsUrl)
    const pendingCommands = new Map()
    let commandId = 0
    let authenticated = false
    let failed = false
    let settled = false

    function settleReject(error) {
      if (settled) {
        return
      }
      settled = true
      reject(error)
    }

    function settleResolve(session) {
      if (settled) {
        return
      }
      settled = true
      resolve(session)
    }

    function cleanup() {
      clearTimeout(authTimer)
    }

    function closeSocket() {
      if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
        socket.close()
      }
    }

    function fail(error) {
      if (failed) {
        return
      }

      failed = true
      cleanup()
      rejectPendingCommands(pendingCommands, error.message)
      closeSocket()

      if (!authenticated) {
        settleReject(error)
      }
    }

    function call(command) {
      return new Promise((resolveCommand, rejectCommand) => {
        if (socket.readyState !== WebSocket.OPEN || !authenticated) {
          rejectCommand(new Error('Not connected to director'))
          return
        }

        const id = String(++commandId)
        const timer = setTimeout(() => {
          if (!pendingCommands.has(id)) {
            return
          }

          pendingCommands.delete(id)
          rejectCommand(new Error(`Command timed out: ${command}`))
        }, commandTimeoutMs)

        pendingCommands.set(id, {
          resolve: resolveCommand,
          reject: rejectCommand,
          timer,
        })

        socket.send(JSON.stringify({ type: 'command', id, command }))
      })
    }

    const session = {
      socket,
      director: credentials.director,
      transport: null,
      call,
      close(reason = 'Disconnected') {
        cleanup()
        rejectPendingCommands(pendingCommands, reason)
        closeSocket()
      },
      rejectPending(reason) {
        rejectPendingCommands(pendingCommands, reason)
      },
      send(payload) {
        socket.send(JSON.stringify(payload))
      },
    }

    const authTimer = setTimeout(() => {
      fail(new Error(`Timed out while connecting to ${credentials.director}`))
    }, authTimeoutMs)

    socket.onopen = () => {
      options.onOpen?.(session)
      session.send({
        type: 'auth',
        username: credentials.username,
        password: credentials.password,
        director: credentials.director,
      })
    }

    socket.onmessage = (event) => {
      let message
      try {
        message = JSON.parse(event.data)
      } catch {
        return
      }

      if (message.type === 'auth_ok') {
        authenticated = true
        session.transport = message.transport ?? null
        cleanup()
        options.onAuthOk?.(message, session)
        settleResolve(session)
        return
      }

      if (message.type === 'pong') {
        options.onPong?.(message, session)
        return
      }

      if (message.type === 'auth_error') {
        fail(new Error(message.message ?? authErrorMessage))
        return
      }

      if (message.type === 'response' || message.type === 'error') {
        const pending = pendingCommands.get(message.id)
        if (!pending) {
          return
        }

        clearTimeout(pending.timer)
        pendingCommands.delete(message.id)

        if (message.type === 'error') {
          pending.reject(new Error(message.message ?? 'Director error'))
        } else {
          pending.resolve(message.data)
        }
        return
      }

      options.onMessage?.(message, session)
    }

    socket.onerror = () => {
      const error = new Error(connectErrorMessage)
      options.onSocketError?.(error, session, { authenticated })
      if (!authenticated) {
        fail(error)
        return
      }
      rejectPendingCommands(pendingCommands, 'WebSocket error')
    }

    socket.onclose = () => {
      cleanup()
      options.onClose?.(session, { authenticated, failed })
      if (!failed && !authenticated) {
        settleReject(new Error(disconnectErrorMessage))
      }
      rejectPendingCommands(pendingCommands, 'WebSocket closed')
    }
  })
}
