<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" :style="isPopup ? 'max-width:100%' : 'max-width:960px'">
      <q-card-section class="panel-header row items-center">
        <span>Bareos Console</span>
        <q-space />
        <q-chip dense square :color="statusColor" text-color="white" :label="consoleStatus" class="q-mr-sm" style="font-size:0.72rem" />
        <q-btn v-if="!isPopup" flat round dense icon="open_in_new" color="white" title="Open in new window" @click="popOut" />
        <q-btn v-if="isPopup"  flat round dense icon="close"       color="white" title="Close window"       @click="closePopup" />
        <q-btn flat round dense icon="delete_sweep" color="white" title="Clear" @click="clearOutput" />
      </q-card-section>

      <!-- terminal area — click to focus, then type -->
      <div
        class="console-output"
        :class="{ 'console-output-popup': isPopup }"
        ref="outputEl"
        tabindex="0"
        @keydown="onKeyDown"
        @focus="focused = true"
        @blur="focused = false"
        @click="focusConsole"
      >
        <div v-for="(line, i) in output" :key="i" :class="['console-line', line.cls]">{{ line.text }}</div>

        <!-- live input line -->
        <div class="console-line console-input-line">
          <span class="console-prompt">{{ currentPrompt }}</span>
          <span>{{ cmd }}</span><span :class="['console-cursor', { blink: focused }]">█</span>
        </div>
      </div>

      <!-- quick command chips -->
      <q-card-section class="q-pt-xs q-pb-sm">
        <div class="row q-gutter-xs flex-wrap">
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
import { useRoute }          from 'vue-router'
import { useAuthStore }     from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'

const auth     = useAuthStore()
const director = useDirectorStore()
const route    = useRoute()

const isPopup = computed(() => route.name === 'console-popup')

