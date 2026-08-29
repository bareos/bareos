import { defineStore } from 'pinia'
import { ref, reactive } from 'vue'

export const useSetupStore = defineStore('setup', () => {
  const state = reactive({
    distro: '',
    version: '',
    pretty_name: '',
    arch: '',
    codename: '',
    hostname: '',
    package_manager: '',
    setup_version: '',
    peer_is_loopback: true,
    dry_run: false,
    subscription_credentials_in_browser: false,
    subscription_credentials_on_terminal: false,
    completed: [],
    finished: false,
    failed: ''
  })
  const repository = ref('subscription')
  const repositoryLogin = ref('')
  const repositoryPassword = ref('')
  const admin = ref(null)
  return {
    state, repository, repositoryLogin, repositoryPassword, admin
  }
})
