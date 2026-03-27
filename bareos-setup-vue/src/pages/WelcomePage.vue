<template>
  <div>
    <div class="text-h4 text-primary q-mb-md">
      <q-icon name="storage" class="q-mr-sm" />Welcome to Bareos Setup
    </div>
    <p class="text-body1 q-mb-lg">
      This wizard will guide you through the installation and initial
      configuration of Bareos — an open-source network backup solution.
    </p>

    <q-card flat bordered class="q-mb-lg">
      <q-card-section>
        <div class="text-subtitle1 text-weight-medium q-mb-sm">What this wizard will do:</div>
        <q-list dense>
          <q-item v-for="item in steps" :key="item.label">
            <q-item-section avatar>
              <q-icon :name="item.icon" color="primary" />
            </q-item-section>
            <q-item-section>{{ item.label }}</q-item-section>
          </q-item>
        </q-list>
      </q-card-section>
    </q-card>

    <q-banner v-if="!connected" class="bg-warning text-white q-mb-md" rounded>
      <template #avatar><q-icon name="warning" /></template>
      Cannot reach the setup backend. Make sure <code>bareos-setup</code> is running.
    </q-banner>

    <div class="row justify-end">
      <q-btn label="Get Started" color="primary" icon-right="arrow_forward"
             :disable="!connected" @click="next" />
    </div>
  </div>
</template>

<script setup>
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store  = useSetupStore()
const { connected } = useSetupWs()

const steps = [
  { icon: 'search',       label: 'Detect your operating system' },
  { icon: 'tune',        label: 'Select Bareos components to install' },
  { icon: 'source',      label: 'Configure the package repository' },
  { icon: 'download',    label: 'Install selected packages' },
  { icon: 'storage',     label: 'Set up the catalog database (PostgreSQL)' },
  { icon: 'lock',        label: 'Configure passwords' },
  { icon: 'check_circle',label: 'Review and finish' },
]

function next() {
  store.nextStep()
  router.push('/os')
}
</script>
