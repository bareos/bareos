<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="q-mb-md bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Director Scope') }}</span>
        <q-space />
        <q-chip dense square color="white" text-color="primary" :label="directorScopeLabel" />
      </q-card-section>
      <q-card-section>
        <q-select
          v-model="selectedDirectorsModel"
          data-testid="director-page-directors"
          :options="directorOptions"
          option-label="label"
          option-value="value"
          emit-value
          map-options
          multiple
          use-chips
          outlined
          dense
          :label="t('Directors')"
        />
        <div class="text-caption text-grey-6 q-mt-sm">
          {{ t('Select the directors that contribute to the status and message views.') }}
        </div>
      </q-card-section>
    </q-card>

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status"       :label="t('Status')"       no-caps />
      <q-tab name="messages"     :label="t('Messages')"     no-caps />
      <q-tab name="catalog"      :label="t('Catalog Maintenance')" no-caps :disable="isCommonDirectorPage" />
      <q-tab name="subscription" :label="t('Subscription')" no-caps :disable="isCommonDirectorPage" />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>
      <!-- STATUS -->
      <q-tab-panel name="status" class="q-pa-none">
        <q-banner
          v-if="statusDirectorErrors.length"
          rounded
          dense
          class="bg-warning text-black q-mb-md"
        >
          <template #avatar>
            <q-icon name="warning" />
          </template>
          <div v-for="item in statusDirectorErrors" :key="item.director">
            <strong>{{ item.director }}</strong>: {{ item.message }}
          </div>
        </q-banner>
        <q-inner-loading :showing="statusLoading" />
        <div v-if="statusError" class="text-negative q-pa-md">{{ statusError }}</div>
        <template v-else-if="statusCards.length">
          <div class="row q-col-gutter-md">

            <!-- Director Info – single summary line -->
            <div class="col-12">
                <q-card flat bordered class="bareos-panel">
                  <q-card-section class="panel-header row items-center">
                    <span>{{ t('Director Info') }}</span>
                    <q-space />
                    <span class="text-white text-caption q-mr-sm" style="opacity:0.7">↻ {{ statusCountdown }}s</span>
                    <q-btn flat round dense icon="refresh" color="white"
                           @click="manualRefreshStatus" :loading="statusLoading" />
                  </q-card-section>
                <q-card-section class="q-py-sm">
                  <div class="column q-gutter-md">
                    <div
                      v-for="card in statusCards"
                      :key="card.director"
                      class="row items-center q-gutter-sm text-body2 flex-wrap"
                    >
                      <q-chip dense square color="primary" text-color="white" icon="dns">
                        {{ card.director }}
                        <q-tooltip>Director: {{ card.director }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.config_warnings != null"
                              dense
                              square
                              :clickable="!isCommonDirectorPage"
                              :color="card.config_warnings ? 'negative' : 'positive'"
                              :icon="card.config_warnings ? 'error' : 'check_circle'"
                              text-color="white"
                              @click="!isCommonDirectorPage && showConfigStatus(card.director)">
                        {{ card.config_warnings ? t('Config Warning') : t('Config OK') }}
                        <q-tooltip>
                          {{
                            isCommonDirectorPage
                              ? t('Configuration status details stay scoped to a single director.')
                              : t('Click to show configuration status')
                          }}
                        </q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.version"
                              dense square color="blue-7" text-color="white" icon="info">
                        {{ card.version }}
                        <q-tooltip>Version: {{ card.version }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.release_date"
                              dense square color="blue-grey-6" text-color="white" icon="event">
                        {{ card.release_date }}
                        <q-tooltip>Released: {{ card.release_date }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.binary_info"
                              dense square color="blue-grey-7" text-color="white" icon="build">
                        {{ card.binary_info }}
                        <q-tooltip>Build: {{ card.binary_info }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.os"
                              dense square :color="card.osIcon.color" text-color="white"
                              :icon="card.osIcon.icon">
                        {{ card.os }}
                        <q-tooltip>OS: {{ card.os }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.daemon_started"
                              dense square color="teal-7" text-color="white" icon="schedule">
                        {{ settings.relativeTime ? formatDirectorRelativeTime(card.daemon_started, settings.locale) : card.daemon_started }}
                        <q-tooltip>Started: {{ card.daemon_started }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.jobs_run != null"
                              dense square color="purple-7" text-color="white" icon="check">
                        {{ card.jobs_run }}
                        <q-tooltip>Jobs Run: {{ card.jobs_run }}</q-tooltip>
                      </q-chip>
                      <q-chip v-if="card.jobs_running != null"
                              dense square color="orange-7" text-color="white" icon="play_arrow">
                        {{ card.jobs_running }}
                        <q-tooltip>Running: {{ card.jobs_running }}</q-tooltip>
                      </q-chip>
                    </div>
                  </div>
                </q-card-section>
              </q-card>
            </div>

            <!-- Scheduled Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">{{ t('Scheduled Jobs') }}</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!scheduledJobs.length" class="q-pa-md text-grey">
                    {{ t('No scheduled jobs.') }}
                  </div>
                  <q-table v-else flat dense
                    :rows="scheduledJobs"
                    :columns="scheduledJobCols"
                    row-key="scopeKey"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-director="props">
                      <td>
                        <q-chip dense square color="primary" text-color="white">
                          {{ props.value }}
                        </q-chip>
                      </td>
                    </template>
                    <template #body-cell-name="props">
                      <td>
                        <a href="#" class="text-primary" @click.prevent="openJobsByName(props.row)">
                          {{ props.value }}
                        </a>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-scheduled="props">
                      <td>
                        <span :title="settings.relativeTime ? props.value : undefined">
                          {{ settings.relativeTime ? formatDirectorRelativeTime(props.value, settings.locale) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-volume="props">
                      <td>
                        <a v-if="props.value"
                           href="#"
                           class="text-primary"
                           @click.prevent="openVolumeDetails(props.row)">
                          {{ props.value }}
                        </a>
                        <span v-else>—</span>
                      </td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

            <!-- Running Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">{{ t('Running Jobs') }}</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!runningJobs.length" class="q-pa-md text-grey">
                    {{ t('No jobs running.') }}
                  </div>
                  <q-table v-else flat dense
                    :rows="runningJobs"
                    :columns="runningJobCols"
                    row-key="scopeKey"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-director="props">
                      <td>
                        <q-chip dense square color="primary" text-color="white">
                          {{ props.value }}
                        </q-chip>
                      </td>
                    </template>
                    <template #body-cell-jobid="props">
                      <td>
                        <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                          {{ props.value }}
                        </a>
                      </td>
                    </template>
                    <template #body-cell-name="props">
                      <td>
                        <a href="#" class="text-primary" @click.prevent="openJobsByName(props.row)">
                          {{ props.value }}
                        </a>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-start_time="props">
                      <td>
                        <span :title="settings.relativeTime ? props.value : undefined">
                          {{ settings.relativeTime ? formatDirectorRelativeTime(props.value, settings.locale) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-files="props">
                      <td class="text-right" style="min-width:80px">
                        <div>{{ formatNumber(props.value ?? 0, settings.locale) }}</div>
                        <q-linear-progress v-if="!isWaiting(props.row.status)" indeterminate
                          color="teal" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-bytes="props">
                      <td class="text-right" style="min-width:90px">
                        <div>{{ formatBytes(Number(props.value) || 0) }}</div>
                        <q-linear-progress v-if="!isWaiting(props.row.status)" indeterminate
                          color="primary" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-status="props">
                      <td>
                        <span v-if="isWaiting(props.value)"
                              class="row items-center no-wrap q-gutter-x-xs">
                          <q-icon name="hourglass_empty" color="orange-7" size="16px"
                                  class="animated-spin" />
                          <span class="text-orange-7 text-caption">{{ props.value }}</span>
                        </span>
                        <JobStatusBadge v-else :status="props.value" />
                      </td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

            <!-- Terminated Jobs card -->
            <div class="col-12">
              <q-card flat bordered class="bareos-panel">
                <q-card-section class="panel-header">{{ t('Terminated Jobs') }}</q-card-section>
                <q-card-section class="q-pa-none">
                  <div v-if="!terminatedJobs.length" class="q-pa-md text-grey">
                    {{ t('No terminated jobs.') }}
                  </div>
                  <q-table v-else flat dense
                    :rows="terminatedJobs"
                    :columns="terminatedJobCols"
                    row-key="scopeKey"
                    hide-pagination
                    :rows-per-page-options="[0]"
                  >
                    <template #body-cell-director="props">
                      <td>
                        <q-chip dense square color="primary" text-color="white">
                          {{ props.value }}
                        </q-chip>
                      </td>
                    </template>
                    <template #body-cell-jobid="props">
                      <td>
                        <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                          {{ props.value }}
                        </a>
                      </td>
                    </template>
                    <template #body-cell-name="props">
                      <td>
                        <a href="#" class="text-primary" @click.prevent="openJobsByName(props.row)">
                          {{ props.value }}
                        </a>
                      </td>
                    </template>
                    <template #body-cell-level="props">
                      <td><JobLevelBadge v-if="levelCode(props.value)" :level="levelCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-type="props">
                      <td><JobTypeBadge v-if="typeCode(props.value)" :type="typeCode(props.value)" />
                          <span v-else>{{ props.value }}</span></td>
                    </template>
                    <template #body-cell-finished="props">
                      <td>
                        <span :title="settings.relativeTime ? props.value : undefined">
                          {{ settings.relativeTime ? formatDirectorRelativeTime(props.value, settings.locale) : props.value }}
                        </span>
                      </td>
                    </template>
                    <template #body-cell-files="props">
                      <td class="text-right" style="min-width:80px">
                        <div>{{ formatNumber(props.value ?? 0, settings.locale) }}</div>
                        <q-linear-progress
                          :value="(Number(props.value) || 0) / maxTermFiles"
                          color="teal" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-bytes="props">
                      <td class="text-right" style="min-width:90px">
                        <div>{{ formatBytes(Number(props.value) || 0) }}</div>
                        <q-linear-progress
                          :value="(Number(props.value) || 0) / maxTermBytes"
                          color="primary" track-color="grey-3"
                          size="4px" class="q-mt-xs" rounded />
                      </td>
                    </template>
                    <template #body-cell-status="props">
                      <td><JobStatusBadge :status="props.value" /></td>
                    </template>
                  </q-table>
                </q-card-section>
              </q-card>
            </div>

          </div>
        </template>
      </q-tab-panel>

      <!-- MESSAGES -->
      <q-tab-panel name="messages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Director Messages') }}</span>
            <q-space />
            <q-select v-model="messagesLimit" :options="[50,100,250,500]" dense outlined dark
                      style="width:80px" class="q-mr-sm" />
            <q-btn flat round dense icon="refresh" color="white" @click="refreshMessages" :loading="messagesLoading" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner
              v-if="messagesDirectorErrors.length"
              rounded
              dense
              class="bg-warning text-black"
            >
              <template #avatar>
                <q-icon name="warning" />
              </template>
              <div v-for="item in messagesDirectorErrors" :key="item.director">
                <strong>{{ item.director }}</strong>: {{ item.message }}
              </div>
            </q-banner>
            <q-inner-loading :showing="messagesLoading" />
            <div v-if="messagesError" class="q-pa-md text-negative">{{ messagesError }}</div>
            <div v-else-if="!logEntries.length && !messagesLoading" class="q-pa-md text-grey">{{ t('(no messages)') }}</div>
            <div v-else class="terminal-output">
              <template v-for="item in logEntries" :key="item.scopeKey">
                <span class="terminal-line">{{ formatLogLine(item) }}</span>
              </template>
            </div>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- CATALOG MAINTENANCE -->
      <q-tab-panel name="catalog" class="q-pa-none">
        <div class="row q-col-gutter-md">
          <div class="col-12" v-if="catalogAclError">
            <q-banner dense rounded class="bg-negative text-white">
              {{ t('Could not determine catalog-maintenance permissions') }}: {{ catalogAclError }}
            </q-banner>
          </div>

          <div class="col-12" v-else-if="catalogAclNotice">
            <q-banner dense rounded class="bg-grey-3 text-grey-9">
              <template #avatar>
                <q-icon name="lock" color="warning" />
              </template>
              {{ t('Catalog maintenance actions are disabled for this console because') }}
              {{ t('the required Command ACL does not allow') }}
              {{ catalogAclNotice }}.
            </q-banner>
          </div>

          <!-- Empty Jobs -->
          <div class="col-12">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header row items-center">
                <span>{{ t('Jobs With No Data') }}</span>
                <q-space />
                <q-btn flat round dense icon="refresh" color="white"
                       @click="loadEmptyJobs" :loading="emptyJobsLoading" />
              </q-card-section>
              <q-card-section class="q-pa-none">
                <q-inner-loading :showing="emptyJobsLoading" />
                <div v-if="emptyJobsError" class="text-negative q-pa-md">{{ emptyJobsError }}</div>
                <template v-else>
                  <div class="q-pa-sm row items-center q-gutter-sm">
                    <span class="text-caption text-grey-6">
                      {{ t('{count} job(s) with 0 files and 0 bytes found', { count: emptyJobs.length }) }}
                    </span>
                    <q-space />
                    <span v-if="!canDeleteEmptyJobs" class="text-caption text-grey-6">
                      <span v-html="t('Delete is unavailable because the ACL forbids the <code>delete</code> command.')" />
                    </span>
                    <q-btn
                      color="negative" size="sm" no-caps unelevated
                      :label="deleteSelectedLabel"
                      icon="delete"
                      :loading="deleteLoading"
                      :disable="deleteActionDisabled"
                      @click="deleteSelected"
                    />
                  </div>
                  <q-table
                    :rows="emptyJobs"
                    :columns="emptyJobCols"
                    row-key="id"
                    dense flat
                    selection="multiple"
                    v-model:selected="selectedEmptyJobs"
                    :pagination="{ rowsPerPage: 20 }"
                    :no-data-label="t('No empty jobs found')"
                  >
                    <template #body-cell-id="props">
                      <q-td :props="props">
                        <router-link
                          :to="{ name: 'job-details', params: { id: props.value } }"
                          class="text-primary"
                        >{{ props.value }}</router-link>
                      </q-td>
                    </template>
                    <template #body-cell-status="props">
                      <q-td :props="props">
                        <JobStatusBadge :status="props.value" />
                      </q-td>
                    </template>
                  </q-table>
                  <div v-if="deleteResult" class="q-pa-sm">
                    <q-banner
                      :class="deleteResult.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
                      dense rounded
                    >{{ deleteResult.msg }}</q-banner>
                  </div>
                </template>
              </q-card-section>
            </q-card>
          </div>

          <!-- Prune -->
          <div class="col-12 col-md-6">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header">{{ t('Prune Expired Records') }}</q-card-section>
              <q-card-section>
                <p class="text-body2 q-mb-md">
                  {{ t('Remove catalog records that have exceeded their retention period.') }}
                  {{ t('This does not delete any data from storage volumes.') }}
                </p>
                <div class="row q-gutter-sm">
                    <q-btn
                      v-for="prune in pruneActions" :key="prune.cmd"
                      :label="prune.label" :icon="prune.icon"
                      color="warning" text-color="black"
                      size="sm" no-caps unelevated
                      :loading="pruneLoading[prune.cmd]"
                      :disable="!canPruneCatalog"
                      @click="runPrune(prune.cmd)"
                    />
                  </div>
                <div v-if="!canPruneCatalog" class="text-caption text-grey-6 q-mt-sm">
                  <span v-html="t('Prune is unavailable because the ACL forbids the <code>prune</code> command.')" />
                </div>
                <div v-if="pruneResults.length" class="q-mt-md column q-gutter-xs">
                  <q-banner
                    v-for="(r, i) in pruneResults" :key="i"
                    :class="r.ok ? 'bg-positive text-white' : 'bg-negative text-white'"
                    dense rounded
                  >{{ r.msg }}</q-banner>
                </div>
              </q-card-section>
            </q-card>
          </div>

        </div>
      </q-tab-panel>

      <!-- SUBSCRIPTION -->
      <q-tab-panel name="subscription" class="q-pa-none">
        <q-card flat bordered class="bareos-panel" style="max-width:800px">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Subscription') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white"
                   @click="refreshSubscription" :loading="subscriptionLoading" />
          </q-card-section>
          <q-card-section>
            <q-inner-loading :showing="subscriptionLoading" />
            <div v-if="subscriptionError" class="text-negative">{{ subscriptionError }}</div>
            <template v-else-if="subscriptionData">
              <SubscriptionReport :data="subscriptionData" />

              <!-- Download buttons: 2×2 grid (normal / anonymized) × (PDF / JSON) -->
              <div class="row q-gutter-sm q-mb-md q-mt-md">
                <q-btn color="primary" :label="t('Download PDF')" icon="picture_as_pdf"
                       no-caps :loading="pdfLoading"
                       @click="printSubscription(false)" />
                <q-btn color="secondary" :label="t('Download JSON')" icon="download"
                       no-caps :loading="downloadLoading"
                       @click="downloadSubscription(false)" />
                <q-btn color="primary" :label="t('Download Anonymized PDF')"
                       icon="picture_as_pdf"
                       no-caps :loading="pdfAnonLoading"
                       @click="printSubscription(true)" />
                <q-btn color="secondary" :label="t('Download Anonymized JSON')"
                       icon="download"
                       no-caps :loading="downloadAnonLoading"
                       @click="downloadSubscription(true)" />
              </div>

               <q-btn color="primary" :label="t('Get Official Support')" icon="open_in_new"
                     href="https://www.bareos.com/subscription/" target="_blank" no-caps />
            </template>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>

  <!-- Config status dialog -->
  <q-dialog v-model="configStatusDlg.open">
    <q-card style="min-width:640px; max-width:90vw">
      <q-card-section class="panel-header row items-center q-py-sm">
        <span>{{ t('Configuration Status') }}</span>
        <q-space />
        <q-btn flat round dense icon="close" color="white" v-close-popup />
      </q-card-section>
      <q-card-section class="q-pa-none">
        <q-inner-loading :showing="configStatusDlg.loading" />
        <pre v-if="configStatusDlg.text" class="config-status-output">{{ configStatusDlg.text }}</pre>
        <div v-else-if="!configStatusDlg.loading" class="text-grey q-pa-md text-center">{{ t('No output') }}</div>
      </q-card-section>
    </q-card>
  </q-dialog>

  <!-- Hidden print root — rendered into DOM, visible only during window.print() -->
  <teleport to="body">
    <div id="subscription-print-root" style="display:none">
      <SubscriptionReport v-if="printData" :data="printData" :anonymized="printAnonymized" />
    </div>
  </teleport>
</template>

<script setup>
import { ref, computed, watch, nextTick, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useDirectorAclStore } from '../stores/directorAcl.js'
import { useSettingsStore } from '../stores/settings.js'
import {
  directorCollection,
  directorCommandAllowed,
  missingDirectorCommands,
  normaliseJob,
} from '../composables/useDirectorFetch.js'
import {
  fetchAggregatedDirectorMessages,
  fetchAggregatedDirectorStatus,
  normaliseDirectorStatusSnapshot,
} from '../composables/directorPageAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { formatBytes } from '../mock/index.js'
import {
  formatDirectorRelativeTime,
  formatNumber,
} from '../utils/locales.js'
import { resolveOsIcon } from '../utils/osIcon.js'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge  from '../components/JobLevelBadge.vue'
import JobTypeBadge   from '../components/JobTypeBadge.vue'
import SubscriptionReport from '../components/SubscriptionReport.vue'

const tab = ref('status')
const router = useRouter()
const $q = useQuasar()
const auth = useAuthStore()
const director = useDirectorStore()
const acl = useDirectorAclStore()
const settings  = useSettingsStore()
const { t } = useI18n()

const directorOptions = computed(() => {
  const values = new Set([
    ...director.availableDirectors,
    ...settings.selectedDirectors,
    auth.user?.director,
    settings.directorName,
  ].filter(Boolean))
  return [...values].map(value => ({ label: value, value }))
})

function syncSelectedDirectors() {
  const validDirectors = directorOptions.value.map(option => option.value)
  const selected = settings.selectedDirectors.filter(value => validDirectors.includes(value))

  if (selected.length > 0) {
    if (selected.length !== settings.selectedDirectors.length) {
      settings.setSelectedDirectors(selected)
    }
    return
  }

  const fallbackDirector = auth.user?.director || settings.directorName
  if (fallbackDirector) {
    settings.setSelectedDirectors([fallbackDirector])
  }
}

const selectedDirectorsModel = computed({
  get: () => settings.selectedDirectors,
  set: (value) => {
    const selected = Array.isArray(value) ? value : []
    if (selected.length > 0) {
      settings.setSelectedDirectors(selected)
      return
    }

    const fallbackDirector = auth.user?.director || settings.directorName
    settings.setSelectedDirectors(fallbackDirector ? [fallbackDirector] : [])
  },
})

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    directorOptions.value.some(option => option.value === value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isSingleDirectorScope = computed(() => activeDirectors.value.length === 1)
const isCommonDirectorPage = computed(() => activeDirectors.value.length > 1)
const showDirectorColumn = computed(() => isCommonDirectorPage.value)
const directorScopeLabel = computed(() => (
  isCommonDirectorPage.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))

async function ensureSingleScopeDirector() {
  if (!isSingleDirectorScope.value) {
    return
  }

  const scopeDirector = activeDirectors.value[0]
  if (!scopeDirector) {
    return
  }

  if (auth.user?.director === scopeDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(scopeDirector)
}

// ── Status ───────────────────────────────────────────────────────────────────
const statusLoading = ref(false)
const statusError   = ref(null)
const statusSnapshots = ref([])
const statusDirectorErrors = ref([])

async function refreshStatus() {
  statusLoading.value = true
  statusError.value = null
  statusDirectorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      statusSnapshots.value = []
      return
    }

    if (isCommonDirectorPage.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedDirectorStatus(credentials, activeDirectors.value)
      statusSnapshots.value = result.snapshots
      statusDirectorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const result = await director.call('status director')
    statusSnapshots.value = [normaliseDirectorStatusSnapshot(result, currentDirector)]
  } catch (e) {
    statusError.value = e.message
  } finally {
    statusLoading.value = false
  }
}

const statusCountdown = ref(settings.refreshInterval)
let _statusTimer = null

function startStatusAutoRefresh() {
  clearInterval(_statusTimer)
  statusCountdown.value = settings.refreshInterval
  _statusTimer = setInterval(() => {
    statusCountdown.value -= 1
    if (statusCountdown.value <= 0) {
      refreshStatus()
      statusCountdown.value = settings.refreshInterval
    }
  }, 1000)
}

function manualRefreshStatus() {
  refreshStatus()
  statusCountdown.value = settings.refreshInterval
}

function resolveDirectorOsChip(full = '') {
  const lower = full.toLowerCase()
  if (lower.includes('windows') || lower.includes('win'))
    return resolveOsIcon({ os: 'windows', osInfo: full })
  if (lower.includes('darwin') || lower.includes('apple') || lower.includes('macos'))
    return resolveOsIcon({ os: 'macos', osInfo: full })
  return resolveOsIcon({ os: 'linux', osInfo: full })
}

const statusCards = computed(() => statusSnapshots.value.map(({ director, header }) => ({
  ...header,
  director: header?.director || director,
  osIcon: resolveDirectorOsChip(header?.os ?? ''),
  scopeDirector: director,
})))
const scheduledJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.scheduledJobs))
const runningJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.runningJobs))
const terminatedJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.terminatedJobs))

const maxTermBytes = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.bytes) || 0)))
const maxTermFiles = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.files) || 0)))

