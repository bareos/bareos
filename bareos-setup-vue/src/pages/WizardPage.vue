<template>
  <q-layout view="hHh lpR fFf">
    <q-header class="bg-primary"><q-toolbar>
      <img src="../../../core/src/images/bareos-logo.svg" alt="Bareos"
        class="toolbar-logo q-mr-sm" />
      <q-toolbar-title>Bareos Setup</q-toolbar-title>
      <div v-if="store.state.setup_version" class="text-caption">
        v{{ store.state.setup_version }}
      </div>
    </q-toolbar></q-header>
    <q-page-container><q-page class="q-pa-lg wizard">
      <q-linear-progress :value="progress" class="q-mb-lg" />
      <div class="row items-center q-mb-sm">
        <img v-if="step !== 'welcome'" src="../../../core/src/images/boris.png"
          alt="Boris the Bareos wizard" class="boris-mascot boris-mascot--inline q-mr-md" />
        <div>
          <h1 class="text-h5 q-my-none"><q-icon :name="stepIcon" size="28px" class="q-mr-sm" />{{ title }}</h1>
          <p class="text-body1">{{ description }}</p>
        </div>
      </div>
      <q-banner v-if="error" class="bg-negative text-white q-mb-md">
        <template #avatar><q-icon name="error" /></template>
        {{ error }}<template #action><q-btn flat label="Retry" @click="retry" /></template>
      </q-banner>
      <q-banner v-if="!connected" class="bg-warning q-mb-md">
        <template #avatar><q-icon name="sync_problem" /></template>
        Waiting for the local setup service…
      </q-banner>

      <q-card v-if="step === 'welcome'" flat bordered><q-card-section>
        <div class="row items-center q-mb-md">
          <img src="../../../core/src/images/boris.png" alt="Boris the Bareos wizard"
            class="boris-mascot boris-mascot--welcome q-mr-lg" />
          <q-list dense class="col">
            <q-item v-for="component in installedComponents" :key="component.label">
              <q-item-section avatar><q-icon :name="component.icon" color="primary" /></q-item-section>
              <q-item-section>
                <q-item-label>{{ component.label }}</q-item-label>
                <q-item-label caption>{{ component.detail }}</q-item-label>
              </q-item-section>
            </q-item>
          </q-list>
        </div>
        <q-list dense>
          <q-item><q-item-section avatar><q-icon name="rocket_launch" color="positive" /></q-item-section>
            <q-item-section>Installs and configures Director, Storage Daemon, File Daemon, PostgreSQL, and the WebUI — all in one guided flow.</q-item-section></q-item>
          <q-item><q-item-section avatar><q-icon name="cloud_download" color="positive" /></q-item-section>
            <q-item-section>Adds the official Bareos repository and installs packages using your distribution's standard package manager.</q-item-section></q-item>
          <q-item><q-item-section avatar><q-icon name="verified_user" color="positive" /></q-item-section>
            <q-item-section>Secure by default: TLS and strong admin passwords are generated automatically.</q-item-section></q-item>
          <q-item><q-item-section avatar><q-icon name="health_and_safety" color="positive" /></q-item-section>
            <q-item-section>Safe to run: existing Bareos installations and configurations are never modified.</q-item-section></q-item>
          <q-item><q-item-section avatar><q-icon name="admin_panel_settings" color="positive" /></q-item-section>
            <q-item-section>Transparent privilege use: only standard, approved commands are ever executed as root or via sudo — nothing else.</q-item-section></q-item>
        </q-list>
      </q-card-section></q-card>
      <q-card v-else-if="step === 'platform'" flat bordered><q-card-section>
        <div v-if="store.state.distro" class="row items-center">
          <q-icon v-if="distroIcon" size="3em" class="q-mr-md" :style="{ color: '#' + distroIcon.hex }">
            <svg viewBox="0 0 24 24"><path :d="distroIcon.path" fill="currentColor" /></svg>
          </q-icon>
          <q-icon v-else :name="FALLBACK_ICON" color="primary" size="3em" class="q-mr-md" />
          <div>
            <div class="text-h6">{{ store.state.pretty_name || store.state.distro }}</div>
            <div class="row q-gutter-xs q-mt-xs">
              <q-chip dense icon="inventory_2" color="grey-3">{{ store.state.package_manager }}</q-chip>
              <q-chip v-if="store.state.arch" dense icon="memory" color="grey-3">{{ store.state.arch }}</q-chip>
              <q-chip v-if="store.state.codename" dense icon="label" color="grey-3">{{ store.state.codename }}</q-chip>
            </div>
          </div>
        </div>
        <q-spinner v-else-if="!error" size="2em" />
      </q-card-section></q-card>
      <q-card v-else-if="step === 'repository'" flat bordered><q-card-section>
        <q-card flat bordered class="q-mb-md cursor-pointer repo-card repo-card--subscription"
          :class="{ 'repo-card--selected': store.repository === 'subscription' }"
          @click="store.repository = 'subscription'">
          <q-card-section>
            <div class="row items-center no-wrap">
              <q-radio v-model="store.repository" val="subscription" color="primary" />
              <div class="col">
                <div class="row items-center">
                  <span class="text-weight-bold text-primary">Bareos Subscription</span>
                  <q-chip dense color="primary" text-color="white" icon="star" class="q-ml-sm">Recommended</q-chip>
                </div>
                <div class="text-caption text-grey-8">Vendor-supported packages with priority updates and support.</div>
              </div>
            </div>
            <q-banner dense class="bg-blue-1 text-primary q-mt-md" @click.stop>
              <template #avatar><q-icon name="workspace_premium" color="primary" /></template>
              New to Bareos Subscription? Request free evaluation access —
              test new features, plugins, and integrations, with fast approval.
              <template #action>
                <q-btn flat dense color="primary" icon-right="open_in_new" label="Get evaluation access"
                  type="a" href="https://www.bareos.com/try/" target="_blank" rel="noopener" />
              </template>
            </q-banner>
            <template v-if="store.repository === 'subscription'">
              <q-input v-model="store.repositoryLogin" label="Subscription login" autocomplete="off"
                class="q-mt-md" @click.stop>
                <template #prepend><q-icon name="person" /></template>
              </q-input>
              <q-input v-model="store.repositoryPassword" label="Subscription password" type="password"
                autocomplete="new-password" @click.stop>
                <template #prepend><q-icon name="key" /></template>
              </q-input>
            </template>
          </q-card-section>
        </q-card>
        <q-card flat class="cursor-pointer repo-card repo-card--community"
          @click="store.repository = 'community'">
          <q-card-section>
            <div class="row items-center no-wrap">
              <q-radio v-model="store.repository" val="community" color="grey" dense />
              <span class="text-caption text-grey-7">Bareos Community (unsupported, community-maintained packages)</span>
            </div>
          </q-card-section>
        </q-card>
      </q-card-section></q-card>
      <q-card v-else-if="step === 'storage'" flat bordered><q-card-section>
        <q-toggle v-model="store.customizeStorage" label="Customize disk storage path" />
        <q-input v-if="store.customizeStorage" v-model="store.storagePath"
          label="Disk storage path" hint="Absolute path on this host">
          <template #prepend><q-icon name="folder" /></template>
        </q-input>
        <p v-else><q-icon name="storage" color="primary" class="q-mr-xs" />Disk storage: <code>/var/lib/bareos/storage</code></p>
      </q-card-section></q-card>
      <q-card v-else-if="step === 'progress'" flat bordered><q-card-section>
        <q-list dense bordered separator>
          <q-expansion-item v-for="item in installSteps" :key="item.id"
            v-model="expandedSteps[item.id]">
            <template #header>
              <q-item-section avatar>
                <q-spinner v-if="item.id === currentStepId && !done(item.id)" color="primary" size="24px" />
                <q-icon v-else :name="done(item.id) ? 'check_circle' : 'radio_button_unchecked'"
                  :color="done(item.id) ? 'positive' : 'grey'" size="24px" />
              </q-item-section>
              <q-item-section :class="{ 'text-primary text-weight-bold': item.id === currentStepId && !done(item.id) }">
                {{ item.label }}
              </q-item-section>
            </template>
            <q-card>
              <q-card-section>
                <pre v-if="stepLogs[item.id]" class="output-console"
                  :ref="el => { if (el) logRefs[item.id] = el }">{{ stepLogs[item.id] }}</pre>
                <p v-else class="text-grey">No output yet.</p>
              </q-card-section>
            </q-card>
          </q-expansion-item>
        </q-list>
      </q-card-section></q-card>
      <q-card v-else flat bordered><q-card-section>
        <q-icon name="check_circle" color="positive" size="3em" />
        <p>Installation and service verification completed.</p>
        <p v-if="store.admin"><q-icon name="vpn_key" color="warning" class="q-mr-xs" />Save this initial WebUI password now:
          <code>{{ store.admin.username }} / {{ store.admin.password }}</code>
          <q-btn flat dense icon="content_copy" label="Copy password" class="q-ml-sm"
            @click="copyAdminPassword" />
          <br>
          Copy the password and store it somewhere safe. If it is lost, you can
          find it on this host in
          <code>/etc/bareos/bareos-dir.d/console/admin.conf</code>.
        </p>
        <p><q-icon name="open_in_browser" color="primary" class="q-mr-xs" />Log in to the Bareos WebUI at:
          <a :href="webuiUrl" target="_blank" rel="noopener noreferrer">{{ webuiUrl }}</a>
        </p>
        <q-banner v-if="setupClosed" class="bg-positive text-white q-mb-md" rounded>
          <template #avatar><q-icon name="check_circle" /></template>
          Setup wizard closed. You can close this browser tab now.
        </q-banner>
        <q-btn color="primary" label="Close setup wizard" icon="power_settings_new"
          :disable="setupClosed" @click="closeSetup" />
      </q-card-section></q-card>

      <div class="row justify-between q-mt-lg">
        <q-btn v-if="step !== 'welcome' && step !== 'complete' && !(step === 'progress' && installStarted)"
          flat icon="arrow_back" label="Back" @click="back" />
        <q-space />
        <q-btn v-if="step !== 'complete'" color="primary"
          :disable="busy || !connected || !canContinue"
          :icon-right="step === 'progress' ? 'play_arrow' : 'arrow_forward'"
          :label="step === 'progress' ? 'Start installation' : 'Continue'" @click="next" />
        <q-btn v-if="step === 'progress' && failed" flat color="negative"
          icon="undo" label="Rollback" @click="rollback" />
      </div>
    </q-page></q-page-container>
  </q-layout>
