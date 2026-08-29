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
        <q-markup-table v-if="store.state.distro" flat bordered dense>
          <tbody>
            <tr v-for="row in platformRows" :key="row.label">
              <th class="text-left platform-table__label">{{ row.label }}</th>
              <td>
                <span v-if="row.icon" class="row items-center no-wrap">
                  <svg v-if="platformDistroIcon" class="platform-distro-icon q-mr-sm"
                    viewBox="0 0 24 24" role="img" :aria-label="platformIconLabel">
                    <path :fill="`#${platformDistroIcon.hex}`" :d="platformDistroIcon.path" />
                  </svg>
                  <q-icon v-else :name="FALLBACK_ICON" size="24px" class="q-mr-sm"
                    :aria-label="platformIconLabel" />
                  {{ row.value }}
                </span>
                <template v-else>{{ row.value }}</template>
              </td>
            </tr>
          </tbody>
        </q-markup-table>
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
            <q-card flat bordered class="bg-blue-1 q-mt-md" @click.stop>
              <q-card-section>
                <div class="row items-center q-mb-sm">
                  <q-icon name="workspace_premium" color="primary" size="28px" class="q-mr-sm" />
                  <div>
                    <div class="text-subtitle2 text-primary">Try Bareos Subscription features</div>
                    <div class="text-caption text-grey-8">
                      Evaluation access includes subscription packages and plugins.
                    </div>
                  </div>
                </div>
                <q-list dense class="q-mb-sm">
                  <q-item v-for="feature in subscriptionHighlights" :key="feature.label">
                    <q-item-section avatar>
                      <q-icon :name="feature.icon" color="primary" />
                    </q-item-section>
                    <q-item-section>
                      <q-item-label>{{ feature.label }}</q-item-label>
                      <q-item-label caption>{{ feature.caption }}</q-item-label>
                    </q-item-section>
                  </q-item>
                </q-list>
                <q-btn unelevated dense color="primary" icon-right="open_in_new"
                  label="Get evaluation access" type="a"
                  href="https://www.bareos.com/try/" target="_blank"
                  rel="noopener" />
              </q-card-section>
            </q-card>
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
              <span class="text-caption text-grey-7 q-ml-sm">Bareos Community (unsupported, community-maintained packages)</span>
            </div>
          </q-card-section>
        </q-card>
      </q-card-section></q-card>
      <q-card v-else-if="step === 'progress'" flat bordered><q-card-section>
        <q-list dense bordered separator>
          <q-expansion-item v-for="item in installSteps" :key="item.id"
            v-model="expandedSteps[item.id]">
            <template #header>
              <q-item-section avatar>
                <q-spinner v-if="item.id === currentStepId && !done(item.id)" color="primary" size="24px" />
                <q-icon v-else :name="done(item.id) ? 'check_circle' : item.icon"
                  :color="done(item.id) ? 'positive' : 'grey-6'" size="24px" />
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
        <q-banner v-if="installFinished" class="bg-positive text-white q-mt-md" rounded>
          <template #avatar><q-icon name="check_circle" /></template>
          Installation steps completed. Review the setup summary for login
          options and next steps.
          <template #action>
            <q-btn flat color="white" label="Show setup summary"
              @click="step = 'complete'" />
          </template>
        </q-banner>
      </q-card-section></q-card>
      <q-card v-else flat bordered><q-card-section>
        <q-icon name="check_circle" color="positive" size="3em" />
        <p>Installation and service verification completed.</p>
        <p v-if="adminPasswordVisible"><q-icon name="vpn_key" color="warning" class="q-mr-xs" />Save this initial WebUI password now:
          <code>{{ store.admin.username }} / {{ store.admin.password }}</code>
          <q-btn flat dense icon="content_copy" label="Copy password" class="q-ml-sm"
            @click="copyAdminPassword" />
          <br>
          Copy the password and store it somewhere safe. If it is lost, you can
          find it on this host in
          <code>/etc/bareos/bareos-dir.d/console/admin.conf</code>.
        </p>
        <q-banner v-else-if="adminPasswordPrintedToTerminal" class="bg-info text-white q-mb-md" rounded>
          <template #avatar><q-icon name="terminal" /></template>
          The initial WebUI admin password was printed on the terminal where
          <code>bareos-setup</code> is running. It is not shown here because
          this browser connection is not local loopback.
        </q-banner>
        <p><q-icon name="open_in_browser" color="primary" class="q-mr-xs" />Log in to the Bareos WebUI at one of these URLs:</p>
        <q-markup-table v-if="webuiUrls.length" flat bordered dense class="q-mb-md">
          <tbody>
            <tr v-for="url in webuiUrls" :key="url.href">
              <th class="text-left webui-url-table__label">{{ url.label }}</th>
              <td><a :href="url.href" target="_blank" rel="noopener noreferrer">{{ url.href }}</a></td>
            </tr>
          </tbody>
        </q-markup-table>
        <p><q-icon name="terminal" color="primary" class="q-mr-xs" />You can
          also manage this Bareos system locally with <code>bconsole</code>.</p>
        <q-card flat bordered class="q-mb-md"><q-card-section>
          <div class="text-subtitle1 q-mb-sm">Possible next steps</div>
          <q-list dense>
            <q-item v-for="item in nextStepLinks" :key="item.href"
              clickable tag="a" :href="item.href" target="_blank"
              rel="noopener noreferrer">
              <q-item-section avatar>
                <q-icon :name="item.icon" color="primary" />
              </q-item-section>
              <q-item-section>
                <q-item-label>{{ item.label }}</q-item-label>
                <q-item-label caption>{{ item.caption }}</q-item-label>
              </q-item-section>
              <q-item-section side><q-icon name="open_in_new" /></q-item-section>
            </q-item>
          </q-list>
        </q-card-section></q-card>
        <q-expansion-item v-if="hasInstallationLog" icon="article" label="Installation log"
          caption="Redacted commands and output from this setup run" class="q-mb-md"
          expand-separator>
          <q-card>
            <q-card-section>
              <div class="row q-gutter-sm q-mb-sm">
                <q-btn flat dense icon="content_copy" label="Copy log"
                  @click="copyInstallationLog" />
                <q-btn flat dense icon="download" label="Download log"
                  @click="downloadInstallationLog" />
              </div>
              <pre class="output-console">{{ installationLog }}</pre>
            </q-card-section>
          </q-card>
        </q-expansion-item>
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
          :icon-right="primaryActionIcon"
          :label="primaryActionLabel" @click="next" />
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
import { FALLBACK_ICON, getDistroIcon } from '../utils/distro-icons.js'

