<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status"       :label="t('Status')"       no-caps />
      <q-tab name="messages"     :label="t('Messages')"     no-caps />
      <q-tab name="catalog"      :label="t('Catalog Maintenance')" no-caps />
      <q-tab name="subscription" :label="t('Subscription')" no-caps />
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
          <div
            v-for="item in statusDirectorErrors"
            :key="item.director"
            class="row items-center q-gutter-xs"
          >
            <DirectorBadge :director="item.director" size="sm" />
            <span>{{ item.message }}</span>
          </div>
        </q-banner>
        <q-inner-loading :showing="statusLoading" />
        <div v-if="statusError" class="text-negative q-pa-md">{{ statusError }}</div>
        <template v-else-if="statusCards.length">
          <div class="row q-col-gutter-md">

            <div class="col-12">
              <DirectorInfoCard
                :status-cards="statusCards"
                :status-countdown="statusCountdown"
                :status-loading="statusLoading"
                :locale="settings.locale"
                :relative-time="settings.relativeTime"
                @refresh="manualRefreshStatus"
                @show-config-status="showConfigStatus"
              />
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
                        <DirectorLabel :director="props.row.director || props.value || ''" />
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
                        <DirectorLabel :director="props.row.director || props.value || ''" />
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
                        <DirectorLabel :director="props.row.director || props.value || ''" />
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
              <div
                v-for="item in messagesDirectorErrors"
                :key="item.director"
                class="row items-center q-gutter-xs"
              >
                <DirectorBadge :director="item.director" size="sm" />
                <span>{{ item.message }}</span>
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
        <q-card
          v-if="isCommonDirectorPage"
          flat
          bordered
          class="q-mb-md bareos-panel"
          style="max-width:800px"
        >
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Catalog Maintenance Target') }}</span>
          </q-card-section>
          <q-card-section>
             <q-select
               v-model="singletonTabDirector"
               :options="singletonTabDirectorOptions"
               option-label="label"
               option-value="value"
              emit-value
              map-options
               outlined
               dense
               :label="t('Director')"
             >
               <template #selected-item="scope">
                 <DirectorLabel :director="scope.opt?.value || scope.opt?.label || ''" />
               </template>
               <template #option="scope">
                 <q-item v-bind="scope.itemProps">
                   <q-item-section>
                     <DirectorLabel :director="scope.opt?.value || scope.opt?.label || ''" />
                   </q-item-section>
                 </q-item>
               </template>
             </q-select>
            <div class="text-caption text-grey-6 q-mt-sm">
              {{ t('Catalog maintenance actions run against the selected director while the status and message views stay aggregated.') }}
            </div>
          </q-card-section>
        </q-card>
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
                      {{ emptyJobsSummaryLabel }}
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
                          :to="{
                            name: 'job-details',
                            params: { id: props.value },
                            query: buildJobDetailsQuery({
                              director: currentSingletonDirector,
                              directorTab: tab,
                              directorTarget: isDirectorSingletonTab(tab) ? currentSingletonDirector : '',
                            }),
                          }"
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
            <div v-if="isCommonDirectorPage" class="q-mb-md">
              <q-select
                v-model="singletonTabDirector"
                :options="singletonTabDirectorOptions"
                option-label="label"
                option-value="value"
                emit-value
                map-options
                outlined
                dense
                :label="t('Director')"
              >
                <template #selected-item="scope">
                  <DirectorLabel :director="scope.opt?.value || scope.opt?.label || ''" />
                </template>
                <template #option="scope">
                  <q-item v-bind="scope.itemProps">
                    <q-item-section>
                      <DirectorLabel :director="scope.opt?.value || scope.opt?.label || ''" />
                    </q-item-section>
                  </q-item>
                </template>
              </q-select>
              <div class="text-caption text-grey-6 q-mt-sm">
                {{ t('Subscription data and downloads run against the selected director while the status and message views stay aggregated.') }}
              </div>
            </div>
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
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useDirectorAclStore } from '../stores/directorAcl.js'
import { useSettingsStore } from '../stores/settings.js'
import { translate } from '../i18n/index.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import {
  directorCollection,
  directorCommandAllowed,
  missingDirectorCommands,
  normaliseJob,
} from '../composables/useDirectorFetch.js'
import {
  fetchAggregatedDirectorConfigStatusAccess,
  fetchAggregatedDirectorMessages,
  fetchAggregatedDirectorStatus,
  normaliseDirectorStatusSnapshot,
} from '../composables/directorPageAggregate.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { formatBytes } from '../mock/index.js'
import {
  buildDirectorPageQuery,
  isDirectorSingletonTab,
  resolveDirectorTargetQuery,
} from '../utils/director.js'
import {
  buildJobDetailsQuery,
  withJobsSearchQuery,
} from '../utils/jobs.js'
import { buildVolumeDetailsQuery } from '../utils/volumes.js'
import {
  formatDirectorRelativeTime,
  formatNumber,
} from '../utils/locales.js'
import { buildDirectorStatusCards } from '../utils/directorStatus.js'
import { resolveJobLevelCode } from '../utils/jobLevels.js'
import { resolveJobTypeCode } from '../utils/jobTypes.js'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorInfoCard from '../components/DirectorInfoCard.vue'
import DirectorLabel from '../components/DirectorLabel.vue'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge  from '../components/JobLevelBadge.vue'
import JobTypeBadge   from '../components/JobTypeBadge.vue'
import SubscriptionReport from '../components/SubscriptionReport.vue'

