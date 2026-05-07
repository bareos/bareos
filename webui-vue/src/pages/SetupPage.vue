<!--
  BAREOS® - Backup Archiving REcovery Open Sourced

  Copyright (C) 2026 Bareos GmbH & Co. KG

  This program is Free Software; you can redistribute it and/or
  modify it under the terms of version three of the GNU Affero General Public
  License as published by the Free Software Foundation and included
  in the file LICENSE.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.
-->

<template>
  <q-page class="setup-page flex flex-center">
    <div class="setup-shell">
      <q-card flat bordered class="setup-card">
        <q-card-section class="bg-primary text-white">
          <div class="text-h6">{{ t('Welcome to Bareos setup') }}</div>
          <div class="text-body2 q-mt-xs">
            {{ t('This wizard creates the first managed deployment and bootstraps the initial director, storage, client, and admin console resources.') }}
          </div>
        </q-card-section>

        <q-card-section>
          <q-banner dense class="bg-blue-1 text-primary q-mb-md rounded-borders">
            <template #avatar><q-icon name="info" /></template>
            {{ t('For production, first-run setup should later be protected with a one-time server-generated setup token.') }}
          </q-banner>

          <q-banner v-if="setup.error || errorMsg" data-testid="setup-error" dense class="bg-negative text-white q-mb-md rounded-borders">
            <template #avatar><q-icon name="error" /></template>
            {{ errorMsg || setup.error }}
          </q-banner>

          <q-stepper
            v-model="step"
            flat
            bordered
            animated
            color="primary"
            active-color="primary"
            done-color="positive"
          >
            <q-step
              :name="1"
              :title="t('Welcome')"
              icon="auto_fix_high"
              :done="step > 1"
            >
              <div class="wizard-intro">
                <img :src="borisIcon" alt="Boris wizard" class="boris-icon" />
                <div class="wizard-intro-copy">
                  <div class="text-h6 q-mb-sm">{{ t('Boris will guide the initial setup') }}</div>
                    <p class="q-mb-md">
                      {{ t('The wizard uses sensible defaults for the first managed deployment, the daemon names, and the repository layout. Advanced options remain available when you need to adjust them. After creating the configuration it validates the repository and briefly starts the generated daemons before Bareos switches to the normal login flow.') }}
                    </p>
                  <LanguageSelect
                    v-model="locale"
                    data-testid="setup-language"
                    :label="t('Wizard language')"
                    class="q-mb-md"
                  />
                  <div class="row q-col-gutter-sm">
                    <div class="col-12 col-sm-6">
                      <q-card flat bordered class="setup-highlight">
                        <q-card-section>
                          <div class="text-subtitle2">{{ t('Step 1') }}</div>
                          <div>{{ t('Provide the server address and administrator credentials.') }}</div>
                        </q-card-section>
                      </q-card>
                    </div>
                    <div class="col-12 col-sm-6">
                      <q-card flat bordered class="setup-highlight">
                        <q-card-section>
                          <div class="text-subtitle2">{{ t('Step 2') }}</div>
                          <div>{{ t('Review and create the initial configuration.') }}</div>
                        </q-card-section>
                      </q-card>
                    </div>
                  </div>
                </div>
              </div>

              <q-stepper-navigation>
                <q-btn color="primary" :label="t('Start wizard')" @click="nextStep" />
              </q-stepper-navigation>
            </q-step>

            <q-step
              :name="2"
              :title="t('Services')"
              icon="dns"
              :done="step > 2"
            >
              <div class="text-body2 q-mb-md">
                {{ t('Enter the Bareos server address and the first administrator credentials.') }}
              </div>

              <q-input
                v-model="form.daemonAddress"
                data-testid="setup-daemon-address"
                :label="t('Bareos Server Address (FQDN)')"
                :hint="t('Enter the FQDN of the Bareos server that will run the director, storage daemon, and file daemon.')"
                outlined
                dense
                hide-bottom-space
                class="q-mb-md"
              />
              <div class="text-subtitle2 q-mb-sm">{{ t('Administrator') }}</div>
              <div class="row q-col-gutter-md q-mb-md">
                <div class="col-12 col-md-5">
                  <q-input
                    v-model="form.consoleName"
                    data-testid="setup-console-name"
                    :label="t('Administrator Name')"
                    outlined
                    dense
                  />
                </div>
                <div class="col-12 col-md-7">
                  <q-input
                    v-model="form.consolePassword"
                    data-testid="setup-console-password"
                    :label="t('Administrator Password')"
                    :type="showAdminPassword ? 'text' : 'password'"
                    outlined
                    dense
                    autocomplete="new-password"
                  >
                    <template #append>
                      <q-btn
                        flat
                        dense
                        round
                        color="primary"
                        :icon="showAdminPassword ? 'visibility_off' : 'visibility'"
                        :aria-label="showAdminPassword
                          ? t('Hide admin password')
                          : t('Show admin password')"
                        :title="showAdminPassword
                          ? t('Hide admin password')
                          : t('Show admin password')"
                        @click="showAdminPassword = !showAdminPassword"
                      />
                      <q-btn
                        flat
                        dense
                        round
                        color="primary"
                        icon="content_copy"
                        :aria-label="t('Copy admin password')"
                        :title="t('Copy admin password')"
                        @click="copyAdminPassword"
                      />
                      <q-btn
                        flat
                        dense
                        no-caps
                        color="primary"
                        :label="t('Regenerate')"
                        @click="form.consolePassword = generateSetupPassword()"
                      />
                    </template>
                  </q-input>
                </div>
              </div>

              <q-stepper-navigation>
                <q-btn color="primary" :label="t('Continue')" @click="nextStep" />
                <q-btn flat color="primary" :label="t('Back')" class="q-ml-sm" @click="step = 1" />
              </q-stepper-navigation>
            </q-step>

            <q-step
              :name="3"
              :title="t('Review')"
              icon="fact_check"
            >
              <div class="text-body2 q-mb-md">
                {{ t('Review the generated values before creating the initial configuration.') }}
              </div>

              <q-list bordered separator class="rounded-borders q-mb-md">
                <q-item>
                  <q-item-section>
                    <q-item-label caption>{{ t('Bareos Server Address (FQDN)') }}</q-item-label>
                    <q-item-label>
                      {{ form.daemonAddress }}
                    </q-item-label>
                  </q-item-section>
                </q-item>
                <q-item>
                  <q-item-section>
                    <q-item-label caption>{{ t('Administrator Name') }}</q-item-label>
                    <q-item-label>{{ form.consoleName }}</q-item-label>
                  </q-item-section>
                </q-item>
              </q-list>

              <q-banner
                v-if="setup.creating || setup.progressTitle || setup.progressLogs.length || setup.completed"
                dense
                :class="setup.completed
                  ? 'bg-positive text-white q-mb-md rounded-borders'
                  : 'bg-blue-1 text-primary q-mb-md rounded-borders'"
                data-testid="setup-progress"
              >
                <template #avatar>
                  <q-icon
                    v-if="setup.completed"
                    name="check_circle"
                  />
                  <q-spinner
                    v-else
                    color="primary"
                    size="sm"
                  />
                </template>
                <div class="text-subtitle2">
                  {{ setup.progressTitle || t('Creating initial configuration') }}
                </div>
                <div
                  v-if="setup.progressLogs.length"
                  class="setup-progress-log q-mt-sm text-body2"
                >
                  <div
                    v-for="(entry, index) in setup.progressLogs"
                    :key="`${index}-${entry}`"
                  >
                    {{ entry }}
                  </div>
                </div>
              </q-banner>

              <q-card
                v-if="setup.completed"
                flat
                bordered
                class="q-mb-md"
                data-testid="setup-storage-bootstrap"
              >
                <q-card-section>
                  <div class="text-subtitle1">
                    {{ t('Remote storage daemon (optional)') }}
                  </div>
                  <div class="text-body2 q-mt-xs">
                    {{ t('Create a storage bootstrap session, run the generated bareos-sd --discovery command on the storage host, then select the reported archive path once discovery data arrives.') }}
                  </div>
                </q-card-section>

                <q-separator />

                <q-card-section>
                  <q-banner
                    v-if="setup.storageBootstrapError"
                    dense
                    class="bg-negative text-white q-mb-md rounded-borders"
                  >
                    <template #avatar><q-icon name="error" /></template>
                    {{ setup.storageBootstrapError }}
                  </q-banner>

                  <div
                    v-if="!storageBootstrapSession"
                    class="column items-start q-gutter-md"
                  >
                    <div class="text-body2">
                      {{ t('Use this optional step when the storage daemon should be prepared on another host before it receives its final configuration.') }}
                    </div>
                    <q-btn
                      data-testid="setup-storage-bootstrap-create"
                      color="primary"
                      :label="t('Prepare remote storage daemon')"
                      :loading="setup.storageBootstrapLoading"
                      @click="prepareRemoteStorage"
                    />
                  </div>

                  <div v-else class="column q-gutter-md">
                    <div class="row items-center q-col-gutter-md">
                      <div class="col-auto">
                        <q-chip
                          square
                          dense
                          text-color="white"
                          :color="storageBootstrapStatusMeta.color"
                          data-testid="setup-storage-bootstrap-status"
                        >
                          {{ storageBootstrapStatusMeta.label }}
                        </q-chip>
                      </div>
                      <div class="col">
                        <div class="text-body2 text-weight-medium">
                          {{ t('Session {id}', { id: storageBootstrapSession.id }) }}
                        </div>
                        <div class="text-caption text-grey-8">
                          {{ storageBootstrapStatusMeta.description }}
                        </div>
                      </div>
                      <div class="col-auto">
                        <q-btn
                          flat
                          color="primary"
                          icon="refresh"
                          :label="t('Refresh')"
                          :loading="setup.storageBootstrapLoading"
                          @click="refreshRemoteStorage"
                        />
                      </div>
                    </div>

                    <q-input
                      :model-value="storageBootstrapCommand"
                      data-testid="setup-storage-bootstrap-command"
                      :label="t('Run on the storage host')"
                      readonly
                      autogrow
                      type="textarea"
                      outlined
                    >
                      <template #append>
                        <q-btn
                          flat
                          dense
                          round
                          color="primary"
                          icon="content_copy"
                          :aria-label="t('Copy bootstrap command')"
                          :title="t('Copy bootstrap command')"
                          @click="copyStorageBootstrapCommand"
                        />
                      </template>
                    </q-input>

                    <q-banner
                      dense
                      class="bg-blue-1 text-primary rounded-borders"
                    >
                      <template #avatar><q-icon name="terminal" /></template>
                      {{ t('The storage host registers itself with bconfig-service, uploads its discovery report, waits for your selected archive path, and then downloads its config bundle.') }}
                    </q-banner>

                    <div
                      v-if="storageBootstrapSession.selection"
                      class="text-body2"
                    >
                      {{ t('Selected archive path: {path}', {
                        path: storageBootstrapSession.selection.archive_path,
                      }) }}
                    </div>

                    <div
                      v-if="storageDiscoveryFilesystems.length"
                      class="column q-gutter-md"
                    >
                      <div class="text-subtitle2">
                        {{ t('Detected filesystems') }}
                      </div>
                      <q-select
                        v-model="storageBootstrapSelection.filesystemMountpoint"
                        :options="storageFilesystemOptions"
                        emit-value
                        map-options
                        outlined
                        dense
                        :label="t('Archive filesystem')"
                      />
                      <q-input
                        v-model="storageBootstrapSelection.archivePath"
                        outlined
                        dense
                        :label="t('Archive path')"
                        :hint="t('Choose the directory that should become the File device archive path on the storage host.')"
                      />
                      <q-btn
                        color="primary"
                        :label="t('Use selected archive path')"
                        :loading="setup.submittingStorageSelection"
                        :disable="!storageBootstrapSelection.archivePath.trim()"
                        @click="submitRemoteStorageLayout"
                      />

                      <q-list bordered separator class="rounded-borders">
                        <q-item
                          v-for="filesystem in storageDiscoveryFilesystems"
                          :key="filesystem.mountpoint"
                        >
                          <q-item-section>
                            <q-item-label class="text-weight-medium">
                              {{ filesystem.mountpoint }}
                            </q-item-label>
                            <q-item-label caption>
                              {{ filesystem.filesystem_type }} ·
                              {{ t('Free: {size}', {
                                size: formatByteCount(filesystem.free_bytes),
                              }) }} ·
                              {{ filesystem.suitable_for_archive
                                ? t('recommended')
                                : t('not recommended') }}
                            </q-item-label>
                            <q-item-label
                              v-if="filesystem.recommended_archive_path"
                              caption
                            >
                              {{ t('Suggested archive path: {path}', {
                                path: filesystem.recommended_archive_path,
                              }) }}
                            </q-item-label>
                          </q-item-section>
                        </q-item>
                      </q-list>
                    </div>

                    <q-banner
                      v-else-if="storageBootstrapSession.status === 'discovered'"
                      dense
                      class="bg-warning text-dark rounded-borders"
                    >
                      <template #avatar><q-icon name="warning" /></template>
                      {{ t('No filesystem candidates were reported by the storage host.') }}
                    </q-banner>

                    <div
                      v-if="storageDiscoveryTapeDevices.length || storageDiscoveryChangers.length"
                      class="column q-gutter-md"
                    >
                      <div class="text-subtitle2">
                        {{ t('Detected tape hardware') }}
                      </div>
                      <q-list bordered separator class="rounded-borders">
                        <q-item
                          v-for="device in storageDiscoveryTapeDevices"
                          :key="device.device_node"
                        >
                          <q-item-section>
                            <q-item-label class="text-weight-medium">
                              {{ device.device_node }}
                            </q-item-label>
                            <q-item-label caption>
                              {{ [device.vendor, device.model, device.serial]
                                .filter(Boolean)
                                .join(' · ') }}
                            </q-item-label>
                          </q-item-section>
                        </q-item>
                        <q-item
                          v-for="changer in storageDiscoveryChangers"
                          :key="changer.device_node"
                        >
                          <q-item-section>
                            <q-item-label class="text-weight-medium">
                              {{ changer.device_node }}
                            </q-item-label>
                            <q-item-label caption>
                              {{ t('{count} drive entries', {
                                count: Array.isArray(changer.drives)
                                  ? changer.drives.length
                                  : 0,
                              }) }}
                            </q-item-label>
                          </q-item-section>
                        </q-item>
                      </q-list>
                    </div>
                  </div>
                </q-card-section>
              </q-card>

              <q-stepper-navigation>
                <q-btn
                  v-if="!setup.completed"
                  data-testid="setup-submit"
                  color="primary"
                  :label="t('Create initial configuration')"
                  :loading="setup.creating"
                  @click="submitSetup"
                />
                <q-btn
                  v-else
                  data-testid="setup-continue-login"
                  color="primary"
                  :label="t('Continue to login')"
                  @click="continueToLogin"
                />
                <q-btn
                  v-if="!setup.creating && !setup.completed"
                  flat
                  color="primary"
                  :label="t('Back')"
                  class="q-ml-sm"
                  @click="step = 2"
                />
              </q-stepper-navigation>
            </q-step>
          </q-stepper>
        </q-card-section>
      </q-card>
    </div>
  </q-page>
