import { ref, onUnmounted } from 'vue'

let socket = null
let listeners = []

function getSocket() {
  if (socket && socket.readyState <= WebSocket.OPEN) return socket
  const proto = location.protocol === 'https:' ? 'wss' : 'ws'
  socket = new WebSocket(`${proto}://${location.host}/ws`)
  socket.onmessage = (ev) => {
    let msg
    try { msg = JSON.parse(ev.data) } catch { return }
    listeners.forEach(fn => fn(msg))
  }
  socket.onclose = () => {
    socket = null
    listeners.forEach(fn => fn({ type: 'ws_closed' }))
  }
  return socket
}

export function useSetupWs() {
  const connected = ref(false)
  const messages  = ref([])

  function onMsg(msg) {
    if (msg.type === 'ws_closed') {
      connected.value = false
    } else {
      connected.value = true
      messages.value.push(msg)
    }
  }

  listeners.push(onMsg)

  // Kick the connection
  const ws = getSocket()
  ws.addEventListener('open', () => { connected.value = true })

  onUnmounted(() => {
    listeners = listeners.filter(fn => fn !== onMsg)
  })

  function send(payload) {
    const ws = getSocket()
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(payload))
    } else {
      ws.addEventListener('open', () => ws.send(JSON.stringify(payload)), { once: true })
    }
  }

  function clearMessages() {
    messages.value = []
  }

  return { connected, messages, send, clearMessages }
}
