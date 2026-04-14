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
      <q-banner v-if="store.tapeChangers.length === 1 && store.tapeAssignments.length"
                class="bg-info text-white q-mb-md" rounded>
        <template #avatar><q-icon name="info" /></template>
        The detected tape drives were assigned automatically because only one
        tape changer was found.
      </q-banner>

      <div class="row justify-end q-mb-md">
        <q-btn color="secondary" icon="search" label="Scan Devices" @click="scanStorage" />
      </div>

      <q-card flat bordered class="q-mb-md">
        <q-card-section>
          <div class="text-subtitle2 q-mb-sm">Detected tape changers</div>
          <q-markup-table flat dense>
            <thead>
              <tr>
                <th class="text-left">Identification</th>
                <th class="text-left">Serial</th>
                <th class="text-left">Manufacturer</th>
                <th class="text-left">Type</th>
                <th class="text-left">Firmware</th>
                <th class="text-left">Selected Path</th>
                <th class="text-left">Additional Paths</th>
                <th class="text-left">Drives</th>
                <th class="text-left">Robot Arms</th>
                <th class="text-left">Slots</th>
                <th class="text-left">I/E Slots</th>
                <th class="text-left">Tape Identifiers</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="changer in store.tapeChangers" :key="changer.path">
                <td>{{ changer.identifier || changer.display_name }}</td>
                <td>{{ changer.serial_number || '-' }}</td>
                <td>{{ changer.vendor || '-' }}</td>
                <td>{{ changer.model || '-' }}</td>
                <td>{{ changer.firmware_version || '-' }}</td>
                <td>{{ changer.path }}</td>
                <td>
                  <div v-if="changer.canonical_path && changer.canonical_path !== changer.path">
                    {{ changer.canonical_path }}
                  </div>
                  <div v-for="alias in changer.aliases || []" :key="alias">
                    {{ alias }}
                  </div>
                  <span v-if="!changer.aliases?.length && (!changer.canonical_path || changer.canonical_path === changer.path)">-</span>
                </td>
                <td>{{ changer.status?.drives ?? '-' }}</td>
                <td>{{ changer.status?.robot_arms ?? '-' }}</td>
                <td>{{ changer.status?.slots ?? '-' }}</td>
                <td>{{ changer.status?.ie_slots ?? '-' }}</td>
                <td>
                  <div v-if="changer.drive_identifiers?.length">
                    <div v-for="drive in changer.drive_identifiers" :key="`${changer.path}-${drive.element_address}`">
                      Element {{ drive.element_address }}:
                      {{ (drive.identifiers || []).join(' | ') || '-' }}
                    </div>
                  </div>
                  <span v-else>-</span>
                </td>
              </tr>
            </tbody>
          </q-markup-table>
        </q-card-section>
      </q-card>

      <q-card flat bordered class="q-mb-md">
        <q-card-section>
          <div class="text-subtitle2 q-mb-sm">Detected tape drives</div>
          <q-markup-table flat dense>
            <thead>
              <tr>
                <th class="text-left">Identification</th>
                <th class="text-left">Serial</th>
                <th class="text-left">Manufacturer</th>
                <th class="text-left">Type</th>
                <th class="text-left">Firmware</th>
                <th class="text-left">Selected Path</th>
                <th class="text-left">Additional Paths</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="drive in store.tapeDrives" :key="drive.path">
                <td>{{ drive.identifier || drive.display_name }}</td>
                <td>{{ drive.serial_number || '-' }}</td>
                <td>{{ drive.vendor || '-' }}</td>
                <td>{{ drive.model || '-' }}</td>
                <td>{{ drive.firmware_version || '-' }}</td>
                <td>{{ drive.path }}</td>
                <td>
                  <div v-for="alias in drive.aliases || []" :key="alias">
                    {{ alias }}
                  </div>
                  <span v-if="!drive.aliases?.length">-</span>
                </td>
              </tr>
            </tbody>
          </q-markup-table>
        </q-card-section>
      </q-card>

      <q-card v-for="changer in store.tapeChangers" :key="changer.path"
              flat bordered class="q-mb-md">
        <q-card-section>
          <div class="text-subtitle2 q-mb-sm">
            Assign drives to {{ changer.identifier || changer.display_name }}{{ changer.serial_number ? ` / ${changer.serial_number}` : '' }}
          </div>
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
  if (Array.isArray(suggestedAssignments) && suggestedAssignments.length > 0) {
    store.tapeAssignments = suggestedAssignments
    return
  }

  const validChangerPaths = new Set(store.tapeChangers.map((changer) => changer.path))
  const validDrivePaths = new Set(store.tapeDrives.map((drive) => drive.path))

  store.tapeAssignments = store.tapeAssignments
    .filter((assignment) => validChangerPaths.has(assignment.changer_path))
    .map((assignment) => ({
      changer_path: assignment.changer_path,
      drive_paths: assignment.drive_paths.filter((drivePath) => validDrivePaths.has(drivePath)),
    }))
    .filter((assignment) => assignment.drive_paths.length > 0)
}

function availableDriveOptions(changerPath) {
  const claimed = new Set(
    store.tapeAssignments
      .filter((assignment) => assignment.changer_path !== changerPath)
      .flatMap((assignment) => assignment.drive_paths)
  )

  return store.tapeDrives
    .filter((drive) => !claimed.has(drive.path) || drivePathsFor(changerPath).includes(drive.path))
    .map((drive) => ({
      label: `${drive.identifier || drive.display_name}${drive.serial_number ? ` / ${drive.serial_number}` : ''} (${drive.path})`,
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
