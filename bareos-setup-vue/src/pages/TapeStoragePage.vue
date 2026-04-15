<template>
  <div>
    <div class="step-header">Tape Storage</div>
    <p class="text-body2 q-mb-md">
      Scan the attached autochangers and tape drives, then assign drives to
      changers in the order Bareos should use them.
    </p>

    <q-banner v-if="!store.storageTargets.tape" class="bg-info text-white q-mb-md" rounded>
      <template #avatar><q-icon name="info" /></template>
      Tape storage was not selected.
    </q-banner>

    <template v-else>
      <q-banner class="bg-warning text-white q-mb-md" rounded>
        <template #avatar><q-icon name="warning" /></template>
        Make sure the tape changer is attached to the system before scanning.
      </q-banner>
      <q-banner v-if="store.tapeAssignments.length"
                class="bg-info text-white q-mb-md" rounded>
        <template #avatar><q-icon name="info" /></template>
        Tape drives were assigned automatically for each detected library.
      </q-banner>

      <div class="row justify-end q-mb-md">
        <q-btn color="secondary" icon="search" label="Scan Devices" @click="scanStorage" />
      </div>

      <q-card flat bordered class="q-mb-md">
        <q-card-section>
          <div class="text-subtitle2">Library and drive assignment</div>
          <div class="text-caption text-grey q-mt-xs">
            Library details are shown on the left and the assigned tape drives on the right.
          </div>
        </q-card-section>
      </q-card>

      <q-card v-for="changer in store.tapeChangers" :key="changer.path"
               flat bordered class="q-mb-md">
        <q-card-section>
          <div class="row q-col-gutter-lg items-start">
            <div class="col-12 col-md-6">
              <div class="text-subtitle2 q-mb-sm">
                {{ changerLabel(changer) }}
              </div>
              <div class="q-gutter-y-xs">
                <div><strong>Path:</strong> {{ changer.path }}</div>
                <div><strong>Manufacturer:</strong> {{ changer.vendor || '-' }}</div>
                <div><strong>Type:</strong> {{ changer.model || '-' }}</div>
                <div><strong>Firmware:</strong> {{ changer.firmware_version || '-' }}</div>
                <div>
                  <strong>Slots:</strong> {{ changer.status?.slots ?? '-' }},
                  <strong>Drives:</strong> {{ changer.status?.drives ?? '-' }},
                  <strong>Robot Arms:</strong> {{ changer.status?.robot_arms ?? '-' }},
                  <strong>I/E Slots:</strong> {{ changer.status?.ie_slots ?? '-' }}
                </div>
                <div>
                  <strong>Tape Identifiers:</strong>
                  <div v-if="changer.drive_identifiers?.length" class="q-mt-xs">
                    <div v-for="drive in changer.drive_identifiers" :key="`${changer.path}-${drive.element_address}`">
                      Element {{ drive.element_address }}:
                      {{ (drive.identifiers || []).join(' | ') || '-' }}
                    </div>
                  </div>
                  <span v-else> - </span>
                </div>
              </div>
            </div>
            <div class="col-12 col-md-6">
              <div class="text-subtitle2 q-mb-sm">Assigned tape drives</div>
              <q-select
                :model-value="drivePathsFor(changer.path)"
                multiple
                use-chips
                outlined
                dense
                emit-value
                map-options
                :options="availableDriveOptions(changer.path)"
                @update:model-value="updateAssignment(changer.path, $event)"
              />
              <div class="text-caption text-grey q-mt-sm">
                The selected order becomes the drive order for this changer.
              </div>
              <q-list v-if="selectedDrivesFor(changer.path).length"
                      bordered separator class="q-mt-md">
                <q-item v-for="drive in selectedDrivesFor(changer.path)" :key="drive.path">
                  <q-item-section>
                    <q-item-label>{{ driveLabel(drive) }}</q-item-label>
                    <q-item-label caption>
                      {{ drive.vendor || '-' }} / {{ drive.model || '-' }} / {{ drive.firmware_version || '-' }}
                    </q-item-label>
                  </q-item-section>
                </q-item>
              </q-list>
              <div v-else class="text-caption text-grey q-mt-md">
                No tape drives are currently assigned to this library.
              </div>
            </div>
          </div>
        </q-card-section>
      </q-card>
    </template>

    <div v-if="lines.length" class="output-console q-mb-md" ref="consoleEl">
      <div v-for="(l, i) in lines" :key="i" :class="l.cls">{{ l.text }}</div>
    </div>

    <q-banner v-if="done && exitCode !== 0" class="bg-negative text-white q-mb-md" rounded>
      <template #avatar><q-icon name="error" /></template>
      {{ completedStep === 'test_tape_assignment'
        ? `Tape drive assignment test failed (exit code ${exitCode}).`
        : `Storage configuration failed (exit code ${exitCode}).` }}
    </q-banner>
    <q-banner v-if="done && exitCode === 0" class="bg-positive text-white q-mb-md" rounded>
      <template #avatar><q-icon name="check_circle" /></template>
      {{ completedStep === 'test_tape_assignment'
        ? 'Tape drive assignment verified successfully.'
        : 'Storage configuration applied successfully.' }}
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" :disable="running" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn v-if="store.storageTargets.tape"
               label="Test Assignment" color="secondary" icon="checklist"
               :loading="running" :disable="!canTestAssignments"
               @click="testAssignments" />
        <q-btn v-if="store.storageTargets.tape && !store.storageConfigured"
               label="Configure Storage" color="secondary" icon="save"
                :loading="running" :disable="!canConfigure" @click="configureStorage" />
        <q-btn v-else label="Continue" color="primary" icon-right="arrow_forward"
               :disable="store.storageTargets.tape && !store.storageConfigured"
               @click="next" />
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
const completedStep = ref('')
const lines = ref([])
const consoleEl = ref(null)