// Relative-time toggle handled by global settings store

// Map full-word level/type strings emitted by the director to single-letter
// codes expected by JobLevelBadge / JobTypeBadge.
// The director truncates levels differently per section:
//   scheduled: full string ("Incremental", "Differential")
//   running:   level[7]=0  ("Incremen", "Differen")
//   terminated: level[4]=0 ("Incr", "Diff", "VFul")
const LEVEL_CODE = {
  Full: 'F', Incremental: 'I', Differential: 'D', 'Virtual Full': 'V', Base: 'B',
  Incremen: 'I', Differen: 'D',
  Incr: 'I', Diff: 'D', VFul: 'V',
}
const TYPE_CODE  = { Backup: 'B', Restore: 'R', Verify: 'V', Admin: 'A', Diagnostic: 'D', Copy: 'C', Migration: 'M' }
function levelCode(v) { return LEVEL_CODE[v] ?? null }
function typeCode(v)  { return TYPE_CODE[v]  ?? null }
function isWaiting(status) { return typeof status === 'string' && status.includes('is waiting') }

const scheduledJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left' }]
    : []),
  { name: 'name',      label: 'Job',       field: 'name',      align: 'left' },
  { name: 'level',     label: 'Level',     field: 'level',     align: 'left' },
  { name: 'type',      label: 'Type',      field: 'type',      align: 'left' },
  { name: 'priority',  label: 'Pri',       field: 'priority',  align: 'right' },
  { name: 'scheduled', label: 'Scheduled', field: 'scheduled', align: 'left' },
  { name: 'volume',    label: 'Volume',    field: 'volume',    align: 'left' },
  { name: 'pool',      label: 'Pool',      field: 'pool',      align: 'left' },
  { name: 'storage',   label: 'Storage',   field: 'storage',   align: 'left' },
].map((col) => ({ ...col, label: t(col.label) })))
const runningJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left' }]
    : []),
  { name: 'jobid',      label: 'JobId',      field: 'jobid',      align: 'right' },
  { name: 'name',       label: 'Job',        field: 'name',       align: 'left' },
  { name: 'level',      label: 'Level',      field: 'level',      align: 'left' },
  { name: 'type',       label: 'Type',       field: 'type',       align: 'left' },
  { name: 'start_time', label: 'Started',    field: 'start_time', align: 'left' },
  { name: 'files',      label: 'Files',      field: 'files',      align: 'right' },
  { name: 'bytes',      label: 'Bytes',      field: 'bytes',      align: 'right' },
  { name: 'status',     label: 'Status',     field: 'status',     align: 'left' },
].map((col) => ({ ...col, label: t(col.label) })))
const terminatedJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left' }]
    : []),
  { name: 'jobid',    label: 'JobId',    field: 'jobid',    align: 'right' },
  { name: 'name',     label: 'Job',      field: 'name',     align: 'left' },
  { name: 'level',    label: 'Level',    field: 'level',    align: 'left' },
  { name: 'type',     label: 'Type',     field: 'type',     align: 'left' },
  { name: 'finished', label: 'Finished', field: 'finished', align: 'left' },
  { name: 'files',    label: 'Files',    field: 'files',    align: 'right' },
  { name: 'bytes',    label: 'Bytes',    field: 'bytes',    align: 'right' },
  { name: 'status',   label: 'Status',   field: 'status',   align: 'left' },
].map((col) => ({ ...col, label: t(col.label) })))