</template>

<script setup>
import { computed, onMounted, reactive, ref, watch, nextTick } from 'vue'
import { useQuasar } from 'quasar'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'
import { getDistroIcon, FALLBACK_ICON } from '../utils/distro-icons.js'

const $q = useQuasar()
const store = useSetupStore()
const { connected, messages, send } = useSetupWs()
const step = ref('welcome')
const busy = ref(false)
const error = ref('')
const failed = ref(false)
const installStarted = ref(false)
const currentStepId = ref(null)
const setupClosed = ref(false)
const logRefs = {}
const distroIcon = computed(() => getDistroIcon(store.state.distro))
const installSteps = [
  { id: 'repository', label: 'Configure repository' }, { id: 'packages', label: 'Install packages' },
  { id: 'storage', label: 'Apply optional disk storage path' },
  { id: 'catalog', label: 'Initialize catalog when needed' }, { id: 'admin', label: 'Create initial admin account' },
  { id: 'proxy', label: 'Enable loopback WebUI proxy' }, { id: 'smoke_test', label: 'Verify required services' }]
const stepLogs = reactive(Object.fromEntries(installSteps.map(item => [item.id, ''])))
const expandedSteps = reactive(Object.fromEntries(installSteps.map(item => [item.id, false])))
const installedComponents = [
  { icon: 'hub', label: 'Director', detail: 'Job scheduling and catalog control' },
  { icon: 'save', label: 'Storage Daemon', detail: 'Writes backup data' },
  { icon: 'folder_copy', label: 'File Daemon', detail: "Backs up this host's files" },
  { icon: 'storage', label: 'PostgreSQL', detail: 'Catalog database' },
  { icon: 'web', label: 'WebUI', detail: 'Browser-based management interface' }]
