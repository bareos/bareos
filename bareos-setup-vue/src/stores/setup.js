/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public
   License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
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
    platform_supported: true,
    package_manager_supported: true,
    repo_os_path: '',
    known_repo_os_paths: [],
    suggested_repo_os_paths: [],
    completed: [],
    finished: false,
    failed: ''
  })
  const repository = ref('subscription')
  const repositoryLogin = ref('')
  const repositoryPassword = ref('')
  const repoOsPath = ref('')
  const repoOsPathAcknowledged = ref(false)
  const admin = ref(null)
  return {
    state, repository, repositoryLogin, repositoryPassword,
    repoOsPath, repoOsPathAcknowledged, admin
  }
})