// ── Messages ─────────────────────────────────────────────────────────────────
const messagesLoading = ref(false)
const messagesError   = ref(null)
const messagesLimit   = ref(100)
const logEntries      = ref([])
const messagesDirectorErrors = ref([])

async function refreshMessages() {
  messagesLoading.value = true
  messagesError.value = null
  messagesDirectorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      logEntries.value = []
      return
    }

    if (isCommonDirectorPage.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedDirectorMessages(
        credentials,
        activeDirectors.value,
        messagesLimit.value
      )
      logEntries.value = result.logEntries
      messagesDirectorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const data = await director.call(`list log limit=${messagesLimit.value}`)
    logEntries.value = directorCollection(data?.log).map((entry, index) => ({
      ...entry,
      director: currentDirector,
      scopeKey: `${currentDirector}:log:${entry.logid ?? index}`,
    }))
  } catch (e) {
    messagesError.value = e.message
  } finally {
    messagesLoading.value = false
  }
}

function formatLogLine(item) {
  const prefix = isCommonDirectorPage.value ? `${item.director} · ` : ''
  return `${prefix}${item.time} ${item.logtext}`
}

watch(messagesLimit, () => refreshMessages())

watch(tab, (t) => {
  if (isCommonDirectorPage.value && ['catalog', 'subscription'].includes(t)) {
    tab.value = 'status'
    return
  }
  if (t === 'messages') refreshMessages()
  if (t === 'catalog' && isSingleDirectorScope.value) {
    acl.ensureLoaded()
    loadEmptyJobs()
  }
  if (t === 'subscription' && isSingleDirectorScope.value) refreshSubscription()
})

