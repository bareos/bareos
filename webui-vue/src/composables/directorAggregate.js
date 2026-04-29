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

import {
  directorCollection,
  normaliseJob,
} from './useDirectorFetch.js'

function defaultWsUrl() {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${window.location.host}/ws`
}

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || defaultWsUrl()
const AUTH_TIMEOUT_MS = 8_000
const CMD_TIMEOUT_MS = 30_000

function rejectPendingCommands(pendingCommands, reason) {
  for (const { reject, timer } of pendingCommands.values()) {
    clearTimeout(timer)
    reject(new Error(reason))
  }
  pendingCommands.clear()
}

function createDirectorCommandClient(credentials, options = {}) {
  const wsUrl = options.wsUrl ?? WS_URL
  const authTimeoutMs = options.authTimeoutMs ?? AUTH_TIMEOUT_MS
  const commandTimeoutMs = options.commandTimeoutMs ?? CMD_TIMEOUT_MS

  return new Promise((resolve, reject) => {
    const socket = new WebSocket(wsUrl)
    const pendingCommands = new Map()
    let authenticated = false
    let failed = false
    let commandId = 0
    let transport = null

    const authTimer = setTimeout(() => {
      fail(new Error(`Timed out while connecting to ${credentials.director}`))
    }, authTimeoutMs)

    function cleanup() {
      clearTimeout(authTimer)
    }

    function disconnect() {
      cleanup()
      rejectPendingCommands(pendingCommands, 'Disconnected')
      if (socket.readyState === WebSocket.OPEN
        || socket.readyState === WebSocket.CONNECTING) {
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
      if (socket.readyState === WebSocket.OPEN
        || socket.readyState === WebSocket.CONNECTING) {
        socket.close()
      }

      if (!authenticated) {
        reject(error)
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

    socket.onopen = () => {
      socket.send(JSON.stringify({
        type: 'auth',
        username: credentials.username,
        password: credentials.password,
        director: credentials.director,
      }))
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
        transport = message.transport ?? null
        cleanup()
        resolve({
          director: credentials.director,
          transport,
          call,
          disconnect,
        })
        return
      }

      if (message.type === 'auth_error') {
        fail(new Error(message.message ?? `Authentication failed for ${credentials.director}`))
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
      }
    }

    socket.onerror = () => {
      fail(new Error(`Cannot connect to proxy at ${wsUrl}`))
    }

    socket.onclose = () => {
      cleanup()
      if (!failed && !authenticated) {
        reject(new Error(`Disconnected from ${credentials.director}`))
      }
      rejectPendingCommands(pendingCommands, 'WebSocket closed')
    }
  })
}

function decorateJobs(entries, director) {
  return directorCollection(entries).map((entry) => {
    const job = normaliseJob(entry)
    return {
      ...job,
      director,
      scopeKey: `${director}:${job.id}`,
    }
  })
}

function numberValue(value) {
  const number = Number(value ?? 0)
  return Number.isFinite(number) ? number : 0
}

function sortJobsByStartTime(jobs) {
  return [...jobs].sort((left, right) => {
    const startCompare = String(right.starttime ?? '').localeCompare(String(left.starttime ?? ''))
    if (startCompare !== 0) {
      return startCompare
    }

    const directorCompare = String(left.director ?? '').localeCompare(String(right.director ?? ''))
    if (directorCompare !== 0) {
      return directorCompare
    }

    return numberValue(right.id) - numberValue(left.id)
  })
}

export async function fetchDirectorDashboardSnapshot(credentials, options = {}) {
  const client = await createDirectorCommandClient(credentials, options)

  try {
    const [
      past24hResult,
      runningResult,
      lastResult,
      totalsResult,
      clientsResult,
      storagesResult,
    ] = await Promise.allSettled([
      client.call('llist jobs days=1'),
      client.call('list jobs jobstatus=R'),
      client.call('llist jobs last'),
      client.call('list jobtotals'),
      client.call('list clients'),
      client.call('list storages'),
    ])

    return {
      director: credentials.director,
      transport: client.transport,
      jobsPast24h: past24hResult.status === 'fulfilled'
        ? decorateJobs(past24hResult.value?.jobs, credentials.director)
        : [],
      runningJobs: runningResult.status === 'fulfilled'
        ? decorateJobs(runningResult.value?.jobs, credentials.director)
        : [],
      recentJobs: lastResult.status === 'fulfilled'
        ? decorateJobs(lastResult.value?.jobs, credentials.director)
        : [],
      jobTotals: totalsResult.status === 'fulfilled'
        ? {
          jobs: numberValue(totalsResult.value?.jobtotals?.jobs),
          files: numberValue(totalsResult.value?.jobtotals?.files),
          bytes: numberValue(totalsResult.value?.jobtotals?.bytes),
        }
        : {
          jobs: 0,
          files: 0,
          bytes: 0,
        },
      clientCount: clientsResult.status === 'fulfilled'
        ? directorCollection(clientsResult.value?.clients).length
        : 0,
      storageCount: storagesResult.status === 'fulfilled'
        ? directorCollection(storagesResult.value?.storages).length
        : 0,
    }
  } finally {
    client.disconnect()
  }
}

export function aggregateDirectorDashboardSnapshots(snapshots) {
  return snapshots.reduce((aggregate, snapshot) => ({
    jobsPast24h: sortJobsByStartTime([
      ...aggregate.jobsPast24h,
      ...(snapshot.jobsPast24h ?? []),
    ]),
    runningJobs: sortJobsByStartTime([
      ...aggregate.runningJobs,
      ...(snapshot.runningJobs ?? []),
    ]),
    recentJobs: sortJobsByStartTime([
      ...aggregate.recentJobs,
      ...(snapshot.recentJobs ?? []),
    ]),
    clientCount: aggregate.clientCount + numberValue(snapshot.clientCount),
    storageCount: aggregate.storageCount + numberValue(snapshot.storageCount),
    jobTotals: {
      jobs: aggregate.jobTotals.jobs + numberValue(snapshot.jobTotals?.jobs),
      files: aggregate.jobTotals.files + numberValue(snapshot.jobTotals?.files),
      bytes: aggregate.jobTotals.bytes + numberValue(snapshot.jobTotals?.bytes),
    },
  }), {
    jobsPast24h: [],
    runningJobs: [],
    recentJobs: [],
    clientCount: 0,
    storageCount: 0,
    jobTotals: {
      jobs: 0,
      files: 0,
      bytes: 0,
    },
  })
}
