<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" :style="isPopup ? 'max-width:100%' : 'max-width:960px'">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Bareos Console') }}</span>
        <q-space />
        <q-chip
          v-if="directorOptions.length > 1"
          dense
          square
          color="primary"
          text-color="white"
          :label="selectedDirector"
          class="q-mr-sm"
          style="font-size:0.72rem"
        />
        <q-chip dense square :color="statusColor" text-color="white" :label="consoleStatusLabel" class="q-mr-sm" style="font-size:0.72rem" />
        <q-btn flat round dense icon="refresh" color="white" :title="t('Reconnect')" @click="reconnectSelectedSession" />
        <q-btn v-if="!isPopup" flat round dense icon="open_in_new" color="white" :title="t('Open in new window')" @click="popOut" />
        <q-btn v-if="isPopup"  flat round dense icon="close"       color="white" :title="t('Close window')"       @click="closePopup" />
        <q-btn flat round dense icon="delete_sweep" color="white" :title="t('Clear')" @click="clearOutput" />
      </q-card-section>

      <q-tabs
        v-if="directorOptions.length > 1"
        v-model="selectedDirector"
        dense
        align="left"
        active-color="primary"
        indicator-color="primary"
        class="bg-grey-2 text-dark"
      >
        <q-tab
          v-for="directorOption in directorOptions"
          :key="directorOption.value"
          :name="directorOption.value"
          :label="directorOption.label"
          no-caps
        />
      </q-tabs>

      <!-- terminal area — click to focus, then type -->
      <div
        data-testid="console-output"
        class="console-output"
        :class="{ 'console-output-popup': isPopup }"
        ref="outputEl"
        tabindex="0"
        @keydown="onKeyDown"
        @focus="focused = true"
        @blur="focused = false"
        @click="focusConsole"
      >
        <div v-for="(line, i) in currentSession.output" :key="i" :class="['console-line', line.cls]">{{ line.text }}</div>

        <!-- live input line -->
        <div class="console-line console-input-line">
          <span class="console-prompt">{{ currentSession.currentPrompt }}</span>
          <span>{{ currentSession.cmd }}</span><span :class="['console-cursor', { blink: focused }]">█</span>
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
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { DEFAULT_DIRECTOR_NAME, useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { useConsoleSessionsStore } from '../stores/consoleSessions.js'
import { buildDirectorOptions } from '../utils/director.js'
import {
  CONSOLE_POPUP_AUTH_REQUEST,
  CONSOLE_POPUP_AUTH_RESPONSE,
} from '../utils/consolePopupAuth.js'

const auth     = useAuthStore()
const director = useDirectorStore()
const route    = useRoute()
const router   = useRouter()
const settings = useSettingsStore()
const consoleSessions = useConsoleSessionsStore()
const { t } = useI18n()

const isPopup = computed(() => route.name === 'console-popup')
const selectedDirector = ref(
  String(route.query.director ?? '').trim()
  || auth.user?.director
  || settings.directorName
)

function popOut() {
  const base = window.location.href.replace(/#.*$/, '')
  const directorName = encodeURIComponent(selectedDirector.value)
  const popupName = `bareos-console-${selectedDirector.value.replace(/[^A-Za-z0-9_-]+/g, '-')}`
  window.open(
    `${base}#/console-popup?director=${directorName}`,
    popupName,
    'width=960,height=720,resizable=yes,scrollbars=no'
  )
}

function closePopup() {
  window.close()
}

async function requestPopupCredentials() {
  if (!isPopup.value || auth.getCredentials()?.password || !window.opener) {
    return
  }

  await new Promise((resolve) => {
    const timeout = window.setTimeout(() => {
      window.removeEventListener('message', onMessage)
      resolve()
    }, 1500)

    function onMessage(event) {
      if (event.origin !== window.location.origin) {
        return
      }

      if (event.source !== window.opener) {
        return
      }

      if (event.data?.type !== CONSOLE_POPUP_AUTH_RESPONSE) {
        return
      }

      window.clearTimeout(timeout)
      window.removeEventListener('message', onMessage)

      const credentials = event.data.credentials
      if (credentials?.username && credentials?.password) {
        auth.login(
          credentials.username,
          credentials.director || DEFAULT_DIRECTOR_NAME,
          credentials.password,
        )
      }

      resolve()
    }

    window.addEventListener('message', onMessage)
    window.opener.postMessage({
      type: CONSOLE_POPUP_AUTH_REQUEST,
      director: selectedDirector.value,
    }, window.location.origin)
  })
}

// ── refs ─────────────────────────────────────────────────────────────────────
const outputEl = ref(null)
const focused  = ref(false)

const directorOptions = computed(() => {
  return buildDirectorOptions({
    availableDirectors: director.availableDirectors,
    selectedDirectors: [
      ...settings.selectedDirectors,
      ...consoleSessions.directors,
    ],
    currentDirector: selectedDirector.value,
    fallbackDirector: auth.user?.director,
  })
})

const currentSession = computed(() => (
  consoleSessions.getSession(selectedDirector.value)
))
const consoleStatus = computed(() => currentSession.value.status)

const statusColor = computed(() => ({
  connected: 'positive', connecting: 'warning',
  error: 'negative', disconnected: 'grey',
}[consoleStatus.value] ?? 'grey'))
const consoleStatusLabel = computed(() => ({
  connected: t('Connected'),
  connecting: t('Connecting'),
  error: t('Error'),
  disconnected: t('Disconnected'),
}[consoleStatus.value] ?? consoleStatus.value))

const quickCmds = ['status director', 'list jobs', 'list clients', 'list volumes', 'list pools', 'messages', 'help', 'version']

async function scrollBottom() {
  await nextTick()
  if (outputEl.value) outputEl.value.scrollTop = outputEl.value.scrollHeight
}

function clearOutput() {
  consoleSessions.clearOutput(selectedDirector.value)
}

function focusConsole() {
  outputEl.value?.focus()
}

function ensureSelectedSession() {
  const session = currentSession.value
  const credentials = auth.getCredentials()
  if (!session.initialized) {
    consoleSessions.appendInfo(
      selectedDirector.value,
      t('Bareos WebUI Console — click here to type, use ↑ and ↓ for history, Ctrl+L to clear')
    )
    session.initialized = true
  }

  consoleSessions.connectSession(
    selectedDirector.value,
    credentials
      ? { ...credentials, director: selectedDirector.value }
      : null
  )
}

function reconnectSelectedSession() {
  consoleSessions.disconnectSession(selectedDirector.value, { reason: 'Disconnected' })
  ensureSelectedSession()
}

// ── send / keyboard ───────────────────────────────────────────────────────────
function send() {
  const session = currentSession.value
  const command = session.cmd.trim()

  if (
    command
    && (
      !session.history.length
      || session.history[session.history.length - 1] !== command
    )
  ) {
    session.history.push(command)
    session.historyIdx = session.history.length
  }

  consoleSessions.appendCommand(selectedDirector.value, command)
  session.cmd = ''
  consoleSessions.sendCommand(selectedDirector.value, command)
  scrollBottom()
}

function quickSend(c) {
  currentSession.value.cmd = ''
  consoleSessions.appendCommand(selectedDirector.value, c)
  consoleSessions.sendCommand(selectedDirector.value, c)
  scrollBottom()
  focusConsole()
}

function sendTab() {
  consoleSessions.requestCompletion(selectedDirector.value, currentSession.value.cmd)
}

function onKeyDown(event) {
  const session = currentSession.value

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
    session.cmd = session.cmd.slice(0, -1)
  } else if (event.key === 'Delete') {
    event.preventDefault()
    session.cmd = ''
  } else if (event.key === 'ArrowUp') {
    event.preventDefault()
    if (session.history.length === 0) return
    if (session.historyIdx > 0) session.historyIdx--
    session.cmd = session.history[session.historyIdx] ?? ''
  } else if (event.key === 'ArrowDown') {
    event.preventDefault()
    if (session.historyIdx < session.history.length - 1) {
      session.historyIdx++
      session.cmd = session.history[session.historyIdx]
    } else {
      session.historyIdx = session.history.length
      session.cmd = ''
    }
  } else if (event.ctrlKey && event.key === 'c') {
    event.preventDefault()
    consoleSessions.appendCommand(selectedDirector.value, `${session.cmd}^C`)
    session.cmd = ''
  } else if (event.ctrlKey && event.key === 'l') {
    event.preventDefault()
    clearOutput()
  } else if (event.key.length === 1 && !event.altKey) {
    // printable character
    event.preventDefault()
    session.cmd += event.key
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
onMounted(async () => {
  director.fetchAvailableDirectors().catch(() => {})
  await requestPopupCredentials()
  ensureSelectedSession()
})

onUnmounted(() => {
  focused.value = false
})

watch(selectedDirector, async (directorName, previousDirector) => {
  if (!directorName || directorName === previousDirector) {
    return
  }

  await router.replace({
    name: route.name,
    query: directorName === auth.user?.director ? {} : { director: directorName },
  })
  ensureSelectedSession()
  scrollBottom()
})

watch(() => route.query.director, (queryDirector) => {
  const normalizedDirector = String(queryDirector ?? '').trim()
  const fallbackDirector = auth.user?.director || settings.directorName
  const targetDirector = normalizedDirector || fallbackDirector

  if (targetDirector && targetDirector !== selectedDirector.value) {
    selectedDirector.value = targetDirector
  }
})

watch(() => auth.user?.director, (directorName) => {
  if (!selectedDirector.value && directorName) {
    selectedDirector.value = directorName
    ensureSelectedSession()
  }
})

watch(() => currentSession.value.output.length, () => {
  scrollBottom()
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
