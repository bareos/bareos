<template>
  <div>
    <div class="step-header">Configure Repository</div>
    <p class="text-body2 q-mb-md">
      Choose the Bareos package repository to use for installation.
    </p>

    <q-card flat bordered class="q-mb-md">
      <q-card-section>
        <q-option-group v-model="store.repoType" :options="repoOptions" color="primary" />
      </q-card-section>

      <!-- Subscription credentials -->
      <q-card-section v-if="store.repoType === 'subscription'" class="q-pt-none q-gutter-md">
        <q-input
          v-model="store.repoCredentials.login"
          label="Subscription login"
          dense outlined
          :rules="[v => !!v || 'Login is required']"
        />
        <q-input
          v-model="store.repoCredentials.password"
          label="Subscription password"
          type="password"
          dense outlined
          :rules="[v => !!v || 'Password is required']"
        />
      </q-card-section>
    </q-card>

    <q-banner class="bg-info text-white q-mb-lg" rounded>
      <template #avatar><q-icon name="info" /></template>
      Repository URL: <code>{{ repoUrl }}</code>
    </q-banner>

    <!-- Live output -->
    <div v-if="lines.length" class="output-console q-mb-md" ref="console_">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      Repository setup failed (exit code {{ exitCode }}). Check output above.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="!store.repoAdded" label="Add Repository"
               color="secondary" icon="source" :loading="running"
               :disable="!canProceed"
               @click="addRepo" />
        <q-btn v-else label="Continue" color="primary" icon-right="arrow_forward"
               @click="next" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, nextTick } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store  = useSetupStore()
const { send, messages, clearMessages } = useSetupWs()

const running  = ref(false)
const done     = ref(false)
const exitCode = ref(0)
const lines    = ref([])
const console_ = ref(null)

const repoOptions = [
  { label: 'Subscription (requires Bareos subscription)', value: 'subscription' },
  { label: 'Community (free)',                            value: 'community' },
]

const repoUrl = computed(() => {
  if (!store.osInfo) return '(OS not yet detected)'
  const base = store.repoType === 'subscription'
    ? 'https://download.bareos.com/bareos/release/latest'
    : 'https://download.bareos.org/current'
  const distro = store.osInfo.distro
    ? store.osInfo.distro.charAt(0).toUpperCase() + store.osInfo.distro.slice(1)
    : ''
  return `${base}/${distro}_${store.osInfo.version}/`
})

const canProceed = computed(() => {
  if (store.repoType === 'subscription') {
    return !!store.repoCredentials.login && !!store.repoCredentials.password
  }
  return true
})

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last || !running.value) return
  if (last.type === 'output') {
    lines.value.push({
      text: last.line,
      cls:  last.stream === 'stderr' ? 'output-line-err' : 'output-line-out',
    })
    nextTick(() => {
      if (console_.value) console_.value.scrollTop = console_.value.scrollHeight
    })
  } else if (last.type === 'done') {
    running.value = false
    done.value    = true
    exitCode.value = last.exit_code
    if (last.exit_code === 0) store.repoAdded = true
  }
}, { deep: true })

function addRepo() {
  clearMessages()
  lines.value = []
  done.value  = false
  running.value = true
  store.repoAdded = false
  send({
    action:    'run_step',
    id:        'add_repo',
    sudo:      true,
    repo_type: store.repoType,
    distro:    store.osInfo?.distro,
    version:   store.osInfo?.version,
    // only set for subscription
    repo_login:    store.repoType === 'subscription' ? store.repoCredentials.login    : '',
    repo_password: store.repoType === 'subscription' ? store.repoCredentials.password : '',
  })
}

function back() { store.prevStep(); router.push('/components') }
function next() { store.nextStep(); router.push('/install') }
</script>
