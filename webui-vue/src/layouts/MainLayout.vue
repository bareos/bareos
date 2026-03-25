<template>
  <q-layout view="hHh lpR fFf">
    <!-- Top Navbar -->
    <q-header class="bareos-toolbar">
      <q-toolbar>
        <!-- Logo -->
        <router-link to="/dashboard">
          <img src="/bareos-logo-small.png" alt="Bareos"
               style="height:36px; margin-right:8px;"
               @error="e => e.target.style.display='none'" />
        </router-link>
        <span class="text-white text-weight-bold text-h6" style="letter-spacing:0.02em">
          BAREOS
        </span>

        <!-- Main Nav -->
        <q-tabs v-model="activeTab" dense align="left" class="q-ml-md" indicator-color="white"
                active-color="white" active-bg-color="rgba(255,255,255,0.15)">
          <q-route-tab name="dashboard" to="/dashboard" label="Dashboard" no-caps />
          <q-route-tab name="jobs"      to="/jobs"      label="Jobs"      no-caps />
          <q-route-tab name="restore"   to="/restore"   label="Restore"   no-caps />
          <q-route-tab name="clients"   to="/clients"   label="Clients"   no-caps />
          <q-route-tab name="schedules" to="/schedules" label="Schedules" no-caps />
          <q-route-tab name="storages"  to="/storages"  label="Storages"  no-caps />
          <q-route-tab name="director"  to="/director"  label="Director"  no-caps />
          <q-route-tab name="analytics" to="/analytics" label="Analytics" no-caps />
        </q-tabs>

        <q-space />

        <!-- Director connection status chip -->
        <q-chip
          dense square
          :color="dirStatusColor"
          text-color="white"
          :icon="dirStatusIcon"
          :label="dirStatusLabel"
          class="q-mr-sm"
          style="font-size:0.75rem"
        />

        <!-- Right side: director + user dropdowns -->
        <q-btn flat color="white" :label="auth.user?.director || 'director'" icon="dns" no-caps>
          <q-menu>
            <q-list dense style="min-width:160px">
              <q-item clickable v-close-popup :to="{ name: 'console' }">
                <q-item-section avatar><q-icon name="terminal" /></q-item-section>
                <q-item-section>Console</q-item-section>
              </q-item>
            </q-list>
          </q-menu>
        </q-btn>

        <q-btn flat color="white" :label="auth.user?.username || 'admin'" icon="person" no-caps>
          <q-menu>
            <q-list dense style="min-width:180px">
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
      </q-toolbar>
    </q-header>

    <q-page-container>
      <router-view />
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'

const auth     = useAuthStore()
const director = useDirectorStore()
const router   = useRouter()
const route    = useRoute()
const activeTab = ref(null)

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
