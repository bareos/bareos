<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status"       label="Status"       no-caps />
      <q-tab name="messages"     label="Messages"     no-caps />
      <q-tab name="subscription" label="Subscription" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- STATUS -->
      <q-tab-panel name="status" class="q-pa-none">
        <div class="row q-col-gutter-md">
          <div class="col-12 col-md-8">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header row items-center">
                <span>Director Status</span>
                <q-space />
                <q-btn flat round dense icon="refresh" color="white" @click="refreshStatus" :loading="statusLoading" />
              </q-card-section>
              <q-card-section>
                <q-inner-loading :showing="statusLoading" />
                <div v-if="statusError" class="text-negative">{{ statusError }}</div>
                <div v-else class="console-output">{{ dirStatusText }}</div>
              </q-card-section>
            </q-card>
          </div>
          <div class="col-12 col-md-4">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header">Version</q-card-section>
              <q-card-section>
                <div class="text-h6">Bareos Director</div>
                <div v-if="versionLine" class="text-caption text-grey">{{ versionLine }}</div>
              </q-card-section>
            </q-card>
          </div>
        </div>
      </q-tab-panel>

      <!-- MESSAGES -->
      <q-tab-panel name="messages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Director Messages</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshMessages" :loading="messagesLoading" />
          </q-card-section>
          <q-card-section>
            <q-inner-loading :showing="messagesLoading" />
            <div v-if="messagesError" class="text-negative">{{ messagesError }}</div>
            <div v-else class="console-output">{{ messagesText || '(no messages)' }}</div>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- SUBSCRIPTION -->
      <q-tab-panel name="subscription">
        <q-card flat bordered class="bareos-panel" style="max-width:600px">
          <q-card-section class="panel-header">Subscription</q-card-section>
          <q-card-section>
            <q-list dense separator>
              <q-item><q-item-section class="text-weight-medium" style="max-width:180px">Subscription Status</q-item-section><q-item-section><q-badge color="info" label="Community" /></q-item-section></q-item>
            </q-list>
            <q-btn class="q-mt-md" color="primary" label="Get Official Support" icon="open_in_new"
                   href="https://www.bareos.com/subscription/" target="_blank" no-caps />
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed } from 'vue'
import { useDirectorStore } from '../stores/director.js'

const tab = ref('status')
const director = useDirectorStore()

// ── Status ───────────────────────────────────────────────────────────────────
const statusLoading = ref(false)
const statusError   = ref(null)
const rawStatus     = ref(null)

async function refreshStatus() {
  statusLoading.value = true
  statusError.value = null
  try {
    rawStatus.value = await director.call('status director')
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

// The director returns a dict; format it for display.
const dirStatusText = computed(() => {
  const d = rawStatus.value
  if (!d) return ''
  if (typeof d === 'string') return d
  return JSON.stringify(d, null, 2)
})

const versionLine = computed(() => {
  const text = dirStatusText.value
  const m = text.match(/Version:\s+(.+)/)
  return m ? m[1] : ''
})

// ── Messages ─────────────────────────────────────────────────────────────────
const messagesLoading = ref(false)
const messagesError   = ref(null)
const rawMessages     = ref(null)

async function refreshMessages() {
  messagesLoading.value = true
  messagesError.value = null
  try {
    rawMessages.value = await director.call('messages')
  } catch (e) {
    messagesError.value = e.message
  } finally {
    messagesLoading.value = false
  }
}

const messagesText = computed(() => {
  const d = rawMessages.value
  if (!d) return ''
  if (typeof d === 'string') return d
  return JSON.stringify(d, null, 2)
})

refreshStatus()
refreshMessages()
</script>
