<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:960px">
      <q-card-section class="panel-header row items-center">
        <span>Bareos Console</span>
        <q-space />
        <q-chip dense square :color="statusColor" text-color="white" :label="consoleStatus" class="q-mr-sm" style="font-size:0.72rem" />
        <q-btn flat round dense icon="delete_sweep" color="white" title="Clear" @click="clearOutput" />
      </q-card-section>

      <!-- terminal output area -->
      <q-card-section class="q-pa-none">
        <div class="console-output" ref="outputEl">
          <div v-for="(line, i) in output" :key="i" :class="['console-line', line.cls]">{{ line.text }}</div>
          <div class="console-line console-cursor">█</div>
        </div>
      </q-card-section>

      <!-- input row -->
      <q-card-section class="q-pt-sm q-pb-sm">
        <div class="row q-col-gutter-sm items-center">
          <div class="col">
            <q-input
              v-model="cmd"
              outlined dense
              placeholder="Enter command…"
              :disable="consoleStatus !== 'connected'"
              @keydown="onKeyDown"
              class="console-input"
              ref="inputEl"
            >
              <template #prepend><span class="text-positive text-weight-bold" style="font-family:monospace">*</span></template>
            </q-input>
          </div>
          <div class="col-auto">
            <q-btn color="primary" label="Send" icon="send" no-caps
              :disable="consoleStatus !== 'connected'"
              @click="send" />
          </div>
        </div>

        <!-- quick commands -->
        <div class="row q-gutter-xs q-mt-xs flex-wrap">
          <q-chip v-for="c in quickCmds" :key="c" clickable
            @click="quickSend(c)"
            color="grey-3" text-color="dark" size="sm"
            :disable="consoleStatus !== 'connected'">{{ c }}</q-chip>
        </div>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick, watch } from 'vue'
import { useAuthStore }     from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'

const auth     = useAuthStore()
const director = useDirectorStore()

// ── refs ─────────────────────────────────────────────────────────────────────
const outputEl = ref(null)
const inputEl  = ref(null)
const cmd      = ref('')
const output   = ref([])

// ── command history ───────────────────────────────────────────────────────────
const history    = ref([])
let   historyIdx = -1            // -1 = not browsing history

// ── raw WebSocket state ───────────────────────────────────────────────────────
let   rawWs       = null
let   cmdSeq      = 0
const pendingCmds = new Map()    // id → { resolve, reject, timer }
const consoleStatus = ref('disconnected')  // 'connecting'|'connected'|'error'|'disconnected'

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || 'ws://localhost:8765'

const statusColor = computed(() => ({
  connected: 'positive', connecting: 'warning',
  error: 'negative', disconnected: 'grey',
}[consoleStatus.value] ?? 'grey'))

const quickCmds = ['status director', 'list jobs', 'list clients', 'list volumes', 'list pools', 'messages', 'help', 'version']

// ── output helpers ────────────────────────────────────────────────────────────
function appendLines(text, cls = '') {
  const lines = String(text ?? '').replace(/\r\n/g, '\n').split('\n')
  // trim trailing blank line that most director responses end with
  while (lines.length && lines[lines.length - 1].trim() === '') lines.pop()
  lines.forEach(l => output.value.push({ text: l, cls }))
}

function appendInfo(text)  { appendLines(text, 'console-info') }
function appendCmd(text)   { output.value.push({ text: `* ${text}`, cls: 'console-cmd' }) }
function appendErr(text)   { appendLines(text, 'console-err') }

async function scrollBottom() {
  await nextTick()
  if (outputEl.value) outputEl.value.scrollTop = outputEl.value.scrollHeight
}

function clearOutput() {
  output.value = []
  appendInfo('Console cleared.')
}

// ── raw WebSocket ─────────────────────────────────────────────────────────────
function rejectAll(reason) {
  for (const { timer, reject } of pendingCmds.values()) {
    clearTimeout(timer)
    reject(new Error(reason))
  }
  pendingCmds.clear()
}

