<template>
  <div>
    <div class="step-header">Install Packages</div>
    <p class="text-body2 q-mb-md">
      The following packages will be installed based on your component selection.
    </p>

    <q-card flat bordered class="q-mb-md">
      <q-card-section>
        <div class="text-caption text-grey q-mb-xs">Packages</div>
        <q-chip v-for="pkg in packages" :key="pkg" color="primary" text-color="white" dense>
          {{ pkg }}
        </q-chip>
      </q-card-section>
    </q-card>

    <!-- Live output -->
    <div v-if="lines.length" class="output-console q-mb-md" ref="consoleEl">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      Installation failed (exit code {{ exitCode }}).
    </q-banner>
    <q-banner v-if="done && exitCode === 0" class="bg-positive text-white q-mb-md" rounded>
      <template #avatar><q-icon name="check_circle" /></template>
      Packages installed successfully.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="!store.packagesInstalled" label="Install"
               color="secondary" icon="download" :loading="running"
               @click="install" />
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

const running   = ref(false)
const done      = ref(false)
const exitCode  = ref(0)
const lines     = ref([])
const consoleEl = ref(null)

const PKG_MAP = {
  director:   ['bareos-director', 'bareos-bconsole'],
  storage:    ['bareos-storage'],
  filedaemon: ['bareos-filedaemon'],
  webui:      ['bareos-webui'],
}

const packages = computed(() => {
  const pkgs = []
  for (const [key, list] of Object.entries(PKG_MAP)) {
    if (store.components[key]) pkgs.push(...list)
  }
  return pkgs
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
      if (consoleEl.value) consoleEl.value.scrollTop = consoleEl.value.scrollHeight
    })
  } else if (last.type === 'done') {
    running.value = false
    done.value    = true
    exitCode.value = last.exit_code
    if (last.exit_code === 0) store.packagesInstalled = true
  }
}, { deep: true })

function install() {
  clearMessages()
  lines.value = []
  done.value  = false
  running.value = true
  store.packagesInstalled = false
  send({
    action:   'run_step',
    id:       'install_packages',
    sudo:     true,
    packages: packages.value,
    pkg_mgr:  store.osInfo?.pkg_mgr,
  })
}

function back() { store.prevStep(); router.push('/repo') }
function next() { store.nextStep(); router.push('/database') }
</script>
