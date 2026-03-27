<template>
  <div>
    <div class="step-header">Summary</div>
    <p class="text-body2 q-mb-md">
      Setup is complete. Review what was configured below.
    </p>

    <q-card flat bordered class="q-mb-md">
      <q-card-section>
        <q-list dense>
          <q-item>
            <q-item-section avatar><q-icon name="computer" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Operating System</q-item-label>
              <q-item-label caption>{{ store.osInfo?.pretty_name || store.osInfo?.distro || 'Unknown' }}</q-item-label>
            </q-item-section>
          </q-item>
          <q-item>
            <q-item-section avatar><q-icon name="tune" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Installed Components</q-item-label>
              <q-item-label caption>{{ installedComponents }}</q-item-label>
            </q-item-section>
          </q-item>
          <q-item>
            <q-item-section avatar><q-icon name="source" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Repository</q-item-label>
              <q-item-label caption>{{ store.repoType === 'subscription' ? 'Bareos Subscription' : 'Bareos Community' }}</q-item-label>
            </q-item-section>
          </q-item>
          <q-item v-if="store.dbConfig.createDb">
            <q-item-section avatar><q-icon name="storage" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Database</q-item-label>
              <q-item-label caption>{{ store.dbConfig.dbName }} (user: {{ store.dbConfig.dbUser }})</q-item-label>
            </q-item-section>
          </q-item>
        </q-list>
      </q-card-section>
    </q-card>

    <!-- Script output -->
    <div v-if="scriptContent" class="output-console q-mb-md">{{ scriptContent }}</div>

    <div class="row q-gutter-sm q-mb-lg">
      <q-btn label="Download Script" icon="download" color="secondary"
             :loading="generatingScript" @click="generateScript" />
      <q-btn v-if="store.components.webui" label="Open WebUI"
             icon="open_in_browser" color="primary"
             href="http://localhost:9100" target="_blank" />
    </div>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <q-btn label="Done" color="positive" icon="check_circle" @click="finish" />
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store  = useSetupStore()
const { send, messages } = useSetupWs()

const generatingScript = ref(false)
const scriptContent    = ref('')

const LABELS = {
  director: 'Director', storage: 'Storage Daemon',
  filedaemon: 'File Daemon', webui: 'WebUI',
}
const installedComponents = computed(() =>
  Object.entries(store.components)
    .filter(([, v]) => v)
    .map(([k]) => LABELS[k] || k)
    .join(', ') || 'None'
)

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last) return
  if (last.type === 'script') {
    scriptContent.value    = last.content
    generatingScript.value = false
    // trigger download
    const blob = new Blob([last.content], { type: 'text/plain' })
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href     = url
    a.download = 'bareos-setup.sh'
    a.click()
    URL.revokeObjectURL(url)
  }
}, { deep: true })

function generateScript() {
  generatingScript.value = true
  send({ action: 'generate_script' })
}

function back() { store.prevStep(); router.push('/passwords') }
function finish() {
  store.setupDone = true
  window.close()
}
</script>
