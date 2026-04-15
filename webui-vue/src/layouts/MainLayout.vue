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
            <q-tooltip>Bareos WebUI v{{ appVersion }}</q-tooltip>
          </span>
          <span class="text-white text-weight-bold text-h6" style="letter-spacing:0.02em">
            BAREOS
          </span>
        </div>
        <q-separator dark />

        <!-- Navigation items -->
        <q-list dark>
          <q-item v-for="nav in navItems" :key="nav.to"
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
          <q-item clickable v-ripple @click="openConsole(); drawerOpen = false">
            <q-item-section avatar><q-icon name="terminal" /></q-item-section>
            <q-item-section>Console</q-item-section>
          </q-item>
        </q-list>

        <q-separator dark class="q-my-sm" />

        <!-- Settings / account -->
        <q-list dark>
          <q-item clickable v-ripple :to="{ name: 'settings' }" @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="tune" /></q-item-section>
            <q-item-section>Settings</q-item-section>
          </q-item>
          <q-item clickable v-ripple
                  tag="a" href="https://docs.bareos.org" target="_blank"
                  @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="menu_book" /></q-item-section>
            <q-item-section>Documentation</q-item-section>
          </q-item>
          <q-item clickable v-ripple
                  tag="a" href="https://github.com/bareos/bareos/issues/" target="_blank"
                  @click="drawerOpen = false">
            <q-item-section avatar><q-icon name="bug_report" /></q-item-section>
            <q-item-section>Issue Tracker</q-item-section>
          </q-item>
          <q-separator dark />
          <q-item clickable v-ripple @click="logout">
            <q-item-section avatar><q-icon name="logout" /></q-item-section>
            <q-item-section>Logout</q-item-section>
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
          <q-tooltip>Bareos WebUI v{{ appVersion }}</q-tooltip>
        </router-link>
        <span class="text-white text-weight-bold text-h6" style="letter-spacing:0.02em">
          BAREOS
        </span>

        <!-- Main nav tabs (desktop only) -->
        <q-tabs v-if="!$q.screen.lt.md" v-model="activeTab" dense align="left"
                class="q-ml-md" indicator-color="white"
                active-color="white" active-bg-color="rgba(255,255,255,0.15)">
          <q-route-tab v-for="nav in navItems" :key="nav.to"
                       :data-testid="nav.testId"
                       :name="nav.label.toLowerCase()" :to="nav.to"
                       :label="nav.label" no-caps />
        </q-tabs>

        <q-space />

        <!-- Desktop-only: director menu, user menu -->
        <template v-if="!$q.screen.lt.md">

          <q-btn flat color="white" :label="auth.user?.director || 'director'" icon="dns" no-caps>
            <q-menu>
              <q-list dense style="min-width:160px">
                <q-item clickable v-close-popup @click="openConsole">
                  <q-item-section avatar><q-icon name="terminal" /></q-item-section>
                  <q-item-section>Console</q-item-section>
                </q-item>
              </q-list>
            </q-menu>
          </q-btn>

          <q-btn flat color="white" :label="auth.user?.username || 'admin'" icon="person" no-caps>
            <q-menu>
              <q-list dense style="min-width:180px">
                <q-item clickable v-close-popup :to="{ name: 'settings' }">
                  <q-item-section avatar><q-icon name="tune" /></q-item-section>
                  <q-item-section>Settings</q-item-section>
                </q-item>
                <q-separator />
                <q-item clickable tag="a" href="https://docs.bareos.org" target="_blank" v-close-popup>
                  <q-item-section avatar><q-icon name="menu_book" /></q-item-section>
                  <q-item-section>Documentation</q-item-section>
                </q-item>
                <q-item clickable tag="a" href="https://github.com/bareos/bareos/issues/" target="_blank" v-close-popup>
                  <q-item-section avatar><q-icon name="bug_report" /></q-item-section>
                  <q-item-section>Issue Tracker</q-item-section>
                </q-item>
                <q-separator />
                <q-item clickable v-close-popup @click="logout">
                  <q-item-section avatar><q-icon name="logout" /></q-item-section>
                  <q-item-section>Logout</q-item-section>
                </q-item>
              </q-list>
            </q-menu>
          </q-btn>
        </template>

      </q-toolbar>
    </q-header>

    <q-page-container>
      <router-view />
    </q-page-container>

    <!-- ── Status bar ─────────────────────────────────────────────────────── -->
    <q-footer class="bareos-statusbar row items-center no-wrap q-px-md q-gutter-x-sm">
      <!-- connection indicator -->
      <q-icon :name="dirStatusIcon" :color="dirStatusColor" size="13px" />
      <span data-testid="director-status-label">{{ dirStatusLabel }}</span>
      <span v-if="director.errorMsg" class="text-negative q-ml-xs">— {{ director.errorMsg }}</span>

      <q-space />

      <!-- director / host info -->
      <template v-if="auth.user">
        <q-icon name="dns" size="13px" style="opacity:.7" />
        <span>{{ auth.user.director }}
          <span style="opacity:.6">({{ auth.user.host }}:{{ auth.user.port }})</span>
        </span>
        <span style="opacity:.4">|</span>
        <q-icon name="person" size="13px" style="opacity:.7" />
        <span>{{ auth.user.username }}</span>
        <span style="opacity:.4">|</span>
        <span style="opacity:.7">WebUI v{{ appVersion }}</span>
      </template>
    </q-footer>
  </q-layout>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useQuasar } from 'quasar'