</template>

<script setup>
import {
  computed,
  onBeforeUnmount,
  onMounted,
  reactive,
  ref,
  watch,
} from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import LanguageSelect from '../components/LanguageSelect.vue'
import borisIcon from '../assets/boris.png'
import { useAuthStore } from '../stores/auth.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  buildDefaultSetupBootstrapUrl,
  buildDefaultDaemonAddress,
  buildDefaultRepositoryPath,
  buildDefaultRuntimeRoot,
  buildStorageBootstrapCommand,
  deriveSetupDaemonNames,
  DEFAULT_STORAGE_DEVICE_NAME,
  getDefaultSetupClientPort,
  getDefaultSetupDirectorPort,
  getDefaultSetupStoragePort,
  DEFAULT_DEPLOYMENT_ID,
  generateSetupPassword,
  useSetupStore,
} from '../stores/setup.js'

const router = useRouter()
const auth = useAuthStore()
const setup = useSetupStore()
const settings = useSettingsStore()
const { t } = useI18n()
const $q = useQuasar()

const step = ref(1)
const locale = ref(settings.locale)
const showAdminPassword = ref(false)
const errorMsg = ref(null)
let storageBootstrapPollHandle = null
const defaultDaemonNames = deriveSetupDaemonNames(buildDefaultDaemonAddress())
const storageBootstrapSelection = reactive({
  filesystemMountpoint: '',
  archivePath: '',
})

