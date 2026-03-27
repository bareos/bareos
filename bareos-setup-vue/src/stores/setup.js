import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'

export const useSetupStore = defineStore('setup', () => {
  // OS info from backend
  const osInfo = ref(null)   // { distro, version, pkg_mgr, codename, arch }

  // Wizard selections
  const components = reactive({
    director: true,
    storage:  true,
    filedaemon: true,
    webui:    false,
  })

  // Repo type: 'community' | 'subscription'
  const repoType     = ref('community')
  const repoAdded    = ref(false)
  const packagesInstalled = ref(false)

  // DB config
  const dbConfig = reactive({
    createDb: true,
    dbName:   'bareos',
    dbUser:   'bareos',
  })

  // Passwords (generated or user-entered)
  const passwords = reactive({
    director: '',
    storage:  '',
    filedaemon: '',
    webui:    '',
  })

  // Whether setup is complete
  const setupDone = ref(false)

  // Current wizard step index (0-based)
  const stepIndex = ref(0)
  const steps = [
    'welcome', 'os', 'components', 'repo',
    'install', 'database', 'passwords', 'summary'
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

  return {
    osInfo, components, repoType, repoAdded,
    packagesInstalled, dbConfig, passwords, setupDone,
    stepIndex, steps, nextStep, prevStep, goTo,
  }
})
