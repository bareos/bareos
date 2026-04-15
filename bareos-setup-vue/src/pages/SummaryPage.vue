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
              <q-item-label>Storage Targets</q-item-label>
              <q-item-label caption>{{ storageTargets }}</q-item-label>
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
              <q-item-label caption>Catalog database will be initialized.</q-item-label>
            </q-item-section>
          </q-item>
          <q-item v-if="store.storageTargets.disk">
            <q-item-section avatar><q-icon name="save" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Disk Storage</q-item-label>
              <q-item-label caption>
                {{ store.diskConfig.path }} · {{ store.diskConfig.concurrentJobs }} jobs{{ store.diskConfig.dedupable ? ' · dedupable' : '' }}
              </q-item-label>
            </q-item-section>
          </q-item>
          <q-item v-if="store.storageTargets.tape">
            <q-item-section avatar><q-icon name="inventory_2" color="primary" /></q-item-section>
            <q-item-section>
              <q-item-label>Tape Storage</q-item-label>
              <q-item-label caption>{{ tapeAssignmentsSummary }}</q-item-label>
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
      <q-btn label="Open WebUI"
             icon="open_in_browser" color="primary"
             :href="webuiUrl" target="_blank" />
    </div>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <q-btn label="Done" color="positive" icon="check_circle" @click="finish" />
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { useQuasar } from 'quasar'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const $q = useQuasar()
const router = useRouter()
const store  = useSetupStore()
const { send, messages } = useSetupWs()

const generatingScript = ref(false)
const scriptContent    = ref('')

const webuiUrl = computed(() => {
  const protocol = window.location.protocol === 'https:' ? 'https:' : 'http:'
  return `${protocol}//${window.location.hostname}:9100`
})

const storageTargets = computed(() => {
  const labels = []
  if (store.storageTargets.disk) labels.push('disk')
  if (store.storageTargets.tape) labels.push('tape changer')
  return labels.join(', ') || 'None'
})

const tapeAssignmentsSummary = computed(() =>
  store.tapeAssignments
    .filter((assignment) => assignment.drive_paths.length > 0)
    .map((assignment) => `${assignment.changer_path} (${assignment.drive_paths.length} drives)`)
    .join('; ') || 'No changer assignments'
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
  } else if (last.type === 'error') {
    generatingScript.value = false
    $q.notify({
      type: 'negative',
      message: last.message || 'Failed to generate the setup script.',
    })
  }
}, { deep: true })

function generateScript() {
  generatingScript.value = true
  send({
    action: 'generate_script',
    distro: store.osInfo?.distro,
    version: store.osInfo?.version,
    repo_type: store.repoType,
    repo_login: store.repoCredentials.login,
    repo_password: store.repoCredentials.password,
    pkg_mgr: store.osInfo?.pkg_mgr,
    disk_enabled: store.storageTargets.disk,
    disk_path: store.diskConfig.path,
    disk_dedupable: store.diskConfig.dedupable,
    disk_concurrent_jobs: Number(store.diskConfig.concurrentJobs),
    tape_enabled: store.storageTargets.tape,
    tape_assignments: store.tapeAssignments.filter((assignment) => assignment.drive_paths.length > 0),
    setup_db: store.dbConfig.createDb,
    admin_username: store.adminUser.username,
    admin_password: store.adminUser.password,
  })
}

function back() { store.prevStep(); router.push('/passwords') }
function finish() {
  store.setupDone = true
  window.close()
  window.setTimeout(() => {
    if (!window.closed) {
      $q.notify({
        type: 'info',
        message: 'This tab cannot be closed automatically. You can close it manually and open the WebUI link above.',
      })
    }
  }, 100)
}
</script>
