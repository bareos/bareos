<template>
  <q-layout view="hHh lpR fFf">

    <!-- ── Side drawer (mobile / tablet) ──────────────────────────────────── -->
    <q-drawer v-model="drawerOpen" side="left" overlay behavior="mobile"
              :width="240" class="bareos-toolbar column no-wrap">
      <q-scroll-area class="col">
        <!-- Drawer header -->
        <div class="q-pa-md row items-center">
          <span style="height:32px; margin-right:8px; display:inline-flex; align-items:center">
            <img :src="bareosLogo" alt="Bareos" style="height:32px" />
            <q-tooltip>Bareos WebUI {{ appVersion }}</q-tooltip>
          </span>
          <span class="text-white text-weight-bold text-h6" style="letter-spacing:0.02em">
            BAREOS
          </span>
        </div>
        <q-separator dark />

        <!-- Navigation items -->
        <q-list dark>
          <q-item v-for="nav in mainNavItems" :key="nav.to"
                  :data-testid="nav.drawerTestId"
                  clickable v-ripple :to="nav.to" exact
                  active-class="bg-white text-primary"
                  @click="drawerOpen = false">
            <q-item-section avatar><q-icon :name="nav.icon" /></q-item-section>
            <q-item-section>{{ nav.label }}</q-item-section>
          </q-item>
        </q-list>

        <q-separator dark class="q-my-sm" />

        <!-- Director status -->
        <q-list dark>
          <q-item>
            <q-item-section avatar>
              <q-icon :name="dirStatusIcon" :color="dirStatusColor" />
            </q-item-section>
            <q-item-section class="text-white text-caption">
              {{ dirStatusLabel }}
            </q-item-section>
          </q-item>
          <q-item>
            <q-item-section avatar>
              <q-icon name="dns" color="white" />
            </q-item-section>
            <q-item-section class="text-white text-caption">
              {{ scopeLabel }}
            </q-item-section>
          </q-item>
          <q-item-label header class="text-grey-4">
            {{ t('Directors') }}
          </q-item-label>
          <DirectorScopeMenuContent
            v-model="selectedDirectorsModel"
            :options="directorMenuOptions"
            :console-directors="scopeConsoleDirectors"
            :authenticated-directors="auth.authenticatedDirectors"
            data-test-id="director-scope-drawer"
            @open-console="openConsole($event); drawerOpen = false"
            @login-director="openDirectorLogin($event); drawerOpen = false"
          />
        </q-list>

        <q-separator dark class="q-my-sm" />

        <!-- Settings / account -->
        <q-list dark>
          <q-item clickable v-ripple :to="{ name: 'acls' }" @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="verified_user" /></q-item-section>
            <q-item-section>{{ t('Command ACL') }}</q-item-section>
          </q-item>
          <q-item clickable v-ripple :to="{ name: 'settings' }" @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="tune" /></q-item-section>
            <q-item-section>{{ t('Settings') }}</q-item-section>
          </q-item>
          <q-item clickable v-ripple
                  tag="a" href="https://docs.bareos.org" target="_blank"
                  rel="noopener noreferrer"
                  @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="menu_book" /></q-item-section>
            <q-item-section>{{ t('Documentation') }}</q-item-section>
          </q-item>
          <q-item clickable v-ripple
                  tag="a" href="https://github.com/bareos/bareos/issues/" target="_blank"
                  rel="noopener noreferrer"
                  @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="bug_report" /></q-item-section>
            <q-item-section>{{ t('Issue Tracker') }}</q-item-section>
          </q-item>
          <q-separator dark />
          <q-item clickable v-ripple @click="logout">
            <q-item-section avatar><q-icon name="logout" /></q-item-section>
            <q-item-section>{{ t('Logout') }}</q-item-section>
          </q-item>
        </q-list>
      </q-scroll-area>
    </q-drawer>

    <!-- ── Top Navbar ──────────────────────────────────────────────────────── -->
    <q-header class="bareos-toolbar">
      <q-toolbar>

        <!-- Hamburger (mobile / tablet) -->
        <q-btn v-if="$q.screen.lt.md" flat round dense icon="menu" color="white"
               class="q-mr-sm" @click="drawerOpen = !drawerOpen" />

        <!-- Logo -->
        <router-link to="/dashboard" style="display:inline-flex; align-items:center">
          <img :src="bareosLogo" alt="Bareos"
               style="height:36px; margin-right:8px;" />
          <q-tooltip>Bareos WebUI {{ appVersion }}</q-tooltip>
        </router-link>
        <span class="text-white text-weight-bold text-h6" style="letter-spacing:0.02em">
          BAREOS
        </span>

        <!-- Main nav tabs (desktop only) -->
        <q-tabs v-if="!$q.screen.lt.md" dense align="left"
                class="q-ml-md" indicator-color="white"
                active-color="white" active-bg-color="rgba(255,255,255,0.15)">
          <q-route-tab v-for="nav in mainNavItems" :key="nav.to"
                       :data-testid="nav.testId"
                       :name="nav.label.toLowerCase()" :to="nav.to"
                       :label="nav.label" no-caps />
        </q-tabs>

        <q-space />

        <q-btn
          v-if="directorUpdateAlert"
          flat
          round
          dense
          :icon="directorUpdateAlert.icon"
          :color="directorUpdateAlert.color"
          :href="RELEASE_INFO_PAGE_URL"
          target="_blank"
          rel="noopener noreferrer"
        >
          <q-tooltip>{{ directorUpdateAlert.message }}</q-tooltip>
        </q-btn>

        <!-- Desktop-only: director menu, user menu -->
        <template v-if="!$q.screen.lt.md">

          <q-btn
            flat
            color="white"
            icon="dns"
            no-caps
            class="director-scope-control"
            data-testid="director-scope-control"
          >
            <span class="director-scope-control__label ellipsis">
              {{ headerScopeLabel }}
            </span>
            <q-tooltip>{{ scopeLabel }}</q-tooltip>
            <q-menu>
              <q-list dense style="min-width:160px">
                <q-item-label header>{{ t('Directors') }}</q-item-label>
                <DirectorScopeMenuContent
                  v-model="selectedDirectorsModel"
                  :options="directorMenuOptions"
                  :console-directors="scopeConsoleDirectors"
                  :authenticated-directors="auth.authenticatedDirectors"
                  :close-on-console="true"
                  data-test-id="director-scope-menu"
                  @open-console="openConsole"
                  @login-director="openDirectorLogin"
                />
              </q-list>
            </q-menu>
          </q-btn>

          <q-btn flat color="white" :label="accountMenuLabel" icon="person" no-caps>
            <q-menu>
              <q-list dense style="min-width:180px">
                <q-item
                  v-for="session in accountDirectorSessions"
                  :key="session.director"
                  dense
                >
                  <q-item-section avatar>
                    <q-icon name="dns" />
                  </q-item-section>
                  <q-item-section>
                    <q-item-label>{{ session.username }}</q-item-label>
                    <q-item-label caption>{{ session.director }}</q-item-label>
                  </q-item-section>
                </q-item>
                <q-separator v-if="accountDirectorSessions.length > 0" />
                <q-item clickable v-close-popup :to="{ name: 'acls' }">
                  <q-item-section avatar><q-icon name="verified_user" /></q-item-section>
                  <q-item-section>{{ t('Command ACL') }}</q-item-section>
                </q-item>
                <q-item clickable v-close-popup :to="{ name: 'settings' }">
                  <q-item-section avatar><q-icon name="tune" /></q-item-section>
                  <q-item-section>{{ t('Settings') }}</q-item-section>
                </q-item>
                <q-separator />
                <q-item clickable tag="a" href="https://docs.bareos.org" target="_blank" rel="noopener noreferrer" v-close-popup>
                  <q-item-section avatar><q-icon name="menu_book" /></q-item-section>
                  <q-item-section>{{ t('Documentation') }}</q-item-section>
                </q-item>
                <q-item clickable tag="a" href="https://github.com/bareos/bareos/issues/" target="_blank" rel="noopener noreferrer" v-close-popup>
                  <q-item-section avatar><q-icon name="bug_report" /></q-item-section>
                  <q-item-section>{{ t('Issue Tracker') }}</q-item-section>
                </q-item>
                <q-separator />
                <q-item clickable v-close-popup @click="logout">
                  <q-item-section avatar><q-icon name="logout" /></q-item-section>
                  <q-item-section>{{ t('Logout') }}</q-item-section>
                </q-item>
              </q-list>
            </q-menu>
          </q-btn>
        </template>

      </q-toolbar>
    </q-header>

    <q-page-container>
      <q-banner
        v-if="missingDirectorLogins.length > 0"
        class="bg-warning text-black q-ma-md rounded-borders"
      >
        <q-btn
          flat
          no-caps
          color="black"
          class="q-px-none"
          icon="login"
          :label="missingDirectorLoginMessage"
          :aria-label="`${t('Login')}: ${firstMissingDirector}`"
          @click="openDirectorLogin(firstMissingDirector)"
        >
          <q-tooltip>{{ `${t('Login')}: ${firstMissingDirector}` }}</q-tooltip>
        </q-btn>
      </q-banner>
      <router-view />
    </q-page-container>

    <!-- ── Status bar ─────────────────────────────────────────────────────── -->
    <q-footer class="bareos-statusbar row items-center no-wrap q-px-md q-gutter-x-sm">
      <!-- proxy connection -->
      <q-icon :name="dirStatusIcon" :color="dirStatusColor" size="13px" />
      <span data-testid="director-status-label">
        {{ dirStatusLabel }}
        <span style="opacity:.7"> · {{ connectionEndpoint }}</span>
      </span>
      <span v-if="directorStatusError" class="text-negative q-ml-xs">· {{ directorStatusError }}</span>
      <span style="opacity:.4">|</span>
      <q-icon name="dns" size="13px" style="opacity:.7" />
      <span>{{ scopeLabel }}</span>

      <q-space />

      <!-- session info -->
      <template v-if="accountDirectorSessions.length > 0">
        <q-icon name="person" size="13px" style="opacity:.7" />
        <span>{{ accountMenuLabel }}</span>
        <span style="opacity:.4">|</span>
        <span style="opacity:.7">WebUI {{ appVersion }}</span>
      </template>
    </q-footer>
  </q-layout>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import bareosLogo from '../assets/bareos-logo-small.png'