const canConfigure = computed(() => {
  if (!store.storageTargets.tape) return true
  return store.tapeAssignments.some((assignment) => assignment.drive_paths.length > 0)
})

const canTestAssignments = computed(() => {
  if (!store.storageTargets.tape) return false
  return store.tapeAssignments.some((assignment) => assignment.drive_paths.length > 0)
})

watch(messages, (msgs) => {
  const last = msgs[msgs.length - 1]
  if (!last) return

  if (last.type === 'storage_inventory') {
    store.diskInventory = last.disks ?? []
    store.tapeChangers = last.tape_changers ?? []
    store.tapeDrives = last.tape_drives ?? []
    applyAssignments(last.suggested_tape_assignments ?? [])
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
    completedStep.value = last.step ?? ''
    lines.value.push({
      text: last.message,
      cls: 'output-line-err',
    })
  } else if (last.type === 'done') {
    running.value = false
    done.value = true
    exitCode.value = last.exit_code
    completedStep.value = last.step ?? ''
    if (last.exit_code === 0 && last.step === 'configure_storage')
      store.storageConfigured = true
  }
}, { deep: true })

watch(
  () => store.tapeAssignments,
  () => {
    store.resetStorageConfigured()
  },
  { deep: true }
)

onMounted(() => {
  if (store.storageTargets.tape) scanStorage()
})

function buildPayload() {
  return {
    disk_enabled: store.storageTargets.disk,
    disk_path: store.diskConfig.path,
    disk_dedupable: store.diskConfig.dedupable,
    disk_concurrent_jobs: Number(store.diskConfig.concurrentJobs),
    tape_enabled: store.storageTargets.tape,
    tape_assignments: store.tapeAssignments
      .filter((assignment) => assignment.drive_paths.length > 0),
  }
}

function drivePathsFor(changerPath) {
  return store.tapeAssignments.find((assignment) => assignment.changer_path === changerPath)?.drive_paths ?? []
}

function changerLabel(changer) {
  return `${changer.identifier || changer.display_name}${changer.serial_number ? ` / ${changer.serial_number}` : ''}`
}

function driveLabel(drive) {
  return `${drive.identifier || drive.display_name}${drive.serial_number ? ` / ${drive.serial_number}` : ''} (${drive.path})`
}

function selectedDrivesFor(changerPath) {
  const assignedPaths = drivePathsFor(changerPath)
  const driveMap = new Map(store.tapeDrives.map((drive) => [drive.path, drive]))
  return assignedPaths
    .map((drivePath) => driveMap.get(drivePath))
    .filter((drive) => Boolean(drive))
}

function updateAssignment(changerPath, drivePaths) {
  const nextAssignments = store.tapeAssignments.filter(
    (assignment) => assignment.changer_path !== changerPath
  )
  nextAssignments.push({
    changer_path: changerPath,
    drive_paths: drivePaths,
  })
  store.tapeAssignments = nextAssignments
}

function applyAssignments(suggestedAssignments) {
  const validChangerPaths = new Set(store.tapeChangers.map((changer) => changer.path))
  const validDrivePaths = new Set(store.tapeDrives.map((drive) => drive.path))
  const existingAssignments = new Map(
    store.tapeAssignments
      .filter((assignment) => validChangerPaths.has(assignment.changer_path))
      .map((assignment) => [
        assignment.changer_path,
        assignment.drive_paths.filter((drivePath) => validDrivePaths.has(drivePath)),
      ])
  )

  if (Array.isArray(suggestedAssignments) && suggestedAssignments.length > 0) {
    const mergedAssignments = suggestedAssignments
      .filter((assignment) => validChangerPaths.has(assignment.changer_path))
      .map((assignment) => {
        const existingDrivePaths = existingAssignments.get(assignment.changer_path)
        return {
          changer_path: assignment.changer_path,
          drive_paths: existingDrivePaths ?? assignment.drive_paths.filter(
            (drivePath) => validDrivePaths.has(drivePath)
          ),
        }
      })

    store.tapeAssignments = mergedAssignments.filter(
      (assignment) => assignment.drive_paths.length > 0
    )
    return
  }

  store.tapeAssignments = store.tapeAssignments
    .filter((assignment) => validChangerPaths.has(assignment.changer_path))
    .map((assignment) => ({
      changer_path: assignment.changer_path,
      drive_paths: assignment.drive_paths.filter((drivePath) => validDrivePaths.has(drivePath)),
    }))
    .filter((assignment) => assignment.drive_paths.length > 0)
}

function availableDriveOptions(changerPath) {
  return store.tapeDrives
    .map((drive) => ({
      label: driveLabel(drive),
      value: drive.path,
    }))
}

function scanStorage() {
  send({ action: 'scan_storage' })
}

function configureStorage() {
  clearMessages()
  lines.value = []
  done.value = false
  running.value = true
  completedStep.value = ''
  store.storageConfigured = false
  send({
    action: 'run_step',
    id: 'configure_storage',
    sudo: true,
    ...buildPayload(),
  })
}

function testAssignments() {
  clearMessages()
  lines.value = []
  done.value = false
  running.value = true
  completedStep.value = ''
  send({
    action: 'test_tape_assignment',
    sudo: true,
    ...buildPayload(),
  })
}

function back() {
  store.prevStep()
  router.push('/disk')
}

function next() {
  store.nextStep()
  router.push('/database')
}
</script>