const $q = useQuasar()
const store = useSetupStore()
const { connected, messages, send } = useSetupWs()
const step = ref('welcome')
const busy = ref(false)
const error = ref('')
const failed = ref(false)
const installStarted = ref(false)
const installFinished = ref(false)
const currentStepId = ref(null)
const setupClosed = ref(false)
const logRefs = {}
const platformRows = computed(() => [
  { label: 'Distribution', value: store.state.pretty_name || store.state.distro, icon: true },
  { label: 'Distribution ID', value: store.state.distro },
  { label: 'Version', value: store.state.version },
  { label: 'Codename', value: store.state.codename },
  { label: 'Architecture', value: store.state.arch },
  { label: 'Package manager', value: store.state.package_manager },
  { label: 'Setup version', value: store.state.setup_version },
].filter(row => row.value))
const installSteps = [
  { id: 'repository', label: 'Configure repository', icon: 'cloud_download' },
  { id: 'packages', label: 'Install packages', icon: 'inventory_2' },
  { id: 'catalog', label: 'Initialize catalog when needed', icon: 'storage' },
  { id: 'admin', label: 'Create initial admin account', icon: 'admin_panel_settings' },
  { id: 'proxy', label: 'Enable loopback WebUI proxy', icon: 'lan' },
  { id: 'smoke_test', label: 'Verify required services', icon: 'verified' }]