const validTabs = new Set(['status', 'messages', 'catalog', 'subscription'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'status'
}

const route = useRoute()
const tab = ref(normaliseTab(route.query.tab))
const router = useRouter()
const $q = useQuasar()
const auth = useAuthStore()
const director = useDirectorStore()
const acl = useDirectorAclStore()
const settings  = useSettingsStore()
const { t } = useI18n()
const tr = (msgid, values) => translate(settings.locale, msgid, values)

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonDirectorPage,
  isSingleDirectorScope,
  scopeLabel: directorScopeLabel,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

const showDirectorColumn = computed(() => isCommonDirectorPage.value)
const singletonTabDirector = ref('')
const singletonTabDirectorOptions = computed(() => (
  activeDirectors.value.map(value => ({ label: value, value }))
))
const currentSingletonDirector = computed(() => (
  isCommonDirectorPage.value
    ? (singletonTabDirector.value || activeDirectors.value[0] || '')
    : (activeDirectors.value[0] || '')
))

function syncSingletonTabDirector() {
  const validDirectors = activeDirectors.value
  if (!validDirectors.length) {
    singletonTabDirector.value = ''
    return
  }

  const requestedDirector = isDirectorSingletonTab(tab.value)
    ? resolveDirectorTargetQuery(route.query)
    : ''
  if (requestedDirector && validDirectors.includes(requestedDirector)) {
    singletonTabDirector.value = requestedDirector
    return
  }

  if (validDirectors.includes(singletonTabDirector.value)) {
    return
  }

  singletonTabDirector.value = validDirectors[0]
}

async function ensureSingletonTabDirector() {
  const targetDirector = currentSingletonDirector.value
  if (!targetDirector) {
    return null
  }

  await ensureScopeDirector(targetDirector)
  return targetDirector
}

// ── Status ───────────────────────────────────────────────────────────────────
const statusLoading = ref(false)
const statusError   = ref(null)
const statusSnapshots = ref([])
const statusDirectorErrors = ref([])
const configStatusAccessByDirector = ref({})

function mapConfigStatusAccess(entries) {
  return Object.fromEntries(entries.map(entry => [
    entry.director,
    {
      available: entry.available === true,
      message: entry.message ?? '',
    },
  ]))
}

async function refreshStatus() {
  statusLoading.value = true
  statusError.value = null
  statusDirectorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      statusSnapshots.value = []
      configStatusAccessByDirector.value = {}
      return
    }

    if (isCommonDirectorPage.value) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const [result, configStatusAccess] = await Promise.all([
        fetchAggregatedDirectorStatus(credentials, activeDirectors.value),
        fetchAggregatedDirectorConfigStatusAccess(credentials, activeDirectors.value),
      ])
      statusSnapshots.value = result.snapshots
      statusDirectorErrors.value = result.directorErrors
      configStatusAccessByDirector.value = mapConfigStatusAccess(configStatusAccess)
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const credentials = auth.getCredentials()
    if (!credentials?.password) {
      throw new Error(t('Not logged in.'))
    }
    const [result, configStatusAccess] = await Promise.all([
      director.call('status director'),
      fetchAggregatedDirectorConfigStatusAccess(credentials, [currentDirector]),
    ])
    statusSnapshots.value = [normaliseDirectorStatusSnapshot(result, currentDirector)]
    configStatusAccessByDirector.value = mapConfigStatusAccess(configStatusAccess)
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

const statusCards = computed(() => (
  buildDirectorStatusCards(
    statusSnapshots.value,
    configStatusAccessByDirector.value
  )
))
const scheduledJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.scheduledJobs))
const runningJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.runningJobs))
const terminatedJobs = computed(() => statusSnapshots.value.flatMap(snapshot => snapshot.terminatedJobs))

