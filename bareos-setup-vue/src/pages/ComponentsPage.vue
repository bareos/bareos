<template>
  <div>
    <div class="step-header">Storage Targets</div>
    <p class="text-body2 q-mb-md">
      Select where Bareos should store backup data. Disk and tape can be used
      independently, but at least one target is required.
    </p>

    <q-card flat bordered class="q-mb-lg">
      <q-card-section>
        <q-list>
          <q-item tag="label">
            <q-item-section avatar>
              <q-checkbox v-model="store.storageTargets.disk" />
            </q-item-section>
            <q-item-section>
              <q-item-label>Store to disk</q-item-label>
              <q-item-label caption>Configure a filesystem path for disk volumes.</q-item-label>
            </q-item-section>
          </q-item>

          <q-item tag="label">
            <q-item-section avatar>
              <q-checkbox v-model="store.storageTargets.tape" />
            </q-item-section>
            <q-item-section>
              <q-item-label>Store to tape changer</q-item-label>
              <q-item-label caption>Scan attached changers and assign tape drives.</q-item-label>
            </q-item-section>
          </q-item>
        </q-list>
      </q-card-section>
    </q-card>

    <q-card v-if="store.tapeChangers.length" flat bordered class="q-mb-lg">
      <q-card-section>
        <div class="text-subtitle2 q-mb-sm">Detected tape changers</div>
        <q-list bordered separator>
          <q-item v-for="changer in store.tapeChangers" :key="changer.path" class="q-py-md">
            <q-item-section>
              <q-item-label class="text-subtitle2">
                {{ changerLabel(changer) }}
              </q-item-label>
              <q-item-label caption class="q-mt-xs">
                {{ changer.vendor || '-' }} / {{ changer.model || '-' }} / {{ changer.firmware_version || '-' }}
              </q-item-label>
              <q-item-label class="q-mt-sm">
                <strong>Path:</strong> {{ changer.path }}
              </q-item-label>
              <q-item-label class="q-mt-xs">
                <strong>Slots:</strong> {{ changer.status?.slots ?? '-' }},
                <strong>Drives:</strong> {{ changer.status?.drives ?? '-' }},
                <strong>I/E Slots:</strong> {{ changer.status?.ie_slots ?? '-' }}
              </q-item-label>
              <q-item-label class="q-mt-xs">
                <strong>Tape Identifiers:</strong>
              </q-item-label>
              <div v-if="changer.drive_identifiers?.length" class="text-caption q-mt-xs">
                <div v-for="drive in changer.drive_identifiers" :key="`${changer.path}-${drive.element_address}`">
                  Element {{ drive.element_address }}:
                  {{ (drive.identifiers || []).join(' | ') || '-' }}
                </div>
              </div>
              <q-item-label v-else caption class="q-mt-xs">-</q-item-label>
            </q-item-section>
          </q-item>
        </q-list>
      </q-card-section>
    </q-card>

    <q-banner v-if="!anySelected" class="bg-warning text-white q-mb-md" rounded>
      <template #avatar><q-icon name="warning" /></template>
      Select at least one storage target.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <q-btn label="Continue" color="primary" icon-right="arrow_forward"
             :disable="!anySelected" @click="next" />
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store = useSetupStore()
const { send, messages } = useSetupWs()

const anySelected = computed(() =>
  store.storageTargets.disk || store.storageTargets.tape
)

watch(
  () => [store.storageTargets.disk, store.storageTargets.tape],
  () => {
    store.resetStorageConfigured()
  }
)

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last || last.type !== 'storage_inventory') return

  store.diskInventory = last.disks ?? []
  store.tapeChangers = last.tape_changers ?? []
  store.tapeDrives = last.tape_drives ?? []

  if (store.tapeChangers.length > 0 && !store.storageTargets.tape
      && !store.tapeTargetAutoSelected) {
    store.storageTargets.tape = true
    store.tapeTargetAutoSelected = true
  }
}, { deep: true })

onMounted(() => {
  send({ action: 'scan_storage' })
})

function changerLabel(changer) {
  return `${changer.identifier || changer.display_name}${changer.serial_number ? ` / ${changer.serial_number}` : ''}`
}

function back() {
  store.prevStep()
  router.push('/install')
}

function next() {
  store.nextStep()
  router.push('/disk')
}
</script>