const stepLogs = reactive(Object.fromEntries(installSteps.map(item => [item.id, ''])))
const expandedSteps = reactive(Object.fromEntries(installSteps.map(item => [item.id, false])))
const installedComponents = [
  { icon: 'hub', label: 'Director', detail: 'Job scheduling and catalog control' },
  { icon: 'save', label: 'Storage Daemon', detail: 'Writes backup data' },
  { icon: 'folder_copy', label: 'File Daemon', detail: "Backs up this host's files" },
  { icon: 'storage', label: 'PostgreSQL', detail: 'Catalog database' },
  { icon: 'web', label: 'WebUI', detail: 'Browser-based management interface' }]
const subscriptionHighlights = [
  {
    icon: 'developer_board',
    label: 'Hyper-V plugin',
    caption: 'Protect Microsoft Hyper-V virtualization workloads.',
  },
  {
    icon: 'restore',
    label: 'Windows disaster recovery with Bareos Barri',
    caption: 'Evaluate Bareos-assisted Windows recovery workflows.',
  },
  {
    icon: 'account_tree',
    label: 'Proxmox plugin',
    caption: 'Back up Proxmox virtualization environments.',
  },
]
const title = computed(() => step.value === 'complete'
  ? 'Setup complete' : ({ welcome: 'Welcome', platform: 'Platform', repository: 'Repository',
    progress: 'Installation progress' }[step.value]))
const platformDistroIcon = computed(() => step.value === 'platform'
  ? getDistroIcon(store.state.distro) : null)
const platformIconLabel = computed(() =>
  `${store.state.pretty_name || store.state.distro || 'Detected platform'} icon`)
const stepIcon = computed(() => ({ welcome: 'rocket_launch', platform: 'computer', repository: 'cloud_download',
  progress: 'settings', complete: 'check_circle' }[step.value] || 'rocket_launch'))
const description = computed(() => ({
  welcome: 'Bareos Setup will guide you through the installation and setup of Bareos and required services on this computer.',
  platform: 'Detecting your Linux distribution and package manager to select compatible packages.',
  repository: 'Choose which Bareos package repository to install from.',
  progress: 'Installation stops on failure. Retry the failed step or roll back wizard-owned changes.',
}[step.value] || ''))
const progress = computed(() => step.value === 'complete' ? 1 :
  ({ welcome: 0, platform: .2, repository: .45, progress: .7 }[step.value] || 0))
const canContinue = computed(() => step.value !== 'repository' ||
  store.repository === 'community' || (store.repositoryLogin && store.repositoryPassword))
const primaryActionLabel = computed(() => {
  if (step.value === 'progress') {
    return installFinished.value ? 'Show setup summary' : 'Start installation'
  }
  return 'Continue'
})
const primaryActionIcon = computed(() => {
  if (step.value === 'progress') {
    return installFinished.value ? 'check_circle' : 'play_arrow'
  }
  return 'arrow_forward'
})
const adminPasswordVisible = computed(() => Boolean(store.admin?.password))
const adminPasswordPrintedToTerminal = computed(() =>
  Boolean(store.admin?.password_printed_to_terminal))
