<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:900px">
      <q-card-section class="panel-header row items-center">
        <span>Bareos Console</span>
        <q-space />
        <q-chip dense square :color="statusColor" text-color="white" :label="director.status" class="q-mr-sm" style="font-size:0.72rem" />
        <q-btn flat round dense icon="delete_sweep" color="white" title="Clear" @click="output = []" />
      </q-card-section>
      <q-card-section>
        <div class="console-output q-mb-md" ref="outputEl">
          <div v-for="(line, i) in output" :key="i" :class="lineClass(line)">
            <span v-if="line.type === 'cmd'">* </span>
            <span v-else-if="line.type === 'err'" class="text-negative">! </span>
            {{ line.text }}
          </div>
          <span class="text-positive">*&nbsp;</span>
        </div>
        <div class="row q-col-gutter-sm">
          <div class="col">
            <q-input
              v-model="cmd"
              outlined dense
              placeholder="Enter command (e.g. status director)"
              :disable="!director.isConnected"
              @keyup.enter="send"
              class="font-mono"
            >
              <template #prepend><span class="text-positive text-weight-bold">*</span></template>
            </q-input>
          </div>
          <div class="col-auto">
            <q-btn color="primary" label="Send" icon="send" @click="send" no-caps :disable="!director.isConnected" />
          </div>
        </div>
        <div class="row q-gutter-xs q-mt-sm flex-wrap">
          <q-chip v-for="c in quickCmds" :key="c" clickable @click="quickSend(c)"
                  color="grey-3" text-color="dark" size="sm" :disable="!director.isConnected">{{ c }}</q-chip>
        </div>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref, computed, nextTick } from 'vue'
import { useDirectorStore } from '../stores/director.js'

const director = useDirectorStore()
const cmd       = ref('')
const outputEl  = ref(null)
const output    = ref([
  { type: 'out', text: 'Bareos WebUI Console' },
  { type: 'out', text: director.isConnected
      ? `Connected to ${director.status === 'connected' ? 'director' : '…'}`
      : 'Not connected — log in to use the console.' },
])

const quickCmds = ['status director', 'list jobs', 'list clients', 'list volumes', 'list pools', 'messages', 'help', 'version']

const statusColor = computed(() => ({
  connected: 'positive', connecting: 'warning', authenticating: 'warning',
  error: 'negative', disconnected: 'grey',
}[director.status] ?? 'grey'))

function lineClass(line) {
  if (line.type === 'cmd') return 'text-info'
  if (line.type === 'err') return 'text-negative'
  return ''
}

function appendOutput(text, type = 'out') {
  if (typeof text === 'object') {
    // Pretty-print JSON data from the director
    output.value.push({ type, text: JSON.stringify(text, null, 2) })
  } else {
    String(text).split('\n').forEach(l => output.value.push({ type, text: l }))
  }
}

async function scrollBottom() {
  await nextTick()
  if (outputEl.value) outputEl.value.scrollTop = outputEl.value.scrollHeight
}

async function send() {
  const c = cmd.value.trim()
  if (!c) return
  output.value.push({ type: 'cmd', text: c })
  cmd.value = ''

  if (!director.isConnected) {
    appendOutput('Not connected to director.', 'err')
    await scrollBottom()
    return
  }

  try {
    const result = await director.call(c)
    appendOutput(result)
  } catch (err) {
    appendOutput(err.message, 'err')
  }

  await scrollBottom()
}

function quickSend(c) { cmd.value = c; send() }
</script>

<style scoped>
.font-mono :deep(input) { font-family: 'Courier New', monospace; font-size: 0.85rem; }
</style>

