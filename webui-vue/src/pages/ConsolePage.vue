<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:900px">
      <q-card-section class="panel-header row items-center">
        <span>Bareos Console</span>
        <q-space />
        <q-btn flat round dense icon="delete_sweep" color="white" title="Clear" @click="output = []" />
      </q-card-section>
      <q-card-section>
        <div class="console-output q-mb-md" ref="outputEl">
          <div v-for="(line, i) in output" :key="i" :class="line.type === 'cmd' ? 'text-info' : ''">
            <span v-if="line.type === 'cmd'">* </span>{{ line.text }}
          </div>
          <span class="text-positive">*&nbsp;</span>
        </div>
        <div class="row q-col-gutter-sm">
          <div class="col">
            <q-input
              v-model="cmd"
              outlined dense
              placeholder="Enter command (e.g. status director)"
              @keyup.enter="send"
              class="font-mono"
            >
              <template #prepend><span class="text-positive text-weight-bold">*</span></template>
            </q-input>
          </div>
          <div class="col-auto">
            <q-btn color="primary" label="Send" icon="send" @click="send" no-caps />
          </div>
        </div>
        <div class="row q-gutter-xs q-mt-sm flex-wrap">
          <q-chip v-for="c in quickCmds" :key="c" clickable @click="quickSend(c)" color="grey-3" text-color="dark" size="sm">{{ c }}</q-chip>
        </div>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref, nextTick } from 'vue'
import { directorStatus } from '../mock/index.js'

const cmd = ref('')
const output = ref([
  { type: 'out', text: 'Bareos WebUI Console — connected to bareos-dir' },
  { type: 'out', text: 'Type "help" for a list of commands.' },
])
const outputEl = ref(null)

const quickCmds = ['status director', 'status client', 'list jobs', 'list clients', 'list volumes', 'messages', 'help']

async function send() {
  const c = cmd.value.trim()
  if (!c) return
  output.value.push({ type: 'cmd', text: c })
  cmd.value = ''

  // Mock responses
  let resp
  if (c === 'status director' || c === 'status') resp = directorStatus
  else if (c === 'list jobs')    resp = 'JobId  Level  Files     Bytes     Status  Name\n' + Array.from({length:5},(_,i)=>`${i+1}      Full   ${(Math.random()*1e6|0).toLocaleString()}  2.1 GB    T       BackupClient${i+1}`).join('\n')
  else if (c === 'list clients') resp = 'bareos-fd\nfileserver-fd\ndb-server-fd\nmail-fd\nweb-fd'
  else if (c === 'messages')     resp = 'No new messages.'
  else if (c === 'help')         resp = 'Available commands: status, list, run, restore, cancel, messages, version, quit'
  else                           resp = `Command "${c}" accepted.`

  output.value.push({ type: 'out', text: resp })
  await nextTick()
  if (outputEl.value) outputEl.value.scrollTop = outputEl.value.scrollHeight
}

function quickSend(c) { cmd.value = c; send() }
</script>

<style scoped>
.font-mono :deep(input) { font-family: 'Courier New', monospace; font-size: 0.85rem; }
</style>