const nextStepLinks = computed(() => [
  {
    icon: 'menu_book',
    label: 'Read the Bareos documentation',
    caption: 'Learn how to configure jobs, schedules, clients, storage, and restores.',
    href: 'https://docs.bareos.org/',
  },
  ...(store.repository === 'subscription' ? [] : [{
    icon: 'workspace_premium',
    label: 'Request Bareos trial access',
    caption: 'Try subscription packages, plugins, and vendor-supported builds.',
    href: 'https://www.bareos.com/try/',
  }]),
  {
    icon: 'calculate',
    label: 'Open the Bareos pricing calculator',
    caption: 'Estimate subscription, support, and service options for your setup.',
    href: 'https://www.bareos.com/pricing/',
  },
  {
    icon: 'groups',
    label: 'Join a Bareos Expert Circle',
    caption: 'Meet Bareos experts and discuss backup topics, releases, and operations.',
    href: 'https://www.bareos.com/meet/',
  },
  {
    icon: 'support_agent',
    label: 'Explore consulting, support, and funded development',
    caption: 'Get expert help for setup review, migration, operations, and new features.',
    href: 'https://www.bareos.com/services/',
  },
])
function formatUrlHost(host) {
  const value = host.trim()
  if (!value) return ''
  if (value.includes(':') && !(value.startsWith('[') && value.endsWith(']'))) {
    return `[${value}]`
  }
  return value
}
const webuiUrls = computed(() => {
  const candidates = [
    { label: 'Loopback IPv4', host: '127.0.0.1' },
    { label: 'Localhost', host: 'localhost' },
    { label: 'Hostname', host: store.state.hostname },
    { label: 'Current browser host', host: window.location.hostname },
  ]
  const seen = new Set()
  return candidates.flatMap(candidate => {
    const host = formatUrlHost(candidate.host || '')
    if (!host) return []
    const href = `https://${host}/bareos-webui-new`
    const key = href.toLowerCase()
    if (seen.has(key)) return []
    seen.add(key)
    return [{ label: candidate.label, href }]
  })
})
const installationLog = computed(() => installSteps.map(item => {
  const log = stepLogs[item.id].trimEnd()
  if (!log) return ''
  return `## ${item.label}\n${log}`
}).filter(Boolean).join('\n\n'))
const hasInstallationLog = computed(() => installationLog.value.length > 0)
async function copyInstallationLog() {
  if (!installationLog.value) return
  try {
    await navigator.clipboard.writeText(installationLog.value)
    $q.notify({ type: 'positive', message: 'Installation log copied.' })
  } catch (_) {
    $q.notify({ type: 'negative', message: 'Could not copy installation log.' })
  }
}
function downloadInstallationLog() {
  if (!installationLog.value) return
  const blob = new Blob([installationLog.value], { type: 'text/plain' })
  const url = URL.createObjectURL(blob)
  const anchor = document.createElement('a')
  anchor.href = url
  anchor.download = 'bareos-setup-installation.log'
  anchor.click()
  URL.revokeObjectURL(url)
}
function done(id) { return store.state.completed.includes(id) }
function next() {
  if (step.value === 'welcome') { step.value = 'platform'; send({ action: 'state' }) }
  else if (step.value === 'platform') step.value = 'repository'
  else if (step.value === 'repository') step.value = 'progress'
  else if (step.value === 'progress' && installFinished.value) step.value = 'complete'
  else if (step.value === 'progress') start()
}
function back() { step.value = ({ platform: 'welcome', repository: 'platform', progress: 'repository' })[step.value] || step.value }
function payload() {
  return { repository: store.repository, repository_login: store.repositoryLogin,
    repository_password: store.repositoryPassword, distro: store.state.distro, version: store.state.version }
}
function start() {
  installStarted.value = true
  installFinished.value = false
  busy.value = true; error.value = ''; failed.value = false
  store.admin = null
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
  if (message.type === 'closed') {
    setupClosed.value = true
    store.admin = null
  }
  if (message.type === 'done') {
    if (message.exit_code) { failed.value = true; busy.value = false; return }
    if (!store.state.completed.includes(message.step)) store.state.completed.push(message.step)
    expandedSteps[message.step] = false
    const nextStep = installSteps.find(item => !done(item.id))
    if (nextStep) {
      currentStepId.value = nextStep.id
      expandedSteps[nextStep.id] = true
      send({ action: 'run', step: nextStep.id, ...payload() })
    } else {
      store.repositoryLogin = ''
      store.repositoryPassword = ''
      busy.value = false
      currentStepId.value = null
      installFinished.value = true
    }
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
.platform-distro-icon {
  width: 24px;
  height: 24px;
  flex: 0 0 24px;
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
.platform-table__label {
  width: 12rem;
}
.webui-url-table__label {
  width: 12rem;
}
</style>