const form = reactive({
  deploymentId: DEFAULT_DEPLOYMENT_ID,
  deploymentName: 'Production',
  repositoryPath: buildDefaultRepositoryPath(DEFAULT_DEPLOYMENT_ID),
  runtimeRoot: buildDefaultRuntimeRoot(DEFAULT_DEPLOYMENT_ID),
  workflowMode: 'review',
  daemonAddress: buildDefaultDaemonAddress(),
  directorPort: getDefaultSetupDirectorPort(),
  clientPort: getDefaultSetupClientPort(),
  storagePort: getDefaultSetupStoragePort(),
  directorName: defaultDaemonNames.directorName,
  storageName: defaultDaemonNames.storageName,
  clientName: defaultDaemonNames.clientName,
  consoleName: 'admin',
  consolePassword: generateSetupPassword(),
})

const deploymentReady = computed(() => (
  form.deploymentId.trim()
  && form.deploymentName.trim()
  && form.repositoryPath.trim()
  && form.runtimeRoot.trim()
  && form.workflowMode.trim()
))

const servicesReady = computed(() => (
  form.daemonAddress.trim()
  && parsePort(form.directorPort)
  && parsePort(form.clientPort)
  && parsePort(form.storagePort)
  && form.directorName.trim()
  && form.storageName.trim()
  && form.clientName.trim()
  && form.consoleName.trim()
))

