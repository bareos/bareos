<template>
  <div>
    <div class="step-header">Database Configuration</div>
    <p class="text-body2 q-mb-md">
      Bareos requires a PostgreSQL catalog database. Configure it below.
    </p>

    <q-card flat bordered class="q-mb-md">
      <q-card-section>
        <q-toggle v-model="store.dbConfig.createDb" label="Create database and initialize schema" />
        <div v-if="store.dbConfig.createDb" class="q-mt-md q-gutter-md">
          <q-input v-model="store.dbConfig.dbName" label="Database name"
                   dense outlined />
          <q-input v-model="store.dbConfig.dbUser" label="Database user"
                   dense outlined />
        </div>
      </q-card-section>
    </q-card>

    <!-- Live output -->
    <div v-if="lines.length" class="output-console q-mb-md" ref="consoleEl">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      Database setup failed (exit code {{ exitCode }}).
    </q-banner>
    <q-banner v-if="done && exitCode === 0" class="bg-positive text-white q-mb-md" rounded>
      <template #avatar><q-icon name="check_circle" /></template>
      Database configured successfully.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="store.dbConfig.createDb && !dbDone" label="Set up Database"
               color="secondary" icon="storage" :loading="running"
               @click="setupDb" />
        <q-btn label="Continue" color="primary" icon-right="arrow_forward"
               :disable="store.dbConfig.createDb && !dbDone"
               @click="next" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, nextTick } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store  = useSetupStore()
const { send, messages, clearMessages } = useSetupWs()

const running   = ref(false)
const done      = ref(false)
const dbDone    = ref(false)
const exitCode  = ref(0)
const lines     = ref([])
const consoleEl = ref(null)

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last || !running.value) return
  if (last.type === 'output') {
    lines.value.push({
      text: last.line,
      cls:  last.stream === 'stderr' ? 'output-line-err' : 'output-line-out',
    })
    nextTick(() => {
      if (consoleEl.value) consoleEl.value.scrollTop = consoleEl.value.scrollHeight
    })
  } else if (last.type === 'done') {
    running.value = false
    done.value    = true
    exitCode.value = last.exit_code
    if (last.exit_code === 0) dbDone.value = true
  }
}, { deep: true })

function setupDb() {
  clearMessages()
  lines.value = []
  done.value  = false
  dbDone.value = false
  running.value = true
  send({
    action:  'run_step',
    id:      'setup_db',
    sudo:    true,
    db_name: store.dbConfig.dbName,
    db_user: store.dbConfig.dbUser,
  })
}

function back() { store.prevStep(); router.push('/install') }
function next() { store.nextStep(); router.push('/passwords') }
</script>
