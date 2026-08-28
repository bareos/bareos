import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'

export const useSetupStore = defineStore('setup', () => {
  const state = reactive({
    distro: '',
    version: '',
    package_manager: '',
    setup_version: '',
    completed: [],
    finished: false,
    failed: ''
  })
  const repository = ref('subscription')
  const repositoryLogin = ref('')
  const repositoryPassword = ref('')
  const storagePath = ref('/var/lib/bareos/storage')
  const customizeStorage = ref(false)
  const admin = ref(null)
  return {
    state, repository, repositoryLogin, repositoryPassword, storagePath,
    customizeStorage, admin
  }
})