async function navigateForDirector(targetDirector, location) {
  try {
    if (targetDirector) {
      await switchActiveDirector(targetDirector)
    }
    await router.push(location)
  } catch (e) {
    $q.notify({
      type: 'negative',
      message: e?.message ?? String(e),
    })
  }
}

function openJobDetails(row) {
  return navigateForDirector(row.director, {
    name: 'job-details',
    params: { id: row.jobid ?? row.id },
  })
}

function openJobsByName(row) {
  return navigateForDirector(row.director, {
    name: 'jobs',
    query: { name: row.name },
  })
}

function openVolumeDetails(row) {
  if (!row.volume) {
    return
  }

  return navigateForDirector(row.director, {
    name: 'volume-details',
    params: { name: row.volume },
  })
}

async function loadAvailableDirectors() {
  try {
    await director.fetchAvailableDirectors()
  } catch {
    // Keep the selector usable with the active director when the proxy list is
    // unavailable.
  }
}

onMounted(() => {
  loadAvailableDirectors()
  syncSelectedDirectors()
  refreshStatus()
  refreshMessages()
  if (isSingleDirectorScope.value) {
    acl.ensureLoaded()
  }
  startStatusAutoRefresh()
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  if (isCommonDirectorPage.value && ['catalog', 'subscription'].includes(tab.value)) {
    tab.value = 'status'
  }
  refreshStatus()
  refreshMessages()
  if (isSingleDirectorScope.value && tab.value === 'catalog') {
    acl.refresh()
    loadEmptyJobs()
  }
  if (isSingleDirectorScope.value && tab.value === 'subscription') {
    refreshSubscription()
  }
})

