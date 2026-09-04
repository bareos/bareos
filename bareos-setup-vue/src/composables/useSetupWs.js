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
import { ref, onUnmounted } from 'vue'

let socket
const listeners = new Set()

function connect() {
  if (socket && socket.readyState < WebSocket.CLOSING) return socket
  const token = new URLSearchParams(location.search).get('token') || ''
  const scheme = location.protocol === 'https:' ? 'wss' : 'ws'
  socket = new WebSocket(`${scheme}://${location.host}/ws?token=${encodeURIComponent(token)}`)
  socket.onmessage = event => {
    try { listeners.forEach(listener => listener(JSON.parse(event.data))) } catch {}
  }
  socket.onclose = () => {
    listeners.forEach(listener => listener({ type: 'ws_closed' }))
    socket = undefined
  }
  return socket
}

export function useSetupWs() {
  const connected = ref(false)
  const messages = ref([])
  const receive = message => {
    connected.value = message.type !== 'ws_closed'
    if (message.type !== 'ws_closed') messages.value.push(message)
  }
  listeners.add(receive)
  const ws = connect()
  ws.addEventListener('open', () => {
    connected.value = true
    ws.send(JSON.stringify({ action: 'state' }))
  }, { once: true })
  onUnmounted(() => listeners.delete(receive))
  function send(payload) {
    const current = connect()
    const sendNow = () => current.send(JSON.stringify(payload))
    current.readyState === WebSocket.OPEN ? sendNow() : current.addEventListener('open', sendNow, { once: true })
  }
  function clearMessages() {
    messages.value = []
  }
  return { connected, messages, clearMessages, send }
}
