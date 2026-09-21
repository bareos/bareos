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
        <q-btn flat round dense icon="refresh" color="white" :title="t('Reconnect')" :aria-label="t('Reconnect')" @click="reconnectSelectedSession" />
        <q-btn v-if="!isPopup" flat round dense icon="open_in_new" color="white" :title="t('Open in new window')" :aria-label="t('Open in new window')" @click="popOut" />
        <q-btn v-if="isPopup"  flat round dense icon="close"       color="white" :title="t('Close window')" :aria-label="t('Close window')"       @click="closePopup" />
        <q-btn flat round dense icon="delete_sweep" color="white" :title="t('Clear')" :aria-label="t('Clear')" @click="clearOutput" />
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

      <!-- xterm.js renders all output (colors, frames, selection menus)
           and the live input line; click anywhere to focus it. The
           padding lives on this outer wrapper, not on the element passed
           to terminal.mount() below — FitAddon sizes the terminal to
           that element's *parent*, but only ever subtracts the
           terminal's own (zero) padding, so any padding placed directly
           on the mounted element itself would make the fitted terminal
           larger than its visible content area and produce scrollbars. -->
      <div
        class="console-output-wrapper"
        :class="{ 'console-output-wrapper-popup': isPopup }"
        @click="focusConsole"
      >
        <div
          data-testid="console-output"
          class="console-terminal-mount"
          ref="terminalContainerEl"
        ></div>
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
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { DEFAULT_DIRECTOR_NAME, useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { useConsoleSessionsStore, applyConsoleKey } from '../stores/consoleSessions.js'
import { useConsoleTerminal } from '../composables/useConsoleTerminal.js'
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

// ── terminal ──────────────────────────────────────────────────────────────────
const terminalContainerEl = ref(null)
const terminal = useConsoleTerminal({
  onResize: ({ rows, cols }) => {
    consoleSessions.setTerminalSize(selectedDirector.value, rows, cols)
  },
})

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

function clearOutput() {
  terminal.clear()
  consoleSessions.clearOutput(selectedDirector.value)
}

function focusConsole() {
  terminal.focus()
}

// Redraws the live (uncommitted) input line in place: return to column 0,
// erase to end of line, write the prompt + current command, then move the
// real terminal cursor back to the tracked edit position. This mirrors
// what a PTY line-discipline would otherwise do, since input editing here
// is handled locally (session.cmd/cursorPos) rather than by the Director.
function redrawInputLine() {
  if (!terminal.terminal || currentSession.value.selectionActive) {
    return
  }
  const session = currentSession.value
  const cmd = session.cmd ?? ''
  const cursorPos = session.cursorPos ?? cmd.length
  terminal.write(`\r\x1B[K${session.currentPrompt ?? ''}${cmd}`)
  const back = cmd.length - cursorPos
  if (back > 0) {
    terminal.write(`\x1B[${back}D`)
  }
}

// Wraps the raw terminal writer so any text the store writes (command
// echoes, Director responses, selection frames) first erases whatever
// live input line is currently drawn, then redraws it fresh afterward —
// mirroring how a readline-style line discipline keeps the prompt
// "sticky" below interleaved asynchronous output.
function makeTerminalWriter() {
  return (text) => {
    terminal.write('\r\x1B[K')
    terminal.write(text)
    redrawInputLine()
  }
}

function registerTerminalWriter(directorName) {
  consoleSessions.setTerminalWriter(directorName, makeTerminalWriter())
}

function unregisterTerminalWriter(directorName) {
  consoleSessions.setTerminalWriter(directorName, null)
}

function mountTerminal() {
  terminal.mount(terminalContainerEl.value)
  terminal.terminal?.attachCustomKeyEventHandler(handleTerminalKey)
  registerTerminalWriter(selectedDirector.value)
  redrawInputLine()
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
  session.cursorPos = 0
  consoleSessions.sendCommand(selectedDirector.value, command)
}

function quickSend(c) {
  currentSession.value.cmd = ''
  currentSession.value.cursorPos = 0
  consoleSessions.appendCommand(selectedDirector.value, c)
  consoleSessions.sendCommand(selectedDirector.value, c)
  focusConsole()
}

function sendTab() {
  consoleSessions.requestCompletion(selectedDirector.value, currentSession.value.cmd)
  redrawInputLine()
}

// Handles a keydown event captured by xterm.js (via
// attachCustomKeyEventHandler, see mountTerminal()). Mutates the local
// line-editing state (session.cmd/cursorPos/history) exactly as before —
// the console remains a client-side line editor, not a raw PTY passthrough
// — then redraws the live input line directly in the terminal. Returns
// false so xterm.js never processes the key itself (we own all echo).
function handleTerminalKey(event) {
  if (event.type !== 'keydown') {
    return true
  }

  const session = currentSession.value

  if (session.selectionActive) {
    const selectionEvents = {
      ArrowUp: 'key:up',
      ArrowDown: 'key:down',
      ArrowLeft: 'key:left',
      ArrowRight: 'key:right',
      Enter: 'key:enter',
      Escape: 'key:cancel',
      Backspace: 'key:backspace',
      ' ': 'key:space',
    }
    let selectionEvent = selectionEvents[event.key]
    if (event.ctrlKey && event.key === 'c') {
      selectionEvent = 'key:cancel'
    } else if (!selectionEvent && event.key.length === 1
      && !event.ctrlKey && !event.altKey && !event.metaKey) {
      selectionEvent = `key:text:${event.key}`
    }
    if (selectionEvent) {
      event.preventDefault()
      consoleSessions.sendSelectionEvent(selectedDirector.value, selectionEvent)
    }
    return false
  }

  // Don't interfere with unhandled browser shortcuts
  if (event.ctrlKey && !['c', 'l', 'a', 'e', 'k', 'u'].includes(event.key)) {
    return true
  }

  event.preventDefault()

  if (event.key === 'Tab') {
    sendTab()
  } else if (event.key === 'Enter') {
    send()
  } else if (event.key === 'ArrowUp') {
    if (session.history.length === 0) return false
    if (session.historyIdx > 0) session.historyIdx--
    session.cmd = session.history[session.historyIdx] ?? ''
    session.cursorPos = session.cmd.length
    redrawInputLine()
  } else if (event.key === 'ArrowDown') {
    if (session.historyIdx < session.history.length - 1) {
      session.historyIdx++
      session.cmd = session.history[session.historyIdx]
    } else {
      session.historyIdx = session.history.length
      session.cmd = ''
    }
    session.cursorPos = session.cmd.length
    redrawInputLine()
  } else if (event.ctrlKey && event.key === 'c') {
    consoleSessions.appendCommand(selectedDirector.value, `${session.cmd}^C`)
    session.cmd = ''
    session.cursorPos = 0
  } else if (event.ctrlKey && event.key === 'l') {
    clearOutput()
  } else if (applyConsoleKey(session, event)) {
    redrawInputLine()
  }

  return false
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
onMounted(async () => {
  director.fetchAvailableDirectors().catch(() => {})
  await requestPopupCredentials()
  mountTerminal()
  ensureSelectedSession()
})

onUnmounted(() => {
  unregisterTerminalWriter(selectedDirector.value)
  terminal.dispose()
})

watch(selectedDirector, async (directorName, previousDirector) => {
  if (!directorName || directorName === previousDirector) {
    return
  }

  if (previousDirector) {
    unregisterTerminalWriter(previousDirector)
  }
  // Each Director tab has its own independent session/terminal content.
  // Reset the (shared) terminal instance and re-register its writer for
  // the newly selected director — this replays that director's prior
  // output (see consoleSessions.js's terminalLogs) instead of leaving
  // stale content from the previous tab visible.
  terminal.clear()
  registerTerminalWriter(directorName)
  redrawInputLine()

  await router.replace({
    name: route.name,
    query: directorName === auth.user?.director ? {} : { director: directorName },
  })
  ensureSelectedSession()
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

// Redraw the live input line whenever the prompt changes (e.g. after a
// Director response updates it) or a selection menu ends — this is what
// makes the prompt "reappear" below newly streamed asynchronous output
// that didn't go through the wrapped terminal writer directly triggering
// a redraw (e.g. the very first prompt after connecting).
watch(
  () => [currentSession.value.currentPrompt, currentSession.value.selectionActive],
  () => {
    redrawInputLine()
  }
)
</script>


<style scoped>
.console-output-wrapper {
  /* A definite (not min/max-only) height is required here: CSS only
     resolves a child's percentage height (.console-terminal-mount below)
     against a parent with a definite height — min-height/max-height alone
     leave the computed height as "auto", so the child would instead grow
     to fit however many rows xterm.js last rendered, defeating the fit. */
  height: 420px;
  background: #1a1a1a;
  padding: 12px 16px 12px;
  cursor: text;
  overflow: hidden;
}
.console-terminal-mount {
  /* Deliberately named differently from the unscoped .console-output rule
     in src/css/app.scss (used elsewhere for a plain-text status dialog):
     reusing that class name here would leak its own padding onto this
     element, shrinking the box FitAddon actually measures and causing
     xterm to compute more rows/cols than truly fit on screen. */
  width: 100%;
  height: 100%;
  overflow: hidden;
}
.console-terminal-mount :deep(.xterm) {
  height: 100%;
}
.console-output-wrapper-popup {
  height: calc(100vh - 140px);
}
</style>