watch(() => director.isConnected, (connected) => {
  if (!connected || !isSingleDirectorScope.value) {
    return
  }

  refreshStatus()
  if (tab.value === 'messages') refreshMessages()
  if (tab.value === 'catalog') {
    acl.refresh()
    loadEmptyJobs()
  }
  if (tab.value === 'subscription') refreshSubscription()
})

onUnmounted(() => { clearInterval(_statusTimer) })

// ── Subscription ──────────────────────────────────────────────────────────────
const subscriptionLoading = ref(false)
const subscriptionError   = ref(null)
const subscriptionData    = ref(null)

async function refreshSubscription() {
  subscriptionLoading.value = true
  subscriptionError.value   = null
  try {
    subscriptionData.value = await director.call('status subscriptions all')
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    subscriptionLoading.value = false
  }
}

const downloadLoading     = ref(false)
const downloadAnonLoading = ref(false)
const pdfLoading          = ref(false)
const pdfAnonLoading      = ref(false)

// Print state: when set, the hidden #subscription-print-root becomes populated
// and window.print() renders it to PDF via @media print CSS.
const printData       = ref(null)
const printAnonymized = ref(false)

async function printSubscription(anonymize) {
  const loadingRef = anonymize ? pdfAnonLoading : pdfLoading
  loadingRef.value = true
  try {
    const cmd = anonymize
      ? 'status subscriptions all anonymize'
      : 'status subscriptions all'
    printData.value       = await director.call(cmd)
    printAnonymized.value = anonymize
    await nextTick()
    window.print()
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    printData.value  = null
    loadingRef.value = false
  }
}

