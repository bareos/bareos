<template>
  <div>
    <div class="step-header">Administrative User</div>
    <p class="text-body2 q-mb-md">
      Create an administrative user for the Bareos WebUI and bconsole.
    </p>

    <q-card flat bordered class="q-mb-lg">
      <q-card-section class="q-gutter-md">
        <q-input
          v-model="store.adminUser.username"
          label="Username"
          dense outlined
          :rules="[v => !!v || 'Username is required']"
        />
        <q-input
          v-model="store.adminUser.password"
          label="Password"
          :type="showPwd ? 'text' : 'password'"
          dense outlined
          :rules="[v => v.length >= 8 || 'Minimum 8 characters']"
        >
          <template #append>
            <q-btn flat round dense icon="content_copy" size="sm"
                   @click="copyPassword" title="Copy password" />
            <q-icon
              :name="showPwd ? 'visibility_off' : 'visibility'"
              class="cursor-pointer"
              @click="showPwd = !showPwd"
            />
            <q-btn flat round dense icon="casino" size="sm"
                   @click="generate" title="Generate password" />
          </template>
        </q-input>
      </q-card-section>
    </q-card>

    <!-- Output from backend (create_admin_user step) -->
    <div v-if="lines.length" class="output-console q-mb-md" ref="consoleEl">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      Failed to create admin user (exit code {{ exitCode }}).
    </q-banner>
    <q-banner v-if="done && exitCode === 0" class="bg-positive text-white q-mb-md" rounded>
      <template #avatar><q-icon name="check_circle" /></template>
      Admin user created successfully.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="!adminDone" label="Create User"
               color="secondary" icon="person_add" :loading="running"
               :disable="!canProceed"
               @click="createUser" />
        <q-btn label="Continue" color="primary" icon-right="arrow_forward"
               :disable="!adminDone"
               @click="next" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, nextTick } from 'vue'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const $q = useQuasar()
const router = useRouter()
const store  = useSetupStore()
const { send, messages, clearMessages } = useSetupWs()

const showPwd   = ref(false)
const running   = ref(false)
const done      = ref(false)
const adminDone = ref(false)
const exitCode  = ref(0)
const lines     = ref([])
const consoleEl = ref(null)

const canProceed = computed(() =>
  !!store.adminUser.username && store.adminUser.password.length >= 8
)

function generate() {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789!@#$%^&*'
  const arr = new Uint8Array(24)
  crypto.getRandomValues(arr)
  store.adminUser.password = Array.from(arr, b => chars[b % chars.length]).join('')
}

async function copyPassword() {
  if (!store.adminUser.password) return

  await navigator.clipboard.writeText(store.adminUser.password)
  $q.notify({
    type: 'positive',
    message: 'Password copied to clipboard.',
  })
}

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
    if (last.exit_code === 0) adminDone.value = true
  }
}, { deep: true })

function createUser() {
  clearMessages()
  lines.value  = []
  done.value   = false
  adminDone.value = false
  running.value = true
  send({
    action:   'run_step',
    id:       'create_admin_user',
    sudo:     true,
    username: store.adminUser.username,
    password: store.adminUser.password,
  })
}

function back() { store.prevStep(); router.push('/database') }
function next() { store.nextStep(); router.push('/summary') }
</script>