import bareosLogo from '../assets/bareos-logo-small.png'
import { bareosVersion as appVersion } from '../generated/bareos-version.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'

const $q       = useQuasar()
const auth     = useAuthStore()
const director = useDirectorStore()
const router   = useRouter()
const route    = useRoute()
const activeTab  = ref(null)
const drawerOpen = ref(false)

function openConsole() {
  const base = window.location.href.replace(/#.*$/, '')
  window.open(
    base + '#/console-popup',
    'bareos-console',
    'width=960,height=720,resizable=yes,scrollbars=no'
  )
}

const navItems = [
  { label: 'Dashboard',    to: '/dashboard',    icon: 'dashboard',  testId: 'nav-dashboard',    drawerTestId: 'drawer-nav-dashboard'    },
  { label: 'Jobs',         to: '/jobs',         icon: 'work',       testId: 'nav-jobs',         drawerTestId: 'drawer-nav-jobs'         },
  { label: 'Restore',      to: '/restore',      icon: 'restore',    testId: 'nav-restore',      drawerTestId: 'drawer-nav-restore'      },
  { label: 'Clients',      to: '/clients',      icon: 'devices',    testId: 'nav-clients',      drawerTestId: 'drawer-nav-clients'      },
  { label: 'Schedules',    to: '/schedules',    icon: 'schedule',   testId: 'nav-schedules',    drawerTestId: 'drawer-nav-schedules'    },
  { label: 'Storages',     to: '/storages',     icon: 'storage',    testId: 'nav-storages',     drawerTestId: 'drawer-nav-storages'     },
  { label: 'Autochangers', to: '/autochangers', icon: 'swap_horiz', testId: 'nav-autochangers', drawerTestId: 'drawer-nav-autochangers' },
  { label: 'Director',     to: '/director',     icon: 'settings',   testId: 'nav-director',     drawerTestId: 'drawer-nav-director'     },
  { label: 'Analytics',    to: '/analytics',    icon: 'bar_chart',  testId: 'nav-analytics',    drawerTestId: 'drawer-nav-analytics'    },
]

const settings = useSettingsStore()

onMounted(() => {
  $q.dark.set(settings.darkMode)
})

function toggleDark() {
  $q.dark.toggle()
  settings.darkMode = $q.dark.isActive
}

const dirStatusColor = computed(() => ({
  connected:      'positive',
  connecting:     'warning',
  authenticating: 'warning',
  error:          'negative',
  disconnected:   'grey',
}[director.status] ?? 'grey'))

const dirStatusIcon = computed(() => ({
  connected:      'check_circle',
  connecting:     'sync',
  authenticating: 'sync',
  error:          'error',
  disconnected:   'cloud_off',
}[director.status] ?? 'cloud_off'))

const dirStatusLabel = computed(() => ({
  connected:      'Connected',
  connecting:     'Connecting…',
  authenticating: 'Authenticating…',
  error:          'Error',
  disconnected:   'Offline',
}[director.status] ?? 'Offline'))

function logout() {
  director.disconnect()
  auth.logout()
  router.push({ name: 'login' })
}
</script>