async function downloadSubscription(anonymize) {
  const loadingRef = anonymize ? downloadAnonLoading : downloadLoading
  loadingRef.value = true
  try {
    const cmd  = anonymize ? 'status subscriptions all anonymize' : 'status subscriptions all'
    const data = await director.call(cmd)
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
    const url  = URL.createObjectURL(blob)
    const a    = document.createElement('a')
    a.href     = url
    a.download = anonymize ? 'bareos-subscription-anonymized.json' : 'bareos-subscription.json'
    a.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    subscriptionError.value = e.message
  } finally {
    loadingRef.value = false
  }
}

// ── Catalog Maintenance ───────────────────────────────────────────────────────
const emptyJobsLoading  = ref(false)
const emptyJobsError    = ref(null)
const emptyJobs         = ref([])
const selectedEmptyJobs = ref([])
const deleteLoading     = ref(false)
const deleteResult      = ref(null)
const canDeleteEmptyJobs = computed(() => (
  directorCommandAllowed(acl.commands, 'delete')
))
const deleteSelectedLabel = computed(() => (
  selectedEmptyJobs.value.length
    ? t('Delete {count} selected', { count: selectedEmptyJobs.value.length })
    : t('Delete selected')
))
const deleteActionDisabled = computed(() => (
  !canDeleteEmptyJobs.value
  || !selectedEmptyJobs.value.length
))
const canPruneCatalog = computed(() => (
  directorCommandAllowed(acl.commands, 'prune')
))
const catalogAclError = computed(() => acl.error)
const catalogAclNotice = computed(() => {
  if (!acl.commands || acl.error) return null
  const missing = missingDirectorCommands(
    acl.commands,
    ['delete', 'prune']
  )
  if (!missing.length) return null
  return missing.map(cmd => `"${cmd}"`).join(' and ')
})

