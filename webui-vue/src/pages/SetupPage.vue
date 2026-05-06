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
import { computed, onMounted, reactive, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useQuasar } from 'quasar'
import LanguageSelect from '../components/LanguageSelect.vue'
import borisIcon from '../assets/boris.png'
import { useAuthStore } from '../stores/auth.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  buildDefaultDaemonAddress,
  buildDefaultRepositoryPath,
  buildDefaultRuntimeRoot,
  deriveSetupDaemonNames,
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
const defaultDaemonNames = deriveSetupDaemonNames(buildDefaultDaemonAddress())

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

watch(locale, (value) => {
  settings.setLocale(value)
})

watch(() => form.daemonAddress, (value) => {
  const names = deriveSetupDaemonNames(value)
  form.directorName = names.directorName
  form.storageName = names.storageName
  form.clientName = names.clientName
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