const title = computed(() => step.value === 'complete'
  ? 'Setup complete' : ({ welcome: 'Welcome', platform: 'Platform', repository: 'Repository',
    storage: 'Storage', progress: 'Installation progress' }[step.value]))
const stepIcon = computed(() => ({ welcome: 'rocket_launch', platform: 'computer', repository: 'cloud_download',
  storage: 'storage', progress: 'settings', complete: 'check_circle' }[step.value] || 'rocket_launch'))
const description = computed(() => ({
  welcome: 'Bareos Setup will guide you through the installation and setup of Bareos and required services on this computer.',
  platform: 'Detecting your Linux distribution and package manager to select compatible packages.',
  repository: 'Choose which Bareos package repository to install from.',
  storage: 'Configure where Bareos should store backup data on this host.',
  progress: 'Installation stops on failure. Retry the failed step or roll back wizard-owned changes.',
}[step.value] || ''))
const progress = computed(() => step.value === 'complete' ? 1 :
  ({ welcome: 0, platform: .2, repository: .35, storage: .5, progress: .7 }[step.value] || 0))
const canContinue = computed(() => step.value !== 'repository' ||
  store.repository === 'community' || (store.repositoryLogin && store.repositoryPassword))
const webuiUrl = computed(() => {
  const host = window.location.hostname.includes(':')
    ? `[${window.location.hostname}]`
    : window.location.hostname
  return `https://${host}/bareos-webui-new`
})
function done(id) { return store.state.completed.includes(id) }
function next() {
  if (step.value === 'welcome') { step.value = 'platform'; send({ action: 'state' }) }
  else if (step.value === 'platform') step.value = 'repository'
  else if (step.value === 'repository') step.value = 'storage'
  else if (step.value === 'storage') step.value = 'progress'
  else if (step.value === 'progress') start()
}
function back() { step.value = ({ platform: 'welcome', repository: 'platform', storage: 'repository', progress: 'storage' })[step.value] || step.value }
function payload() {
  return { repository: store.repository, repository_login: store.repositoryLogin,
    repository_password: store.repositoryPassword, distro: store.state.distro, version: store.state.version,
    storage_path: store.customizeStorage ? store.storagePath : '/var/lib/bareos/storage' }
}
function start() {
  installStarted.value = true
  busy.value = true; error.value = ''; failed.value = false
  installSteps.forEach(item => { stepLogs[item.id] = ''; expandedSteps[item.id] = false })
  currentStepId.value = 'repository'
  expandedSteps.repository = true
  send({ action: 'run', step: 'repository', ...payload() })
}
function retry() { start() }
function rollback() { send({ action: 'rollback' }); busy.value = false; failed.value = false; currentStepId.value = null }
async function copyAdminPassword() {
  if (!store.admin?.password) return
  try {
    await navigator.clipboard.writeText(store.admin.password)
    $q.notify({ type: 'positive', message: 'Password copied to clipboard.' })
  } catch (_) {
    $q.notify({ type: 'negative', message: 'Could not copy password to clipboard.' })
  }
}
function closeSetup() { send({ action: 'close' }) }
watch(messages, list => {
  const message = list[list.length - 1]; if (!message) return
  if (message.type === 'state') { Object.assign(store.state, message) }
  if (message.type === 'output' && currentStepId.value) stepLogs[currentStepId.value] += `${message.line}\n`
  if (message.type === 'error') { error.value = message.message; failed.value = true; busy.value = false }
  if (message.type === 'admin_credentials') store.admin = message
  if (message.type === 'rollback_complete') store.state.completed = []
  if (message.type === 'closed') setupClosed.value = true
  if (message.type === 'done') {
    if (message.exit_code) { failed.value = true; busy.value = false; return }
    if (!store.state.completed.includes(message.step)) store.state.completed.push(message.step)
    expandedSteps[message.step] = false
    const nextStep = installSteps.find(item => !done(item.id))
    if (nextStep) {
      currentStepId.value = nextStep.id
      expandedSteps[nextStep.id] = true
      send({ action: 'run', step: nextStep.id, ...payload() })
    } else { busy.value = false; currentStepId.value = null; step.value = 'complete' }
  }
}, { deep: true })
watch(() => currentStepId.value && stepLogs[currentStepId.value], async () => {
  const id = currentStepId.value; if (!id) return
  await nextTick()
  const el = logRefs[id]
  if (el) el.scrollTop = el.scrollHeight
})
onMounted(() => send({ action: 'state' }))
</script>

<style scoped>
.toolbar-logo {
  width: 28px;
  height: 28px;
}
.boris-mascot {
  flex: 0 0 auto;
}
.boris-mascot--inline {
  width: 80px;
  height: 80px;
}
.boris-mascot--welcome {
  width: 200px;
  height: 200px;
}
.repo-card--subscription {
  border-color: var(--q-primary);
}
.repo-card--selected {
  box-shadow: 0 0 0 2px var(--q-primary);
}
.repo-card--community {
  opacity: 0.85;
}
</style>
