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
      <div class="text-center q-mb-lg">
        <img :src="bareosLogo" alt="Bareos" class="setup-logo" />
        <div class="text-h5 text-white text-weight-bold q-mt-sm">BAREOS</div>
        <div class="text-subtitle2 text-white">{{ t('Initial setup wizard') }}</div>
      </div>

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
                    <div class="col-12 col-sm-4">
                      <q-card flat bordered class="setup-highlight">
                        <q-card-section>
                          <div class="text-subtitle2">{{ t('Step 1') }}</div>
                          <div>{{ t('Review the automatic deployment defaults.') }}</div>
                        </q-card-section>
                      </q-card>
                    </div>
                    <div class="col-12 col-sm-4">
                      <q-card flat bordered class="setup-highlight">
                        <q-card-section>
                          <div class="text-subtitle2">{{ t('Step 2') }}</div>
                          <div>{{ t('Confirm passwords and optional advanced settings.') }}</div>
                        </q-card-section>
                      </q-card>
                    </div>
                    <div class="col-12 col-sm-4">
                      <q-card flat bordered class="setup-highlight">
                        <q-card-section>
                          <div class="text-subtitle2">{{ t('Step 3') }}</div>
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
              :title="t('Deployment')"
              icon="folder_open"
              :done="step > 2"
            >
              <div class="text-body2 q-mb-md">
                {{ t('Bareos preconfigures the first deployment automatically. Open the advanced section only when you need to change the defaults.') }}
              </div>

              <q-banner dense class="bg-grey-2 text-dark q-mb-md rounded-borders">
                <template #avatar><q-icon name="auto_fix_high" /></template>
                {{ t('Deployment') }} "{{ form.deploymentName }}" ·
                {{ t('Repository path') }} "{{ form.repositoryPath }}" ·
                {{ t('Workflow mode') }} {{ form.workflowMode }}
              </q-banner>

              <q-expansion-item
                v-model="showAdvancedDeployment"
                dense
                switch-toggle-side
                expand-separator
                icon="tune"
                :label="t('Advanced deployment settings')"
                data-testid="setup-advanced-deployment"
                class="q-mb-sm rounded-borders advanced-settings"
              >
                <div class="q-pa-md">
                  <q-input
                    v-model="form.deploymentId"
                    data-testid="setup-deployment-id"
                    :label="t('Deployment ID')"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.deploymentName"
                    data-testid="setup-deployment-name"
                    :label="t('Deployment name')"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.repositoryPath"
                    data-testid="setup-repository-path"
                    :label="t('Repository path')"
                    outlined
                    dense
                    class="q-mb-sm"
                    @update:model-value="repositoryPathTouched = true"
                  />
                  <q-select
                    v-model="form.workflowMode"
                    data-testid="setup-workflow-mode"
                    :label="t('Workflow mode')"
                    :options="workflowOptions"
                    option-label="label"
                    option-value="value"
                    emit-value
                    map-options
                    outlined
                    dense
                  />
                </div>
              </q-expansion-item>

              <q-stepper-navigation>
                <q-btn color="primary" :label="t('Continue')" @click="nextStep" />
                <q-btn flat color="primary" :label="t('Back')" class="q-ml-sm" @click="step = 1" />
              </q-stepper-navigation>
            </q-step>

            <q-step
              :name="3"
              :title="t('Services')"
              icon="dns"
              :done="step > 3"
            >
              <div class="text-body2 q-mb-md">
                {{ t('Bareos also preconfigures the first director, storage daemon, file daemon, and admin console. Adjust them only if the defaults are not suitable.') }}
              </div>

              <q-banner dense class="bg-grey-2 text-dark q-mb-md rounded-borders">
                <template #avatar><q-icon name="dns" /></template>
                {{ t('Director name') }} "{{ form.directorName }}" ·
                {{ t('Storage daemon name') }} "{{ form.storageName }}" ·
                {{ t('File daemon name') }} "{{ form.clientName }}" ·
                {{ t('Admin console name') }} "{{ form.consoleName }}" ·
                {{ t('Daemon address') }} {{ form.daemonAddress }}
              </q-banner>

              <q-expansion-item
                v-model="showAdvancedServices"
                dense
                switch-toggle-side
                expand-separator
                icon="tune"
                :label="t('Advanced service settings')"
                data-testid="setup-advanced-services"
                class="q-mb-md rounded-borders advanced-settings"
              >
                <div class="q-pa-md">
                  <q-input
                    v-model="form.daemonAddress"
                    data-testid="setup-daemon-address"
                    :label="t('Daemon address')"
                    :hint="t('Enter the FQDN of the Bareos server that will run the director, storage daemon, and file daemon.')"
                    outlined
                    dense
                    hide-bottom-space
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.directorPort"
                    data-testid="setup-director-port"
                    :label="t('Director port')"
                    type="number"
                    min="1"
                    max="65535"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.clientPort"
                    data-testid="setup-client-port"
                    :label="t('File daemon port')"
                    type="number"
                    min="1"
                    max="65535"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.storagePort"
                    data-testid="setup-storage-port"
                    :label="t('Storage daemon port')"
                    type="number"
                    min="1"
                    max="65535"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.directorName"
                    data-testid="setup-director-name"
                    :label="t('Director name')"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.storageName"
                    data-testid="setup-storage-name"
                    :label="t('Storage daemon name')"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.clientName"
                    data-testid="setup-client-name"
                    :label="t('File daemon name')"
                    outlined
                    dense
                    class="q-mb-sm"
                  />
                  <q-input
                    v-model="form.consoleName"
                    data-testid="setup-console-name"
                    :label="t('Admin console name')"
                    outlined
                    dense
                  />
                </div>
              </q-expansion-item>
              <q-input
                v-model="form.consolePassword"
                data-testid="setup-console-password"
                :label="t('Admin console password')"
                type="password"
                outlined
                dense
                class="q-mb-md"
                autocomplete="new-password"
              >
                <template #append>
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

              <q-banner dense class="bg-grey-2 text-dark q-mb-md rounded-borders">
                <template #avatar><q-icon name="settings" /></template>
                {{ t('The generated daemon resources use ports {directorPort}, {clientPort}, and {storagePort}. Unique daemon connection passwords and the admin password are generated automatically.', {
                  directorPort: form.directorPort,
                  clientPort: form.clientPort,
                  storagePort: form.storagePort,
                }) }}
              </q-banner>

              <q-stepper-navigation>
                <q-btn color="primary" :label="t('Continue')" @click="nextStep" />
                <q-btn flat color="primary" :label="t('Back')" class="q-ml-sm" @click="step = 2" />
              </q-stepper-navigation>
            </q-step>

            <q-step
              :name="4"
              :title="t('Review')"
              icon="fact_check"
            >
              <div class="text-body2 q-mb-md">
                {{ t('Review the generated values before creating the initial configuration.') }}
              </div>

              <q-list bordered separator class="rounded-borders q-mb-md">
                <q-item>
                  <q-item-section>
                    <q-item-label caption>{{ t('Daemon address') }}</q-item-label>
                    <q-item-label>
                      {{ form.daemonAddress }} · {{ t('Ports') }}
                      {{ form.directorPort }}/{{ form.clientPort }}/{{ form.storagePort }}
                    </q-item-label>
                  </q-item-section>
                </q-item>
                <q-item>
                  <q-item-section>
                    <q-item-label caption>{{ t('Initial resources') }}</q-item-label>
                    <q-item-label v-for="resourceGroup in initialResourceGroups" :key="resourceGroup.label">
                      <strong>{{ resourceGroup.label }}:</strong> {{ resourceGroup.items.join(', ') }}
                    </q-item-label>
                  </q-item-section>
                </q-item>
                <q-item>
                  <q-item-section>
                    <q-item-label caption>{{ t('Credentials') }}</q-item-label>
                    <q-item-label>{{ t('Unique daemon connection passwords and the admin console password are configured automatically.') }}</q-item-label>
                  </q-item-section>
                  <q-item-section side>
                    <q-btn
                      flat
                      dense
                      color="primary"
                      icon="content_copy"
                      no-caps
                      data-testid="setup-copy-admin-password"
                      :label="t('Copy admin password')"
                      @click="copyAdminPassword"
                    />
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
                  @click="step = 3"
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
import { computed, onMounted, reactive, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import LanguageSelect from '../components/LanguageSelect.vue'
import bareosLogo from '../assets/bareos-logo-small.png'
import borisIcon from '../assets/boris.png'
import { useAuthStore } from '../stores/auth.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  buildDefaultDaemonAddress,
  buildDefaultRepositoryPath,
  buildDefaultRuntimeRoot,
  getDefaultSetupClientPort,
  getDefaultSetupDirectorPort,
  getDefaultSetupStoragePort,
  DEFAULT_CATALOG_BACKUP_JOB_NAME,
  DEFAULT_CATALOG_FILESET_NAME,
  DEFAULT_CATALOG_NAME,
  DEFAULT_DEPLOYMENT_ID,
  DEFAULT_DIFFERENTIAL_POOL_NAME,
  DEFAULT_DIRECTOR_STORAGE_NAME,
  DEFAULT_FULL_POOL_NAME,
  DEFAULT_INCREMENTAL_POOL_NAME,
  DEFAULT_JOBDEFS_NAME,
  DEFAULT_LINUX_ALL_FILESET_NAME,
  DEFAULT_MONITOR_CONSOLE_NAME,
  DEFAULT_OPERATOR_PROFILE,
  DEFAULT_PRIMARY_BACKUP_JOB_NAME,
  DEFAULT_RESTORE_JOB_NAME,
  DEFAULT_SCRATCH_POOL_NAME,
  DEFAULT_SELFTEST_FILESET_NAME,
  DEFAULT_STORAGE_DEVICE_NAME,
  DEFAULT_WEBUI_ADMIN_PROFILE,
  DEFAULT_WEBUI_READONLY_PROFILE,
  DEFAULT_WEEKLY_CYCLE_AFTER_BACKUP_NAME,
  DEFAULT_WEEKLY_CYCLE_NAME,
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
const repositoryPathTouched = ref(false)
const showAdvancedDeployment = ref(false)
const showAdvancedServices = ref(false)
const errorMsg = ref(null)
const workflowOptions = [
  { label: 'review', value: 'review' },
  { label: 'direct_commit', value: 'direct_commit' },
]

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
  directorName: 'bareos-dir',
  storageName: 'bareos-sd',
  clientName: 'bareos-fd',
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

const initialResourceGroups = computed(() => ([
  {
    label: t('Daemons and access'),
    items: [
      form.directorName.trim(),
      DEFAULT_DIRECTOR_STORAGE_NAME,
      form.storageName.trim(),
      DEFAULT_STORAGE_DEVICE_NAME,
      form.clientName.trim(),
      form.consoleName.trim(),
      DEFAULT_MONITOR_CONSOLE_NAME,
    ].filter(Boolean),
  },
  {
    label: t('Catalog and profiles'),
    items: [
      DEFAULT_CATALOG_NAME,
      DEFAULT_OPERATOR_PROFILE,
      DEFAULT_WEBUI_READONLY_PROFILE,
      DEFAULT_WEBUI_ADMIN_PROFILE,
    ],
  },
  {
    label: t('Schedules, pools, and filesets'),
    items: [
      DEFAULT_WEEKLY_CYCLE_NAME,
      DEFAULT_WEEKLY_CYCLE_AFTER_BACKUP_NAME,
      DEFAULT_FULL_POOL_NAME,
      DEFAULT_DIFFERENTIAL_POOL_NAME,
      DEFAULT_INCREMENTAL_POOL_NAME,
      DEFAULT_SCRATCH_POOL_NAME,
      DEFAULT_SELFTEST_FILESET_NAME,
      DEFAULT_CATALOG_FILESET_NAME,
      DEFAULT_LINUX_ALL_FILESET_NAME,
    ],
  },
  {
    label: t('Jobs'),
    items: [
      DEFAULT_JOBDEFS_NAME,
      DEFAULT_PRIMARY_BACKUP_JOB_NAME,
      DEFAULT_CATALOG_BACKUP_JOB_NAME,
      DEFAULT_RESTORE_JOB_NAME,
    ],
  },
]))

watch(locale, (value) => {
  settings.setLocale(value)
})

watch(() => form.deploymentId, (value) => {
  if (!repositoryPathTouched.value) {
    form.repositoryPath = buildDefaultRepositoryPath(value)
  }
})

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
      t('Please complete the deployment details before continuing.'),
      deploymentReady.value
    )
    if (!ok) {
      return
    }
  }

  if (step.value === 3) {
    const ok = requireStep(
      t('Please complete the service names and passwords before continuing.'),
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

.advanced-settings {
  border: 1px solid rgba(0, 0, 0, 0.12);
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