function popOut() {
  const base = window.location.href.replace(/#.*$/, '')
  window.open(
    base + '#/console-popup',
    'bareos-console',
    'width=960,height=720,resizable=yes,scrollbars=no'
  )
}

function closePopup() {
  window.close()
}

// ── refs ─────────────────────────────────────────────────────────────────────
const outputEl = ref(null)
const cmd      = ref('')
const focused  = ref(false)
const output   = ref([])

// ── command history ───────────────────────────────────────────────────────────
const history    = ref([])
let   historyIdx = -1

// ── raw WebSocket state ───────────────────────────────────────────────────────
let   rawWs       = null
let   cmdSeq      = 0
const pendingCmds  = new Map()
const completionIds = new Set()   // IDs of in-flight completion (Tab) requests
const consoleStatus = ref('disconnected')
const currentPrompt = ref('* ')  // updated from raw_response.prompt

const WS_URL = import.meta.env.VITE_DIRECTOR_WS_URL || 'ws://localhost:8765'

const statusColor = computed(() => ({
  connected: 'positive', connecting: 'warning',
  error: 'negative', disconnected: 'grey',
}[consoleStatus.value] ?? 'grey'))

const quickCmds = ['status director', 'list jobs', 'list clients', 'list volumes', 'list pools', 'messages', 'help', 'version']

// ── output helpers ────────────────────────────────────────────────────────────
function appendLines(text, cls = '') {
  const lines = String(text ?? '').replace(/\r\n/g, '\n').split('\n')
  while (lines.length && lines[lines.length - 1].trim() === '') lines.pop()
  lines.forEach(l => output.value.push({ text: l, cls }))
}

function appendInfo(text) { appendLines(text, 'console-info') }
function appendErr(text)  { appendLines(text, 'console-err') }

async function scrollBottom() {
  await nextTick()
  if (outputEl.value) outputEl.value.scrollTop = outputEl.value.scrollHeight
}

function clearOutput() {
  output.value = []
  appendInfo('Console cleared.')
}

function focusConsole() {
  outputEl.value?.focus()
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
      appendInfo(`Connected to ${msg.director} — type 'help' for commands, click here to type.`)
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

      if (completionIds.delete(msg.id)) {
        // Tab-completion response: single line → update cmd; multiple → display
        const lines = String(msg.text ?? '').replace(/\r\n/g, '\n').split('\n').filter(l => l.trim())
        if (lines.length === 1) {
          cmd.value = lines[0].trim()
        } else if (lines.length > 1) {
          appendLines(msg.text)
        }
        // empty → no completions, do nothing
      } else {
        appendLines(msg.text)
        currentPrompt.value = msg.prompt === 'select' ? 'Select: '
                            : msg.prompt === 'sub'    ? '> '
                            : '* '
      }
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

  if (c && (!history.value.length || history.value[history.value.length - 1] !== c)) {
    history.value.push(c)
    historyIdx = history.value.length
  }

  // echo the command as a completed line (even if empty — shows the prompt)
  output.value.push({ text: `${currentPrompt.value}${c}`, cls: 'console-cmd' })
  cmd.value = ''

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

function quickSend(c) {
  output.value.push({ text: `${currentPrompt.value}${c}`, cls: 'console-cmd' })
  cmd.value = ''
  if (!rawWs || rawWs.readyState !== WebSocket.OPEN) {
    appendErr('Not connected to director.')
    scrollBottom()
    return
  }
  const id = String(++cmdSeq)
  const timer = setTimeout(() => {
    pendingCmds.has(id) && (pendingCmds.delete(id), appendErr('Command timed out.'), scrollBottom())
  }, 30_000)
  pendingCmds.set(id, { timer })
  rawWs.send(JSON.stringify({ type: 'command', id, command: c }))
  scrollBottom()
  focusConsole()
}

function sendTab() {
  if (!rawWs || rawWs.readyState !== WebSocket.OPEN) return
  const id = String(++cmdSeq)
  const timer = setTimeout(() => {
    completionIds.delete(id)
    pendingCmds.delete(id)
  }, 5_000)
  completionIds.add(id)
  pendingCmds.set(id, { timer })
  rawWs.send(JSON.stringify({ type: 'command', id, command: cmd.value + '\t' }))
}

function onKeyDown(event) {
  // Don't interfere with browser shortcuts (Ctrl+R, Ctrl+T, etc.)
  if (event.ctrlKey && event.key !== 'c' && event.key !== 'l') return

  if (event.key === 'Tab') {
    event.preventDefault()
    sendTab()
  } else if (event.key === 'Enter') {
    event.preventDefault()
    send()
  } else if (event.key === 'Backspace') {
    event.preventDefault()
    cmd.value = cmd.value.slice(0, -1)
  } else if (event.key === 'Delete') {
    event.preventDefault()
    cmd.value = ''
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
  } else if (event.ctrlKey && event.key === 'c') {
    event.preventDefault()
    output.value.push({ text: `${currentPrompt.value}${cmd.value}^C`, cls: 'console-cmd' })
    cmd.value = ''
  } else if (event.ctrlKey && event.key === 'l') {
    event.preventDefault()
    clearOutput()
  } else if (event.key.length === 1 && !event.altKey) {
    // printable character
    event.preventDefault()
    cmd.value += event.key
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
onMounted(() => {
  appendInfo('Bareos WebUI Console  —  click here to type, ↑/↓ for history, Ctrl+L to clear')
  connectRaw()
})

onUnmounted(disconnectRaw)

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
  line-height: 1.5;
  min-height: 420px;
  max-height: 600px;
  overflow-y: auto;
  padding: 12px 16px 12px;
  white-space: pre-wrap;
  word-break: break-all;
  outline: none;
  cursor: text;
}
.console-output-popup {
  min-height: calc(100vh - 140px);
  max-height: calc(100vh - 140px);
}
.console-output:focus {
  box-shadow: inset 0 0 0 2px #1976d2;
}
.console-line         { display: block; }
.console-input-line   { display: block; }
.console-info         { color: #90caf9; }
.console-cmd          { color: #80cbc4; }
.console-err          { color: #ef9a9a; }
.console-prompt       { color: #80cbc4; }
.console-cursor       { opacity: 0.2; }
.console-cursor.blink { animation: blink 1s step-end infinite; }
@keyframes blink { 0%, 100% { opacity: 1; } 50% { opacity: 0; } }
</style>