const emptyJobCols = computed(() => [
  { name: 'id',        label: 'ID',       field: 'id',
    align: 'right', sortable: true },
  { name: 'name',      label: 'Job Name', field: 'name',
    align: 'left',  sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',
    align: 'left',  sortable: true },
  { name: 'type',      label: 'Type',     field: 'type',
    align: 'center' },
  { name: 'starttime', label: 'Start',    field: 'starttime',
    align: 'left',  sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',
    align: 'center' },
].map((col) => ({ ...col, label: t(col.label) })))

async function loadEmptyJobs() {
  emptyJobsLoading.value  = true
  emptyJobsError.value    = null
  deleteResult.value      = null
  selectedEmptyJobs.value = []
  try {
    const res  = await director.call('list jobs')
    const jobs = directorCollection(res?.jobs).map(normaliseJob)
    emptyJobs.value = jobs.filter(j =>
      !Number(j.bytes) && !Number(j.files) && j.status !== 'R'
    )
  } catch (e) {
    emptyJobsError.value = e.message
  } finally {
    emptyJobsLoading.value = false
  }
}

async function deleteSelected() {
  if (deleteActionDisabled.value) return
  deleteLoading.value = true
  deleteResult.value  = null
  const ids = selectedEmptyJobs.value.map(j => j.id).join(',')
  try {
    await director.call(`delete jobid=${ids}`)
    deleteResult.value = { ok: true, msg: t('Deleted job(s) {ids}.', { ids }) }
    const deleted = new Set(selectedEmptyJobs.value.map(j => j.id))
    emptyJobs.value         = emptyJobs.value.filter(j => !deleted.has(j.id))
    selectedEmptyJobs.value = []
  } catch (e) {
    deleteResult.value = { ok: false, msg: e.message }
  } finally {
    deleteLoading.value = false
  }
}