const credentialsReady = computed(() => (
  form.consolePassword.trim()
))
const storageBootstrapSession = computed(() => setup.storageBootstrapSession)
const storageDiscoveryReport = computed(() => (
  storageBootstrapSession.value?.discovery_report ?? null
))
const storageDiscoveryFilesystems = computed(() => (
  Array.isArray(storageDiscoveryReport.value?.filesystems)
    ? storageDiscoveryReport.value.filesystems
    : []
))
const storageDiscoveryTapeDevices = computed(() => (
  Array.isArray(storageDiscoveryReport.value?.tape_devices)
    ? storageDiscoveryReport.value.tape_devices
    : []
))
const storageDiscoveryChangers = computed(() => (
  Array.isArray(storageDiscoveryReport.value?.changers)
    ? storageDiscoveryReport.value.changers
    : []
))
const storageFilesystemOptions = computed(() => (
  storageDiscoveryFilesystems.value.map(filesystem => ({
    label: `${filesystem.mountpoint} (${filesystem.filesystem_type}, ${t('Free: {size}', {
      size: formatByteCount(filesystem.free_bytes),
    })})`,
    value: filesystem.mountpoint,
  }))
))
const storageBootstrapCommand = computed(() => buildStorageBootstrapCommand({
  bootstrapUrl: buildDefaultSetupBootstrapUrl(),
  sessionId: storageBootstrapSession.value?.id,
  bootstrapToken: storageBootstrapSession.value?.bootstrap_token,
}))
const storageBootstrapStatusMeta = computed(() => (
  describeStorageBootstrapStatus(storageBootstrapSession.value?.status)
))

