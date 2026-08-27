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
  return { connected, messages, send }
}