import { bareosVersion as appVersion } from '../generated/bareos-version.js'
import DirectorScopeMenuContent from '../components/DirectorScopeMenuContent.vue'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { useAuthStore } from '../stores/auth.js'
import { useConsoleSessionsStore } from '../stores/consoleSessions.js'
import { useDirectorStore } from '../stores/director.js'
import { formatDirectorSessionsSummary } from '../utils/directorSessions.js'
import { DIRECTOR_WS_URL } from '../utils/directorCommandSocket.js'
import { toUserVisibleDirectorError } from '../utils/directorErrors.js'
import {
  deleteProxySession,
} from '../utils/sessionApi.js'
import {
  RELEASE_INFO_PAGE_URL,
  useReleaseInfoStore,
} from '../stores/releaseInfo.js'
import { useSettingsStore } from '../stores/settings.js'

const $q       = useQuasar()
const auth     = useAuthStore()
const consoleSessions = useConsoleSessionsStore()
const director = useDirectorStore()
const releaseInfo = useReleaseInfoStore()
const router   = useRouter()
const drawerOpen = ref(false)
const directorVersion = ref('')
const { t } = useI18n()
const settings = useSettingsStore()
const connectionEndpoint = computed(() => {
  try {
    return new URL(DIRECTOR_WS_URL, window.location.href).host
  } catch {
    return window.location.host
  }
})
const directorStatusError = computed(() => (
  director.errorMsg
    ? toUserVisibleDirectorError(director.errorMsg, {
      authenticationMessage: t('Authentication failed'),
      connectionMessage: t('Could not connect to director.'),
    })
    : ''
))

