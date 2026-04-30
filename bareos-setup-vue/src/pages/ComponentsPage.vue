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
                <div class="row items-center no-wrap q-col-gutter-sm">
                  <q-icon name="precision_manufacturing" color="primary" />
                  <span>{{ changerLabel(changer) }}</span>
                </div>
              </q-item-label>
              <q-item-label caption class="q-mt-xs q-ml-lg">
                {{ changerDetails(changer) }}
              </q-item-label>
              <q-item-label caption class="q-mt-xs q-ml-lg">
                {{ changerSummary(changer) }}
              </q-item-label>
              <div class="q-ml-lg q-mt-sm">
                <q-item-label>
                  <strong>Path:</strong> {{ changer.path }}
                </q-item-label>
                <div v-if="changerDriveEntries(changer).length" class="text-caption q-mt-sm">
                  <div v-for="drive in changerDriveEntries(changer)"
                       :key="drive.key"
                       class="row items-start no-wrap q-col-gutter-sm q-mt-xs">
                    <q-icon name="dns" color="primary" size="18px" class="q-mt-xs" />
                    <div>{{ drive.label }}</div>
                  </div>
                </div>
                <q-item-label v-else caption class="q-mt-sm">-</q-item-label>
              </div>
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
import {
  formatChangerDetails,
  formatChangerLabel,
  formatChangerSummary,
  resolveChangerDrives,
} from '../utils/tapeLibraries.js'

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
  return formatChangerLabel(changer)
}

function changerDetails(changer) {
  return formatChangerDetails(changer)
}

function changerSummary(changer) {
  return formatChangerSummary(changer)
}

function changerDriveEntries(changer) {
  return resolveChangerDrives(changer, store.tapeDrives)
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
