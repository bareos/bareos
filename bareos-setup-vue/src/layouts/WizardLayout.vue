<template>
  <q-layout view="lHh Lpr lFf">
    <q-header elevated class="bg-primary">
      <q-toolbar>
        <q-icon name="storage" size="28px" class="q-mr-sm" />
        <q-toolbar-title>Bareos Setup Wizard</q-toolbar-title>
      </q-toolbar>
    </q-header>

    <q-page-container>
      <q-page class="setup-page q-pa-lg">
        <!-- Step progress bar -->
        <q-linear-progress
          :value="stepProgress"
          color="primary"
          track-color="grey-3"
          class="q-mb-lg"
          style="height: 8px; border-radius: 4px"
        />
        <div class="text-caption text-right text-grey-6 q-mb-md">
          Step {{ store.stepIndex + 1 }} of {{ store.steps.length }}
          &mdash; {{ stepLabel }}
        </div>

        <!-- Page content -->
        <router-view />
      </q-page>
    </q-page-container>
  </q-layout>
</template>

<script setup>
import { computed } from 'vue'
import { useSetupStore } from '../stores/setup.js'

const store = useSetupStore()

const stepLabels = [
  'Welcome', 'OS Detection', 'Repository', 'Installation',
  'Storage Targets', 'Disk Storage', 'Tape Storage', 'Database',
  'Passwords', 'Summary'
]

const stepLabel   = computed(() => stepLabels[store.stepIndex] ?? '')
const stepProgress = computed(() => (store.stepIndex + 1) / store.steps.length)
</script>
