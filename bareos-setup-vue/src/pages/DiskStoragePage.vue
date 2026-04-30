<template>
  <div>
    <div class="step-header">Disk Storage</div>
    <p class="text-body2 q-mb-md">
      Configure the disk path used for Bareos volumes. When disk storage is not
      selected, this step is skipped.
    </p>

    <q-banner v-if="!store.storageTargets.disk" class="bg-info text-white q-mb-md" rounded>
      <template #avatar><q-icon name="info" /></template>
      Disk storage was not selected.
    </q-banner>

    <template v-else>
      <q-card flat bordered class="q-mb-md">
        <q-card-section class="q-gutter-md">
          <q-input v-model="store.diskConfig.path" outlined dense
                   label="Disk storage path" />
          <q-toggle v-model="store.diskConfig.dedupable"
                    label="Use dedupable storage" />
          <q-input v-model.number="store.diskConfig.concurrentJobs"
                   type="number" min="1" outlined dense
                   label="Parallel backup operations" />
        </q-card-section>
      </q-card>

      <q-card flat bordered class="q-mb-md">
        <q-card-section class="row items-center justify-between">
          <div>
            <div class="text-subtitle2">Attached filesystems and disks</div>
            <div class="text-caption text-grey">
              Helps choose the path for disk volumes.
            </div>
          </div>
          <q-btn flat icon="refresh" label="Refresh" @click="refreshInventory" />
        </q-card-section>
        <q-separator />
        <q-card-section>
          <q-markup-table flat dense>
            <thead>
              <tr>
                <th class="text-left">Path</th>
                <th class="text-left">Type</th>
                <th class="text-left">Size</th>
                <th class="text-left">Filesystem</th>
                <th class="text-left">Mountpoints</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="disk in store.diskInventory" :key="disk.path">
                <td>{{ disk.path }}</td>
                <td>{{ disk.type || '-' }}</td>
                <td>{{ disk.size || '-' }}</td>
                <td>{{ disk.filesystem || '-' }}</td>
                <td>{{ disk.mountpoints || '-' }}</td>
              </tr>
            </tbody>
          </q-markup-table>
        </q-card-section>
      </q-card>
    </template>

    <div v-if="lines.length" class="output-console q-mb-md" ref="consoleEl">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      Storage configuration failed (exit code {{ exitCode }}).
    </q-banner>
    <q-banner v-if="done && exitCode === 0" class="bg-positive text-white q-mb-md" rounded>
      <template #avatar><q-icon name="check_circle" /></template>
      Storage configuration applied successfully.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="configureHere && !store.storageConfigured" label="Configure Storage"
               color="secondary" icon="save" :loading="running"
               :disable="!canConfigure" @click="configureStorage" />
        <q-btn v-if="configureHere && store.storageConfigured"
               label="Continue" color="primary" icon-right="arrow_forward"
               @click="next" />
        <q-btn v-if="!configureHere" label="Continue" color="primary"
               icon-right="arrow_forward" @click="next" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, nextTick, onMounted, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router = useRouter()
const store = useSetupStore()
const { send, messages, clearMessages } = useSetupWs()

const running = ref(false)
const done = ref(false)
const exitCode = ref(0)
const lines = ref([])
const consoleEl = ref(null)

const configureHere = computed(() =>
  store.storageTargets.disk && !store.storageTargets.tape
)

const canConfigure = computed(() =>
  !store.storageTargets.disk
  || (!!store.diskConfig.path && Number(store.diskConfig.concurrentJobs) >= 1)
)

watch(
  () => [store.diskConfig.path, store.diskConfig.dedupable, store.diskConfig.concurrentJobs],
  () => {
    store.resetStorageConfigured()
  }
)

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last) return

  if (last.type === 'storage_inventory') {
    store.diskInventory = last.disks ?? []
  } else if (!running.value) {
    return
  } else if (last.type === 'output') {
    lines.value.push({
      text: last.line,
      cls: last.stream === 'stderr' ? 'output-line-err' : 'output-line-out',
    })
    nextTick(() => {
      if (consoleEl.value) consoleEl.value.scrollTop = consoleEl.value.scrollHeight
    })
  } else if (last.type === 'error') {
    running.value = false
    done.value = true
    exitCode.value = 1
    lines.value.push({
      text: last.message,
      cls: 'output-line-err',
    })
  } else if (last.type === 'done') {
    running.value = false
    done.value = true
    exitCode.value = last.exit_code
    if (last.exit_code === 0) store.storageConfigured = true
  }
}, { deep: true })

onMounted(() => {
  refreshInventory()
})

function buildPayload() {
  return {
    disk_enabled: store.storageTargets.disk,
    disk_path: store.diskConfig.path,
    disk_dedupable: store.diskConfig.dedupable,
    disk_concurrent_jobs: Number(store.diskConfig.concurrentJobs),
    tape_enabled: store.storageTargets.tape,
    tape_assignments: store.tapeAssignments,
  }
}

function refreshInventory() {
  send({ action: 'scan_storage' })
}

function configureStorage() {
  clearMessages()
  lines.value = []
  done.value = false
  running.value = true
  store.storageConfigured = false
  send({
    action: 'run_step',
    id: 'configure_storage',
    sudo: true,
    ...buildPayload(),
  })
}

function back() {
  store.prevStep()
  router.push('/targets')
}

function next() {
  store.nextStep()
  router.push('/tape')
}
</script>