const currentDirector = computed(() => auth.user?.director || settings.directorName || '')
const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope,
  scopeLabel,
  syncSelectedDirectors,
} = useDirectorScope({ t })
const headerScopeLabel = computed(() => (
  isCommonScope.value
    ? `${activeDirectors.value.length} ${t('directors')}`
    : (activeDirectors.value[0] ?? t('Directors'))
))

function connectionCaptionParts(connectionState) {
  if (!connectionState?.status) {
    return []
  }

  if (connectionState.status === 'connected') {
    return [
      t('Connected'),
      ...(connectionState.transport ? [connectionState.transport] : []),
    ]
  }

  if (connectionState.status === 'checking' || connectionState.status === 'connecting') {
    return [t('Connecting…')]
  }

  if (connectionState.status === 'authenticating') {
    return [t('Authenticating…')]
  }

  if (connectionState.status === 'login_required') {
    return [t('Login required')]
  }

  if (connectionState.status === 'error') {
    return [t('Error')]
  }

  if (connectionState.status === 'disconnected') {
    return [t('Offline')]
  }

  return []
}

const directorMenuOptions = computed(() => (
  directorOptions.value.map((option) => {
    const value = String(option?.value ?? '').trim()
    if (!value) {
      return option
    }

    if (!activeDirectors.value.includes(value)) {
      return option
    }

    const connectionState = value === currentDirector.value
      ? {
        status: director.status,
        transport: director.transport,
        errorMsg: director.errorMsg,
      }
      : director.directorConnections[value]
    const captionParts = connectionCaptionParts(connectionState)
    if (captionParts.length === 0) {
      return option
    }

    return {
      ...option,
      caption: captionParts.join(' · '),
    }
  })
))
const scopeConsoleDirectors = computed(() => {
  const validDirectors = directorOptions.value.map(option => option.value)
  const scopedDirectors = activeDirectors.value.filter(value => validDirectors.includes(value))

  if (scopedDirectors.length > 0) {
    return scopedDirectors
  }

  return currentDirector.value ? [currentDirector.value] : []
})
const missingDirectorLogins = computed(() => (
  auth.missingDirectorSessions(activeDirectors.value)
))
const firstMissingDirector = computed(() => missingDirectorLogins.value[0] ?? '')
const missingDirectorLoginMessage = computed(() => (
  missingDirectorLogins.value.length === 1
    ? t('Log in to {director}.', {
      director: missingDirectorLogins.value[0],
    })
    : t('Log in to the selected directors.')
))
const accountDirectorSessions = computed(() => auth.directorSessions)
const accountMenuLabel = computed(() => (
  formatDirectorSessionsSummary(accountDirectorSessions.value, t)
))