watch(locale, (value) => {
  settings.setLocale(value)
})

watch(() => form.daemonAddress, (value) => {
  const names = deriveSetupDaemonNames(value)
  form.directorName = names.directorName
  form.storageName = names.storageName
  form.clientName = names.clientName
})

watch(storageDiscoveryFilesystems, (filesystems) => {
  if (!filesystems.length) {
    storageBootstrapSelection.filesystemMountpoint = ''
    if (!storageBootstrapSession.value?.selection?.archive_path) {
      storageBootstrapSelection.archivePath = ''
    }
    return
  }

  const hasCurrentSelection = filesystems.some(filesystem => (
    filesystem.mountpoint === storageBootstrapSelection.filesystemMountpoint
  ))
  if (hasCurrentSelection) {
    return
  }

  const preferredFilesystem = filesystems.find(filesystem => (
    filesystem.suitable_for_archive
  )) ?? filesystems[0]
  storageBootstrapSelection.filesystemMountpoint = preferredFilesystem.mountpoint
}, { immediate: true })

watch(() => storageBootstrapSelection.filesystemMountpoint, (next, previous) => {
  const previousFilesystem = storageDiscoveryFilesystems.value.find(filesystem => (
    filesystem.mountpoint === previous
  )) ?? null
  const nextFilesystem = storageDiscoveryFilesystems.value.find(filesystem => (
    filesystem.mountpoint === next
  )) ?? null
  const previousSuggestion = buildArchivePathSuggestion(previousFilesystem)

  if (!storageBootstrapSelection.archivePath.trim()
    || storageBootstrapSelection.archivePath === previousSuggestion) {
    storageBootstrapSelection.archivePath
      = buildArchivePathSuggestion(nextFilesystem)
  }
})

