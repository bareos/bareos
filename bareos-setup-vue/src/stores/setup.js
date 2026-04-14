import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'

export const useSetupStore = defineStore('setup', () => {
  const osInfo = ref(null)

  const repoType = ref('subscription')
  const repoCredentials = reactive({
    login: '',
    password: '',
  })
  const repoAdded = ref(false)
  const packagesInstalled = ref(false)
  const storageConfigured = ref(false)

  const storageTargets = reactive({
    disk: true,
    tape: false,
  })

  const diskConfig = reactive({
    path: '/var/lib/bareos/storage',
    dedupable: false,
    concurrentJobs: 15,
  })

  const diskInventory = ref([])
  const tapeChangers = ref([])
  const tapeDrives = ref([])
  const tapeAssignments = ref([])
  const tapeTargetAutoSelected = ref(false)

  const dbConfig = reactive({
    createDb: true,
  })

  const adminUser = reactive({
    username: 'admin',
    password: '',
  })

  const setupDone = ref(false)
  const stepIndex = ref(0)
  const steps = [
    'welcome',
    'os',
    'repo',
    'install',
    'targets',
    'disk',
    'tape',
    'database',
    'passwords',
    'summary',
  ]

  function nextStep() {
    if (stepIndex.value < steps.length - 1) stepIndex.value++
  }

  function prevStep() {
    if (stepIndex.value > 0) stepIndex.value--
  }

  function goTo(name) {
    const idx = steps.indexOf(name)
    if (idx >= 0) stepIndex.value = idx
  }

  function resetStorageConfigured() {
    storageConfigured.value = false
  }

  return {
    osInfo,
    repoType,
    repoCredentials,
    repoAdded,
    packagesInstalled,
    storageConfigured,
    storageTargets,
    diskConfig,
    diskInventory,
    tapeChangers,
    tapeDrives,
    tapeAssignments,
    tapeTargetAutoSelected,
    dbConfig,
    adminUser,
    setupDone,
    stepIndex,
    steps,
    nextStep,
    prevStep,
    goTo,
    resetStorageConfigured,
  }
})