function openConsole(targetDirector = scopeConsoleDirectors.value[0] || currentDirector.value) {
  const base = window.location.href.replace(/#.*$/, '')
  const directorName = targetDirector || auth.user?.director || settings.directorName
  const popupName = `bareos-console-${String(directorName).replace(/[^A-Za-z0-9_-]+/g, '-')}`
  window.open(
    `${base}#/console-popup?director=${encodeURIComponent(directorName)}`,
    popupName,
    'width=960,height=720,resizable=yes,scrollbars=no'
  )
}

function openDirectorLogin(targetDirector) {
  if (!targetDirector) {
    return
  }

  router.push({
    name: 'login',
    query: {
      mode: 'add',
      director: targetDirector,
      returnTo: router.currentRoute.value.fullPath,
    },
  })
}

function pruneSelectedDirectors(removedDirector) {
  const remainingSelected = settings.selectedDirectors
    .filter(value => value !== removedDirector)

  if (remainingSelected.length > 0) {
    settings.setSelectedDirectors(remainingSelected)
    return
  }

  if (auth.user?.director) {
    settings.setSelectedDirectors([auth.user.director])
    return
  }

  settings.setSelectedDirectors([])
}

const mainNavItems = computed(() => [
  { label: t('Dashboard'), to: '/dashboard', icon: 'dashboard', testId: 'nav-dashboard', drawerTestId: 'drawer-nav-dashboard' },
  { label: t('Jobs'), to: '/jobs', icon: 'work', testId: 'nav-jobs', drawerTestId: 'drawer-nav-jobs' },
  { label: t('Restore'), to: '/restore', icon: 'restore', testId: 'nav-restore', drawerTestId: 'drawer-nav-restore' },
  { label: t('Clients'), to: '/clients', icon: 'devices', testId: 'nav-clients', drawerTestId: 'drawer-nav-clients' },
  { label: t('Schedules'), to: '/schedules', icon: 'schedule', testId: 'nav-schedules', drawerTestId: 'drawer-nav-schedules' },
  { label: t('Storages'), to: '/storages', icon: 'storage', testId: 'nav-storages', drawerTestId: 'drawer-nav-storages' },
  { label: t('Director'), to: '/director', icon: 'settings', testId: 'nav-director', drawerTestId: 'drawer-nav-director' },
  { label: t('Analytics'), to: '/analytics', icon: 'bar_chart', testId: 'nav-analytics', drawerTestId: 'drawer-nav-analytics' },
])

onMounted(() => {
  $q.dark.set(settings.darkMode)
  releaseInfo.refresh().catch(() => {})
  director.fetchAvailableDirectors()
    .then(() => {
      syncSelectedDirectors()
    })
    .catch(() => {})
})

function toggleDark() {
  $q.dark.toggle()
  settings.darkMode = $q.dark.isActive
}

const dirStatusColor = computed(() => {
  if (director.status === 'connected') {
    return 'positive'
  }
  return ({
    connecting:     'warning',
    authenticating: 'warning',
    error:          'negative',
    disconnected:   'grey',
  }[director.status] ?? 'grey')
})

const dirStatusIcon = computed(() => ({
  connected:      'check_circle',
  connecting:     'sync',
  authenticating: 'sync',
  error:          'error',
  disconnected:   'cloud_off',
}[director.status] ?? 'cloud_off'))

const dirStatusLabel = computed(() => ({
  connected:      t('Connected'),
  connecting:     t('Connecting…'),
  authenticating: t('Authenticating…'),
  error:          t('Error'),
  disconnected:   t('Offline'),
}[director.status] ?? t('Offline')))

async function refreshDirectorVersion() {
  if (!director.isConnected) {
    directorVersion.value = ''
    return
  }
  try {
    const status = await director.call('status director')
    directorVersion.value = status?.header?.version ?? ''
  } catch {
    directorVersion.value = ''
  }
}

watch(
  () => [...activeDirectors.value],
  (directors) => {
    director.refreshDirectorConnections(directors).catch(() => {})
  },
  { immediate: true }
)

watch(
  () => director.status,
  (status) => {
    if (status === 'connected') {
      releaseInfo.refresh().catch(() => {})
      refreshDirectorVersion()
    } else if (status !== 'connecting' && status !== 'authenticating') {
      directorVersion.value = ''
    }
  },
  { immediate: true }
)

const directorUpdateInfo = computed(() => releaseInfo.getVersionInfo(directorVersion.value))

const directorUpdateAlert = computed(() => {
  if (!auth.user || !directorVersion.value) return null
  if (directorUpdateInfo.value.status === 'uptodate') return null

  const color = ({
    upgrade_required: 'negative',
    update_required: 'warning',
    unknown: 'warning',
  })[directorUpdateInfo.value.status] ?? 'warning'

  const icon = directorUpdateInfo.value.status === 'upgrade_required'
    ? 'error'
    : 'warning'

  const message = directorUpdateInfo.value.status === 'unknown'
    ? directorUpdateInfo.value.package_update_info
    : `Bareos Director (${directorUpdateInfo.value.requested_version}): ${directorUpdateInfo.value.package_update_info}`

  return { color, icon, message }
})

async function logout() {
  try {
    await deleteProxySession()
  } catch {
    // Clear the local state even if the backend session is already gone.
  }
  consoleSessions.disconnectAll({ reason: 'Disconnected', resetInitialized: true })
  director.disconnect()
  auth.logout()
  router.push({ name: 'login' })
}
</script>

<style scoped>
.director-scope-control {
  max-width: 15rem;
}

.director-scope-control__label {
  display: inline-block;
  max-width: 11rem;
}
</style>