watch(() => storageBootstrapSession.value?.selection?.archive_path, (archivePath) => {
  if (typeof archivePath === 'string' && archivePath.trim()) {
    storageBootstrapSelection.archivePath = archivePath
  }
})

watch(() => storageBootstrapSession.value?.selection?.filesystem_mountpoint, (mountpoint) => {
  if (typeof mountpoint === 'string' && mountpoint.trim()) {
    storageBootstrapSelection.filesystemMountpoint = mountpoint
  }
})

watch(() => `${storageBootstrapSession.value?.id ?? ''}:${storageBootstrapSession.value?.status ?? ''}`, () => {
  restartStorageBootstrapPolling()
}, { immediate: true })

onMounted(async () => {
  if (setup.mode === 'unknown' || setup.mode === 'error') {
    try {
      await setup.refresh(true)
    } catch {
      // The wizard already renders the setup store error banner.
    }
  }

  if (setup.isReady) {
    router.replace({ name: 'login' })
  }
})

onBeforeUnmount(() => {
  stopStorageBootstrapPolling()
})

function requireStep(message, condition) {
  if (!condition) {
    errorMsg.value = message
    return false
  }

  errorMsg.value = null
  return true
}

function nextStep() {
  if (step.value === 2) {
    const ok = requireStep(
      t('Please complete the server details and administrator credentials before continuing.'),
      servicesReady.value && credentialsReady.value
    )
    if (!ok) {
      return
    }
  }

  step.value += 1
}

function parsePort(value) {
  const parsed = Number.parseInt(String(value ?? '').trim(), 10)
  return Number.isInteger(parsed) && parsed >= 1 && parsed <= 65535
    ? parsed
    : null
}

