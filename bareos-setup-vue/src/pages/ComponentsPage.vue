<template>
  <div>
    <div class="step-header">Select Components</div>
    <p class="text-body2 q-mb-md">
      Choose which Bareos components to install on this machine.
    </p>

    <q-card flat bordered class="q-mb-lg">
      <q-card-section>
        <q-list>
          <q-item v-for="c in componentList" :key="c.key" tag="label">
            <q-item-section avatar>
              <q-checkbox v-model="store.components[c.key]" />
            </q-item-section>
            <q-item-section>
              <q-item-label>{{ c.label }}</q-item-label>
              <q-item-label caption>{{ c.desc }}</q-item-label>
            </q-item-section>
          </q-item>
        </q-list>
      </q-card-section>
    </q-card>

    <q-banner v-if="!anySelected" class="bg-warning text-white q-mb-md" rounded>
      <template #avatar><q-icon name="warning" /></template>
      Select at least one component.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <q-btn label="Continue" color="primary" icon-right="arrow_forward"
             :disable="!anySelected" @click="next" />
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'

const router = useRouter()
const store  = useSetupStore()

const componentList = [
  { key: 'director',   label: 'Bareos Director',       desc: 'Central daemon that controls backup and restore operations' },
  { key: 'storage',    label: 'Bareos Storage Daemon',  desc: 'Manages physical media and storage volumes' },
  { key: 'filedaemon', label: 'Bareos File Daemon',     desc: 'Client agent running on machines to be backed up' },
  { key: 'webui',      label: 'Bareos WebUI',           desc: 'Web-based management interface' },
]

const anySelected = computed(() => Object.values(store.components).some(Boolean))

function back() { store.prevStep(); router.push('/os') }
function next() { store.nextStep(); router.push('/repo') }
</script>