// ── Prune ─────────────────────────────────────────────────────────────────────
const pruneActions = computed(() => [
  { cmd: 'prune jobs yes',   label: t('Prune Jobs'),
    icon: 'work_off' },
  { cmd: 'prune files yes',  label: t('Prune File Records'),
    icon: 'folder_off' },
  { cmd: 'prune stats yes',  label: t('Prune Statistics'),
    icon: 'bar_chart_off' },
])
const pruneLoading = ref(
  Object.fromEntries(pruneActions.value.map(p => [p.cmd, false]))
)
const pruneResults = ref([])

async function runPrune(cmd) {
  if (!canPruneCatalog.value) return
  pruneLoading.value[cmd] = true
  try {
    await director.call(cmd)
    pruneResults.value.push({ ok: true,
      msg: t('"{cmd}" completed.', { cmd }) })
  } catch (e) {
    pruneResults.value.push({ ok: false,
      msg: `${cmd}: ${e.message}` })
  } finally {
    pruneLoading.value[cmd] = false
  }
}

// ── Config status dialog ──────────────────────────────────────────────────────

const configStatusDlg = ref({ open: false, loading: false, text: '' })

async function showConfigStatus(targetDirector = activeDirectors.value[0]) {
  configStatusDlg.value = { open: true, loading: true, text: '' }
  try {
    if (targetDirector && (!director.isConnected || auth.user?.director !== targetDirector)) {
      await switchActiveDirector(targetDirector)
    } else {
      await ensureSingleScopeDirector()
    }
    configStatusDlg.value.text = await director.rawCall('status configuration')
  } catch (e) {
    configStatusDlg.value.text = `${t('Error')}: ${e.message ?? e}`
  } finally {
    configStatusDlg.value.loading = false
  }
}
</script>

<style scoped>
.config-status-output {
  background: #1e1e1e;
  color: #d4d4d4;
  font-family: monospace;
  font-size: 0.82rem;
  line-height: 1.5;
  padding: 12px 16px;
  margin: 0;
  white-space: pre-wrap;
  max-height: 70vh;
  overflow-y: auto;
}
.terminal-output {
  background: #0d1117;
  color: #c9d1d9;
  font-family: 'Fira Mono', 'Cascadia Code', 'Consolas', 'DejaVu Sans Mono', monospace;
  font-size: 0.8rem;
  line-height: 1.5;
  padding: 12px 16px;
  max-height: 600px;
  overflow-y: auto;
  white-space: pre-wrap;
  word-break: break-all;
  display: flex;
  flex-direction: column;
}
.terminal-line {
  display: block;
  border-bottom: 1px solid #21262d;
  padding: 1px 0;
}
.terminal-line:last-child {
  border-bottom: none;
}
.animated-spin {
  animation: hourglass-spin 1.4s ease-in-out infinite;
}
@keyframes hourglass-spin {
  0%   { transform: rotate(0deg);   }
  45%  { transform: rotate(0deg);   }
  55%  { transform: rotate(180deg); }
  100% { transform: rotate(180deg); }
}

@media print {
  /* Hide everything, then reveal only the print root */
  body * {
    visibility: hidden;
  }
  #subscription-print-root,
  #subscription-print-root * {
    visibility: visible;
  }

  /* Position at top-left so content flows naturally across pages */
  #subscription-print-root {
    display: block !important;
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    overflow: visible !important;
  }

  /* Allow tables to break across pages */
  table {
    page-break-inside: auto;
  }
  tr {
    page-break-inside: avoid;
  }
}
</style>