function formatByteCount(value) {
  const size = Number(value)
  if (!Number.isFinite(size) || size < 0) {
    return t('unknown')
  }

  const units = ['B', 'KiB', 'MiB', 'GiB', 'TiB', 'PiB']
  let current = size
  let unitIndex = 0
  while (current >= 1024 && unitIndex < units.length - 1) {
    current /= 1024
    unitIndex += 1
  }
  const precision = unitIndex === 0 ? 0 : 1
  return `${current.toFixed(precision)} ${units[unitIndex]}`
}

function buildArchivePathSuggestion(filesystem) {
  const suggested = filesystem?.recommended_archive_path
  if (typeof suggested === 'string' && suggested.trim()) {
    return suggested.trim()
  }

  const mountpoint = String(filesystem?.mountpoint ?? '').trim()
  if (!mountpoint) {
    return '/var/lib/bareos/storage'
  }

  return `${mountpoint.replace(/\/$/, '') || ''}/bareos/storage`
}

function describeStorageBootstrapStatus(status) {
  switch (status) {
    case 'pending':
      return {
        color: 'primary',
        label: t('Pending'),
        description: t('Run the bootstrap command on the storage host to start discovery.'),
      }
    case 'registered':
      return {
        color: 'primary',
        label: t('Registered'),
        description: t('The storage host contacted bconfig-service and is gathering discovery data.'),
      }
    case 'discovered':
      return {
        color: 'info',
        label: t('Discovered'),
        description: t('Select the final archive path from the reported filesystem candidates.'),
      }
    case 'selected':
      return {
        color: 'positive',
        label: t('Selected'),
        description: t('The archive path was saved. The storage host can now fetch its config bundle.'),
      }
    case 'config_ready':
      return {
        color: 'positive',
        label: t('Config ready'),
        description: t('The generated storage config bundle is ready for download on the storage host.'),
      }
    case 'applying':
      return {
        color: 'accent',
        label: t('Applying'),
        description: t('The storage host is installing and validating the generated config.'),
      }
    case 'applied':
      return {
        color: 'positive',
        label: t('Applied'),
        description: t('The storage host reported a successful bootstrap apply.'),
      }
    case 'failed':
      return {
        color: 'negative',
        label: t('Failed'),
        description: t('The storage host reported a bootstrap failure.'),
      }
    case 'expired':
      return {
        color: 'negative',
        label: t('Expired'),
        description: t('The bootstrap session expired before the workflow completed.'),
      }
    default:
      return {
        color: 'grey-7',
        label: t('Unknown'),
        description: t('Waiting for storage bootstrap status information.'),
      }
  }
}

function shouldPollStorageBootstrapSession(session) {
  return Boolean(session?.id)
    && !['applied', 'failed', 'expired'].includes(session.status)
}

function stopStorageBootstrapPolling() {
  if (storageBootstrapPollHandle) {
    clearInterval(storageBootstrapPollHandle)
    storageBootstrapPollHandle = null
  }
}

function restartStorageBootstrapPolling() {
  stopStorageBootstrapPolling()
  if (!shouldPollStorageBootstrapSession(storageBootstrapSession.value)) {
    return
  }

  storageBootstrapPollHandle = setInterval(async () => {
    try {
      await setup.refreshStorageBootstrapSession(storageBootstrapSession.value.id)
    } catch {
      // The dedicated storage bootstrap error banner renders the latest error.
    }
  }, 2000)
}

async function copyAdminPassword() {
  try {
    await navigator.clipboard.writeText(form.consolePassword)
    $q.notify({
      type: 'positive',
      message: t('Admin password copied to clipboard'),
      timeout: 1500,
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not copy admin password: {message}', {
        message: error?.message ?? String(error),
      }),
    })
  }
}