const maxTermBytes = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.bytes) || 0)))
const maxTermFiles = computed(() => Math.max(1, ...terminatedJobs.value.map(j => Number(j.files) || 0)))

// Relative-time toggle handled by global settings store

// Map full-word level/type strings emitted by the director to single-letter
function levelCode(v) { return resolveJobLevelCode(v) }
function typeCode(v)  { return resolveJobTypeCode(v) }
function isWaiting(status) { return typeof status === 'string' && status.includes('is waiting') }

const scheduledJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true }]
    : []),
  { name: 'name',      label: 'Job',       field: 'name',      align: 'left', sortable: true },
  { name: 'level',     label: 'Level',     field: 'level',     align: 'left', sortable: true },
  { name: 'type',      label: 'Type',      field: 'type',      align: 'left', sortable: true },
  { name: 'priority',  label: 'Pri',       field: 'priority',  align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
  { name: 'scheduled', label: 'Scheduled', field: 'scheduled', align: 'left', sortable: true },
  { name: 'volume',    label: 'Volume',    field: 'volume',    align: 'left', sortable: true },
  { name: 'pool',      label: 'Pool',      field: 'pool',      align: 'left', sortable: true },
  { name: 'storage',   label: 'Storage',   field: 'storage',   align: 'left', sortable: true },
].map((col) => ({ ...col, label: t(col.label) })))
const runningJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true }]
    : []),
  { name: 'jobid',      label: 'JobId',      field: 'jobid',      align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
  { name: 'name',       label: 'Job',        field: 'name',       align: 'left', sortable: true },
  { name: 'level',      label: 'Level',      field: 'level',      align: 'left', sortable: true },
  { name: 'status',     label: 'Status',     field: 'status',     align: 'left', sortable: true },
  { name: 'type',       label: 'Type',       field: 'type',       align: 'left', sortable: true },
  { name: 'start_time', label: 'Started',    field: 'start_time', align: 'left', sortable: true },
  { name: 'files',      label: 'Files',      field: 'files',      align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
  { name: 'bytes',      label: 'Bytes',      field: 'bytes',      align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
].map((col) => ({ ...col, label: t(col.label) })))
const terminatedJobCols = computed(() => [
  ...(showDirectorColumn.value
    ? [{ name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true }]
    : []),
  { name: 'jobid',    label: 'JobId',    field: 'jobid',    align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
  { name: 'name',     label: 'Job',      field: 'name',     align: 'left', sortable: true },
  { name: 'level',    label: 'Level',    field: 'level',    align: 'left', sortable: true },
  { name: 'status',   label: 'Status',   field: 'status',   align: 'left', sortable: true },
  { name: 'type',     label: 'Type',     field: 'type',     align: 'left', sortable: true },
  { name: 'finished', label: 'Finished', field: 'finished', align: 'left', sortable: true },
  { name: 'files',    label: 'Files',    field: 'files',    align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
  { name: 'bytes',    label: 'Bytes',    field: 'bytes',    align: 'right', sortable: true,
    sort: (a, b) => Number(a ?? 0) - Number(b ?? 0) },
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

watch(() => route.query.tab, (value) => {
  const next = normaliseTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(() => route.query.directorTarget, () => {
  syncSingletonTabDirector()
})

watch(tab, (t) => {
  const current = normaliseTab(route.query.tab)
  const query = buildDirectorPageQuery(route.query, {
    tab: t,
    targetDirector: singletonTabDirector.value,
  })
  if (current !== t || route.query.directorTarget !== query.directorTarget) {
    router.replace({ path: '/director', query })
  }

  if (t === 'messages') refreshMessages()
  if (t === 'catalog') {
    ensureSingletonTabDirector()
      .then(() => acl.refresh())
      .then(() => loadEmptyJobs())
      .catch(() => {})
  }
  if (t === 'subscription') refreshSubscription()
})

watch(() => singletonTabDirector.value, () => {
  if (!isCommonDirectorPage.value) {
    return
  }

  const query = buildDirectorPageQuery(route.query, {
    tab: tab.value,
    targetDirector: singletonTabDirector.value,
  })
  if (route.query.tab !== query.tab || route.query.directorTarget !== query.directorTarget) {
    router.replace({ path: '/director', query })
  }

  if (tab.value === 'catalog') {
    ensureSingletonTabDirector()
      .then(() => acl.refresh())
      .then(() => loadEmptyJobs())
      .catch(() => {})
  }
  if (tab.value === 'subscription') {
    refreshSubscription()
  }
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  syncSingletonTabDirector()
  refreshStatus()
  refreshMessages()
  if (tab.value === 'catalog') {
    ensureSingletonTabDirector()
      .then(() => acl.refresh())
      .then(() => loadEmptyJobs())
      .catch(() => {})
  }
  if (tab.value === 'subscription') {
    refreshSubscription()
  }
})

watch(() => director.isConnected, (connected) => {
  if (!connected) {
    return
  }

  refreshStatus()
  if (tab.value === 'messages') refreshMessages()
  if (tab.value === 'catalog') {
    ensureSingletonTabDirector()
      .then(() => acl.refresh())
      .then(() => loadEmptyJobs())
      .catch(() => {})
  }
  if (tab.value === 'subscription') refreshSubscription()
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
  const query = buildJobDetailsQuery({
    director: row.director,
    directorTab: tab.value,
    directorTarget: isDirectorSingletonTab(tab.value) ? currentSingletonDirector.value : '',
  })
  return navigateForDirector(row.director, {
    name: 'job-details',
    params: { id: row.jobid ?? row.id },
    query,
  })
}

function openJobsByName(row) {
  const query = withJobsSearchQuery({}, row.name)

  return navigateForDirector(row.director, {
    name: 'jobs',
    query,
  })
}

function openVolumeDetails(row) {
  if (!row.volume) {
    return
  }

  const query = buildVolumeDetailsQuery({
    director: row.director,
    directorTab: tab.value,
    directorTarget: isDirectorSingletonTab(tab.value) ? currentSingletonDirector.value : '',
  })
  return navigateForDirector(row.director, {
    name: 'volume-details',
    params: { name: row.volume },
    query,
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
  syncSingletonTabDirector()
  refreshStatus()
  refreshMessages()
  acl.ensureLoaded()
  startStatusAutoRefresh()
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
  syncSingletonTabDirector()
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
    await ensureSingletonTabDirector()
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
    await ensureSingletonTabDirector()
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
    await ensureSingletonTabDirector()
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
    ? tr('Delete {count} selected', { count: selectedEmptyJobs.value.length })
    : t('Delete selected')
))
const emptyJobsSummaryLabel = computed(() => {
  const count = formatNumber(emptyJobs.value.length, settings.locale)
  return emptyJobs.value.length === 1
    ? tr('{count} job with 0 files and 0 bytes found', { count })
    : tr('{count} jobs with 0 files and 0 bytes found', { count })
})
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
    align: 'center', sortable: true },
  { name: 'starttime', label: 'Start',    field: 'starttime',
    align: 'left',  sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',
    align: 'center', sortable: true },
].map((col) => ({ ...col, label: t(col.label) })))

async function loadEmptyJobs() {
  emptyJobsLoading.value  = true
  emptyJobsError.value    = null
  deleteResult.value      = null
  selectedEmptyJobs.value = []
  try {
    const currentDirector = await ensureSingletonTabDirector()
    const res  = await director.call('list jobs')
    const jobs = directorCollection(res?.jobs).map(normaliseJob)
    emptyJobs.value = jobs
      .filter(j => !Number(j.bytes) && !Number(j.files) && j.status !== 'R')
      .map(j => ({ ...j, director: currentDirector || currentSingletonDirector.value || null }))
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
    await ensureSingletonTabDirector()
    await director.call(`delete jobid=${ids}`)
    deleteResult.value = { ok: true, msg: tr('Deleted job(s) {ids}.', { ids }) }
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
    icon: 'bar_chart' },
])
const pruneLoading = ref(
  Object.fromEntries(pruneActions.value.map(p => [p.cmd, false]))
)
const pruneResults = ref([])

async function runPrune(cmd) {
  if (!canPruneCatalog.value) return
  pruneLoading.value[cmd] = true
  try {
    await ensureSingletonTabDirector()
    await director.call(cmd)
    pruneResults.value.push({ ok: true,
      msg: tr('"{cmd}" completed.', { cmd }) })
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