function connectRaw() {
  if (rawWs && (rawWs.readyState === WebSocket.CONNECTING || rawWs.readyState === WebSocket.OPEN)) return

  const creds = auth.getCredentials()
  if (!creds) {
    appendErr('Not logged in — cannot open console.')
    return
  }

  consoleStatus.value = 'connecting'
  appendInfo('Connecting to director…')

  rawWs = new WebSocket(WS_URL)

  rawWs.onopen = () => {
    rawWs.send(JSON.stringify({
      type: 'auth', mode: 'raw',
      username: creds.username,
      password: creds.password,
      director: creds.director,
      host:     creds.host ?? 'localhost',
      port:     creds.port ?? 9101,
    }))
  }

  rawWs.onmessage = (event) => {
    let msg
    try { msg = JSON.parse(event.data) } catch { return }

    if (msg.type === 'auth_ok') {
      consoleStatus.value = 'connected'
      appendInfo(`Connected to ${msg.director} — type 'help' for a list of commands.`)
      scrollBottom()
      return
    }
    if (msg.type === 'auth_error') {
      consoleStatus.value = 'error'
      appendErr(`Authentication error: ${msg.message}`)
      scrollBottom()
      return
    }
    if (msg.type === 'raw_response') {
      const entry = pendingCmds.get(msg.id)
      if (entry) { clearTimeout(entry.timer); pendingCmds.delete(msg.id) }
      appendLines(msg.text)
      scrollBottom()
      return
    }
    if (msg.type === 'error') {
      const entry = pendingCmds.get(msg.id)
      if (entry) { clearTimeout(entry.timer); pendingCmds.delete(msg.id) }
      appendErr(msg.message)
      scrollBottom()
    }
  }

  rawWs.onerror = () => {
    consoleStatus.value = 'error'
    appendErr(`Cannot connect to proxy at ${WS_URL}`)
    scrollBottom()
    rejectAll('WebSocket error')
  }

  rawWs.onclose = () => {
    if (consoleStatus.value !== 'disconnected') {
      consoleStatus.value = 'disconnected'
      appendInfo('Console disconnected.')
      scrollBottom()
    }
    rejectAll('WebSocket closed')
  }
}

function disconnectRaw() {
  rejectAll('Disconnected')
  rawWs?.close()
  rawWs = null
  consoleStatus.value = 'disconnected'
}

// ── send / keyboard ───────────────────────────────────────────────────────────
function send() {
  const c = cmd.value.trim()
  if (!c) return

  // history dedup
  if (!history.value.length || history.value[history.value.length - 1] !== c) {
    history.value.push(c)
  }
  historyIdx = history.value.length   // reset browsing position
  cmd.value  = ''

  appendCmd(c)

  if (!rawWs || rawWs.readyState !== WebSocket.OPEN) {
    appendErr('Not connected to director.')
    scrollBottom()
    return
  }

  const id = String(++cmdSeq)
  const timer = setTimeout(() => {
    if (pendingCmds.has(id)) {
      pendingCmds.delete(id)
      appendErr('Command timed out.')
      scrollBottom()
    }
  }, 30_000)
  pendingCmds.set(id, { timer })
  rawWs.send(JSON.stringify({ type: 'command', id, command: c }))
  scrollBottom()
}

function quickSend(c) { cmd.value = c; send() }

function onKeyDown(event) {
  if (event.key === 'Enter') {
    event.preventDefault()
    send()
  } else if (event.key === 'ArrowUp') {
    event.preventDefault()
    if (history.value.length === 0) return
    if (historyIdx > 0) historyIdx--
    cmd.value = history.value[historyIdx] ?? ''
  } else if (event.key === 'ArrowDown') {
    event.preventDefault()
    if (historyIdx < history.value.length - 1) {
      historyIdx++
      cmd.value = history.value[historyIdx]
    } else {
      historyIdx = history.value.length
      cmd.value  = ''
    }
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
onMounted(() => {
  appendInfo('Bareos WebUI Console  —  use ↑/↓ for command history')
  connectRaw()
})

onUnmounted(disconnectRaw)

// reconnect when the main director reconnects (e.g. page reload)
watch(() => director.isConnected, (connected) => {
  if (connected && consoleStatus.value === 'disconnected') connectRaw()
})
</script>

<style scoped>
.console-output {
  background: #1a1a1a;
  color: #e0e0e0;
  font-family: 'Courier New', Courier, monospace;
  font-size: 0.82rem;
  line-height: 1.45;
  min-height: 360px;
  max-height: 520px;
  overflow-y: auto;
  padding: 12px 16px;
  border-bottom: 1px solid #333;
  white-space: pre-wrap;
  word-break: break-all;
}
.console-line { display: block; }
.console-info  { color: #90caf9; }
.console-cmd   { color: #80cbc4; }
.console-err   { color: #ef9a9a; }
.console-cursor { display: inline; animation: blink 1s step-end infinite; }
@keyframes blink { 50% { opacity: 0; } }
.console-input :deep(input) {
  font-family: 'Courier New', monospace;
  font-size: 0.85rem;
}
</style>