async function copyStorageBootstrapCommand() {
  if (!storageBootstrapCommand.value) {
    return
  }

  try {
    await navigator.clipboard.writeText(storageBootstrapCommand.value)
    $q.notify({
      type: 'positive',
      message: t('Bootstrap command copied to clipboard'),
      timeout: 1500,
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not copy bootstrap command: {message}', {
        message: error?.message ?? String(error),
      }),
    })
  }
}

async function submitSetup() {
  const ready = requireStep(
    t('Please complete all wizard steps before creating the initial configuration.'),
    deploymentReady.value && servicesReady.value && credentialsReady.value
  )
  if (!ready) {
    return
  }

  try {
    errorMsg.value = null
      await setup.createInitialDeployment({
        deploymentId: form.deploymentId.trim(),
        deploymentName: form.deploymentName.trim(),
        repositoryPath: form.repositoryPath.trim(),
        runtimeRoot: form.runtimeRoot.trim(),
        workflowMode: form.workflowMode,
        daemonAddress: form.daemonAddress.trim(),
        directorPort: parsePort(form.directorPort),
        clientPort: parsePort(form.clientPort),
        storagePort: parsePort(form.storagePort),
        directorName: form.directorName.trim(),
        storageName: form.storageName.trim(),
      clientName: form.clientName.trim(),
      consoleName: form.consoleName.trim(),
      consolePassword: form.consolePassword,
    })

    settings.loginUsername = form.consoleName.trim()
    settings.directorName = form.directorName.trim()
    auth.stageSetupPassword(form.consolePassword)
  } catch (e) {
    errorMsg.value = e?.message ?? String(e)
  }
}

async function prepareRemoteStorage() {
  try {
    await setup.createStorageBootstrapSession({
      deploymentId: form.deploymentId.trim(),
      storageName: form.storageName.trim(),
    })
  } catch {
    // The dedicated storage bootstrap error banner renders the latest error.
  }
}

async function refreshRemoteStorage() {
  if (!storageBootstrapSession.value?.id) {
    return
  }

  try {
    await setup.refreshStorageBootstrapSession(storageBootstrapSession.value.id)
  } catch {
    // The dedicated storage bootstrap error banner renders the latest error.
  }
}

async function submitRemoteStorageLayout() {
  if (!storageBootstrapSession.value?.id) {
    return
  }

  const archivePath = storageBootstrapSelection.archivePath.trim()
  const filesystemMountpoint
    = storageBootstrapSelection.filesystemMountpoint.trim()
  if (!archivePath) {
    return
  }

  try {
    await setup.submitStorageBootstrapSelection(
      storageBootstrapSession.value.id,
      {
        filesystem_mountpoint: filesystemMountpoint || null,
        archive_path: archivePath,
        device_name: DEFAULT_STORAGE_DEVICE_NAME,
      }
    )
  } catch {
    // The dedicated storage bootstrap error banner renders the latest error.
  }
}

function continueToLogin() {
  router.push({ name: 'login' })
}
</script>

<style scoped>
.setup-page {
  background: linear-gradient(#ffffff, #0075be);
  min-height: 100vh;
  padding: 24px;
}

.setup-shell {
  width: min(920px, 100%);
}

.setup-card {
  overflow: hidden;
}

.setup-logo {
  height: 48px;
}

.wizard-intro {
  display: flex;
  align-items: center;
  gap: 24px;
}

.wizard-intro-copy {
  flex: 1;
}

.boris-icon {
  width: min(240px, 35vw);
  min-width: 180px;
  height: auto;
  align-self: flex-start;
}

.setup-highlight {
  height: 100%;
}

.text-break {
  word-break: break-word;
}

.setup-progress-log {
  max-height: 180px;
  overflow: auto;
  white-space: pre-wrap;
}

@media (max-width: 700px) {
  .wizard-intro {
    flex-direction: column;
    text-align: center;
  }

  .boris-icon {
    width: min(220px, 70vw);
    min-width: 0;
  }
}
</style>
