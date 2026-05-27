<template>
  <q-page class="q-pa-md">
    <DirectorScopePanel
      v-model="selectedDirectorsModel"
      :title="t('Jobs Scope')"
      :summary-label="jobsScopeLabel"
      :options="directorOptions"
      :help-text="t('Select the directors that contribute to the jobs list.')"
      :errors="directorErrors"
      data-test-id="jobs-directors"
    />

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     :label="t('Show')"     no-caps />
      <q-tab name="actions"  :label="t('Actions')"  no-caps data-testid="jobs-tab-actions" />
      <q-tab name="run"      :label="t('Run')"      no-caps data-testid="jobs-tab-run" />
      <q-tab name="rerun"    :label="t('Rerun')"    no-caps />
      <q-tab name="timeline" :label="t('Timeline')" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated swipeable>

      <!-- ── SHOW ─────────────────────────────────────────────────────────── -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header jobs-list-header row items-center no-wrap">
            <span class="jobs-list-header__title">{{ t('Job List') }}</span>
            <q-space />
            <q-select
              v-model="statusFilters"
              dense
              outlined
              clearable
              emit-value
              map-options
              multiple
              use-chips
              :options="jobStatusOptions"
              :label="t('Status')"
              class="jobs-list-header__filter"
            />
            <q-select
              v-model="levelFilters"
              dense
              outlined
              clearable
              emit-value
              map-options
              multiple
              use-chips
              :options="jobLevelOptions"
              :label="t('Level')"
              class="jobs-list-header__filter"
            />
            <q-select
              v-model="typeFilters"
              dense
              outlined
              clearable
              emit-value
              map-options
              multiple
              use-chips
              :options="jobTypeOptions"
              :label="t('Type')"
              class="jobs-list-header__filter"
            />
            <q-input
              v-model="search"
              dense
              outlined
              clearable
              :placeholder="t('Search…')"
              class="jobs-list-header__search"
            >
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <div class="jobs-list-header__actions row items-center no-wrap">
              <span class="text-white text-caption jobs-list-header__countdown panel-refresh-countdown">
                <span aria-hidden="true">↻</span>
                <span class="panel-refresh-countdown__value">{{ countdown }}s</span>
              </span>
              <q-btn flat round dense icon="refresh" color="white" @click="manualRefresh" />
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
            <div v-if="search || (statusFilters?.length ?? 0) || (levelFilters?.length ?? 0) || (typeFilters?.length ?? 0)" class="q-px-md q-pt-sm">
              <q-chip
                v-if="search"
                removable
                color="grey-8"
                text-color="white"
                icon="search"
                class="q-mb-xs q-mr-xs"
                @remove="search = ''"
              >
                {{ t('Search') }}: {{ search }}
              </q-chip>
              <q-chip
                v-for="status in (statusFilters ?? [])"
                :key="status"
                removable
                color="primary"
                text-color="white"
                icon="filter_list"
                class="q-mb-xs q-mr-xs"
                @remove="statusFilters = statusFilters.filter(value => value !== status)"
              >
                {{ t('Status') }}: {{ t(jobStatusMap[status]?.label ?? status) }}
              </q-chip>
              <q-chip
                v-for="level in (levelFilters ?? [])"
                :key="level"
                removable
                color="teal"
                text-color="white"
                icon="filter_list"
                class="q-mb-xs q-mr-xs"
                @remove="levelFilters = levelFilters.filter(value => value !== level)"
              >
                {{ t('Level') }}: {{ t(jobLevelLabels[level] ?? level) }}
              </q-chip>
              <q-chip
                v-for="type in (typeFilters ?? [])"
                :key="type"
                removable
                color="indigo"
                text-color="white"
                icon="filter_list"
                class="q-mb-xs q-mr-xs"
                @remove="typeFilters = typeFilters.filter(value => value !== type)"
              >
                {{ t('Type') }}: {{ jobTypeLabel(type) }}
              </q-chip>
            </div>
            <q-table
              :rows="filteredJobs"
              :columns="columns"
              row-key="scopeKey"
              binary-state-sort
              dense flat
              :loading="loading"
              v-model:pagination="pagination"
              @request="onRequest"
            >
              <template #body-cell-id="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-name="props">
                <q-td :props="props">
                  <a
                    href="#"
                    class="text-primary"
                    :title="`${t('Job Name')}: ${props.value}`"
                    @click.prevent="search = props.value ?? ''"
                  >
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props" class="text-center">
                  <a
                    v-if="props.row.status"
                    href="#"
                    class="inline-job-filter justify-center"
                    :title="`${t('Status')}: ${jobStatusLabel(props.row.status)}`"
                    @click.prevent="applyStatusFilter(props.row.status)"
                  >
                    <span
                      v-if="isWaitingStatus(displayJobStatus(props.row))"
                      class="row items-center no-wrap q-gutter-x-xs justify-center"
                    >
                      <q-icon name="hourglass_empty" color="orange-7" size="16px" class="animated-spin" />
                      <span class="text-orange-7 text-caption">{{ displayJobStatus(props.row) }}</span>
                    </span>
                    <JobStatusBadge v-else :status="displayJobStatus(props.row)" />
                  </a>
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-client="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openClientDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-type="props">
                <q-td :props="props" class="text-center">
                  <a
                    v-if="props.value"
                    href="#"
                    class="inline-job-filter"
                    :title="`${t('Type')}: ${jobTypeLabel(props.value)}`"
                    @click.prevent="applyTypeFilter(props.value)"
                  >
                    <JobTypeBadge :type="props.value" />
                  </a>
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-level="props">
                <q-td :props="props" class="text-center">
                  <a
                    v-if="props.value"
                    href="#"
                    class="inline-job-filter"
                    :title="`${t('Level')}: ${jobLevelLabel(props.value)}`"
                    @click.prevent="applyLevelFilter(props.value)"
                  >
                    <JobLevelBadge :level="props.value" />
                  </a>
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-starttime="props">
                <q-td :props="props">
                  <span :title="settings.relativeTime ? props.value : timeAgo(props.value, settings.locale)">
                    {{ settings.relativeTime ? timeAgo(props.value, settings.locale) : props.value }}
                  </span>
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right" style="min-width:90px">
                  <div>{{ fmtBytes(props.row.bytes) }}</div>
                  <q-linear-progress
                    v-if="isRunning(props.row.status)"
                    indeterminate
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                  <q-linear-progress
                    v-else
                    :value="bytesGauge(props.row.bytes)"
                    color="primary" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-files="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ formatNumber(props.value, settings.locale) }}</div>
                  <q-linear-progress
                    v-if="isRunning(props.row.status)"
                    indeterminate
                    color="teal" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                  <q-linear-progress
                    v-else
                    :value="filesGauge(props.row.files)"
                    color="teal" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-duration="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ props.value || '—' }}
                    <q-tooltip v-if="props.row.endtime">
                       {{ t('Ended') }}: {{ props.row.endtime }}
                    </q-tooltip>
                  </div>
                  <q-linear-progress
                    :value="durationGauge(props.value)"
                    color="orange" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-speed="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div v-if="isRunning(props.row.status)" class="text-grey-5">—</div>
                  <template v-else>
                    <div>{{ fmtSpeed(props.row.bytes, props.row.duration) }}</div>
                    <q-linear-progress
                      :value="speedGauge(props.row)"
                      color="cyan-7" track-color="grey-3"
                      size="4px" class="q-mt-xs" rounded
                    />
                  </template>
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center" style="white-space:nowrap">
                   <q-btn flat round dense size="sm" icon="restart_alt" :title="t('Rerun')"
                         @click="confirmRerun(props.row)" class="q-mr-xs" />
                  <q-btn v-if="isRunning(props.row.status)"
                         flat round dense size="sm" icon="cancel" color="negative" :title="t('Cancel')"
                         @click="confirmCancel(props.row)" class="q-mr-xs" />
                  <q-btn v-if="canRestoreFromJob(props.row)"
                         flat round dense size="sm" icon="restore" color="teal"
                         :title="t('Restore this job')"
                         @click="openRestoreDetails(props.row)"
                         class="q-mr-xs" />
                   <q-btn flat round dense size="sm" icon="info" :title="t('Details')"
                         @click="openJobDetails(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── ACTIONS ───────────────────────────────────────────────────────── -->
      <q-tab-panel name="actions" class="q-pa-none q-gutter-md">
        <!-- Enable / Disable jobs -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
             <span>{{ t('Enable / Disable Jobs') }}</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" @click="loadJobDefs" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="jobDefs"
              :columns="defsColumns"
              row-key="scopeKey"
              dense flat
              :loading="loadingDefs"
              :pagination="{ rowsPerPage: 15 }"
            >
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-chip dense square
                    :color="props.value ? 'positive' : 'grey'"
                    text-color="white"
                    :label="props.value ? t('enabled') : t('disabled')"
                    style="font-size:0.7rem" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center" style="white-space:nowrap">
                  <q-btn flat dense no-caps size="sm" icon="play_arrow" color="positive"
                         :label="t('Enable')"  class="q-mr-xs"
                         :disable="props.row.enabled"
                         @click="enableJob(props.row)" />
                  <q-btn flat dense no-caps size="sm" icon="pause" color="warning"
                         :label="t('Disable')" class="q-mr-xs"
                         :disable="!props.row.enabled"
                         @click="disableJob(props.row)" />
                   <q-btn flat dense no-caps size="sm" icon="send" color="primary"
                          :data-testid="`jobdef-run-${props.row.name}`"
                          :label="t('Run')"
                          @click="runThisJob(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── RUN ───────────────────────────────────────────────────────────── -->
      <q-tab-panel name="run">
        <q-card
          v-if="isCommonJobs"
          flat
          bordered
          class="q-mb-md bareos-panel"
          style="max-width:800px"
        >
          <q-card-section class="panel-header row items-center">
            <span>{{ t('Run Job Target') }}</span>
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
              {{ t('Run and defaults lookups in this tab use the selected director while the jobs list stays aggregated.') }}
            </div>
          </q-card-section>
        </q-card>
        <q-card flat bordered class="bareos-panel" style="max-width:640px">
          <q-card-section class="panel-header">{{ t('Run Job') }}</q-card-section>
          <q-card-section>
            <q-form @submit.prevent="runJob" class="q-gutter-md">
               <q-select v-model="runForm.job"     data-testid="run-job-field" :options="dotJobs"     :label="t('Job *')"     outlined dense
                         @update:model-value="onJobSelected" />
              <q-select v-model="runForm.client"  :options="dotClients"  :label="t('Client')"    outlined dense clearable />
              <q-select v-model="runForm.fileset" :options="dotFilesets" :label="t('Fileset')"   outlined dense clearable />
              <q-select v-model="runForm.pool"    :options="dotPools"    :label="t('Pool')"      outlined dense clearable />
              <q-select v-model="runForm.storage" :options="dotStorages" :label="t('Storage')"   outlined dense clearable />
              <q-select v-model="runForm.level"   :options="levels"      :label="t('Level')"     outlined dense />
              <q-input  v-model="runForm.when"                           :label="t('When (optional)')" outlined dense
                        :placeholder="t('YYYY-MM-DD HH:MM:SS')" />
              <q-input  v-model.number="runForm.priority" type="number" :label="t('Priority')" outlined dense style="max-width:140px" />
              <div>
                 <q-btn data-testid="run-job-submit" type="submit" color="primary" :label="t('Run Job')" icon="play_arrow"
                        no-caps :loading="runLoading" :disable="!runForm.job" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- ── RERUN ─────────────────────────────────────────────────────────── -->
      <q-tab-panel name="rerun" class="q-pa-none q-gutter-md">

        <!-- Completed jobs table -->
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
             <span>{{ t('Completed Jobs') }}</span>
            <q-space />
            <q-input v-model="rerunSearch" dense outlined :placeholder="t('Search…')" class="q-mr-sm"
                     style="width:200px" clearable>
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="rerunableJobs"
              :columns="rerunColumns"
              row-key="scopeKey"
              dense flat
              :pagination="{ rowsPerPage: 15, sortBy: 'id', descending: true }"
            >
              <template #body-cell-status="props">
                <q-td :props="props" class="text-center">
                  <JobStatusBadge :status="props.value" />
                </q-td>
              </template>
              <template #body-cell-client="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openClientDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-level="props">
                <q-td :props="props" class="text-center">
                  <JobLevelBadge v-if="props.value" :level="props.value" />
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-id="props">
                <q-td :props="props">
                  <a href="#" class="text-primary" @click.prevent="openJobDetails(props.row)">
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-bytes="props">
                <q-td :props="props" class="text-right">{{ fmtBytes(props.row.bytes) }}</q-td>
              </template>
              <template #body-cell-speed="props">
                <q-td :props="props" class="text-right" style="min-width:80px">
                  <div>{{ fmtSpeed(props.row.bytes, props.row.duration) }}</div>
                  <q-linear-progress
                    :value="speedGauge(props.row)"
                    color="cyan-7" track-color="grey-3"
                    size="4px" class="q-mt-xs" rounded
                  />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                   <q-btn flat round dense size="sm" icon="restart_alt" color="primary" :title="t('Rerun')"
                         @click="confirmRerun(props.row)" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>

        <!-- Manual ID fallback -->
        <q-card flat bordered class="bareos-panel" style="max-width:520px">
          <q-card-section class="panel-header">{{ t('Rerun by Job ID') }}</q-card-section>
          <q-card-section>
            <q-form @submit.prevent="submitRerun" class="q-gutter-md">
              <q-input
                v-model="rerunJobId"
                :label="t('Job ID *')"
                outlined
                dense
                type="number"
                min="1"
                step="1"
                style="max-width:180px"
                :hint="t('Enter the ID of any completed job from the selected directors')"
                :error="Boolean(rerunJobIdError)"
                :error-message="rerunJobIdError"
              />
              <div>
                <q-btn type="submit" color="primary" :label="t('Rerun')" icon="restart_alt"
                        no-caps :loading="rerunLoading" :disable="!rerunJobIdValue" />
              </div>
            </q-form>
          </q-card-section>
        </q-card>

      </q-tab-panel>

      <!-- ── TIMELINE ──────────────────────────────────────────────────────── -->
      <q-tab-panel name="timeline" class="q-pa-none">
        <JobTimeline
          :key="activeDirectors.join('\u0000') || 'jobs-timeline'"
          :directors="activeDirectors"
          :client-details-query="buildClientDetailsQuery({
            jobsOrigin: true,
            clientsTab: 'timeline',
            jobsAction: tab,
            jobsStatus: statusFilters,
            jobsLevel: encodeJobsLevelFilters(levelFilters),
            jobsType: encodeJobsTypeFilters(typeFilters),
            jobsSearch: search,
          })"
          :job-details-query="timelineJobDetailsQuery"
        />
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useQuasar } from 'quasar'
import { useI18n } from 'vue-i18n'
import {
  jobStatusMap,
  formatBytes,
  formatSpeed,
  parseDurationSecs,
  timeAgo,
} from '../mock/index.js'
import { directorCollection, normaliseJob } from '../composables/useDirectorFetch.js'
import { createDirectorCommandClient } from '../composables/directorAggregate.js'
import {
  fetchAggregatedJobsPage,
  sortJobsByPagination,
  usesDefaultJobsSorting,
} from '../composables/jobsAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildClientDetailsQuery } from '../utils/clients.js'
import { quoteDirectorString } from '../utils/directorStrings.js'
import { buildDirectorOptions } from '../utils/director.js'
import { formatNumber } from '../utils/locales.js'
import { resolveJobTypeCode, resolveJobTypeInfo } from '../utils/jobTypes.js'
import {
  buildJobDetailsQuery,
  encodeJobsLevelFilters,
  encodeJobsStatusFilters,
  encodeJobsTypeFilters,
  normaliseJobId,
  resolveJobsLevelFilters,
  resolveJobsSearchQuery,
  resolveJobsStatusFilters,
  resolveJobsTypeFilters,
  withJobsSearchQuery,
  withJobsLevelFilterQuery,
  withJobsStatusFilterQuery,
  withJobsTypeFilterQuery,
} from '../utils/jobs.js'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorScopePanel from '../components/DirectorScopePanel.vue'
import JobStatusBadge from '../components/JobStatusBadge.vue'
import JobLevelBadge from '../components/JobLevelBadge.vue'
import JobTypeBadge from '../components/JobTypeBadge.vue'
import JobTimeline from '../components/JobTimeline.vue'

const route    = useRoute()
const router   = useRouter()
const $q       = useQuasar()
const auth     = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const { t } = useI18n()
const validTabs = new Set(['list', 'actions', 'run', 'rerun', 'timeline'])
function normaliseTab(value) {
  return validTabs.has(value) ? value : 'list'
}

function jobsStatusFiltersEqual(left, right) {
  return encodeJobsStatusFilters(left) === encodeJobsStatusFilters(right)
}

function jobsLevelFiltersEqual(left, right) {
  return encodeJobsLevelFilters(left) === encodeJobsLevelFilters(right)
}

function jobsTypeFiltersEqual(left, right) {
  return encodeJobsTypeFilters(left) === encodeJobsTypeFilters(right)
}

const tab          = ref(normaliseTab(route.query.action))
const search       = ref(resolveJobsSearchQuery(route.query))
const statusFilters = ref(resolveJobsStatusFilters(route.query))
const levelFilters = ref(resolveJobsLevelFilters(route.query))
const typeFilters = ref(resolveJobsTypeFilters(route.query))
const directorErrors = ref([])
const fmtBytes  = formatBytes
const fmtSpeed  = formatSpeed
const jobStatusOptions = computed(() => Object.entries(jobStatusMap).map(([value, meta]) => ({
  value,
  label: t(meta.label),
})))
const jobLevelLabels = {
  F: 'Full',
  I: 'Incremental',
  D: 'Differential',
  V: 'Virtual Full',
  B: 'Base',
}
const jobLevelOptions = computed(() => Object.entries(jobLevelLabels).map(([value, label]) => ({
  value,
  label: t(label),
})))
const jobTypeOrder = ['B', 'R', 'V', 'D', 'A', 'C', 'c', 'g', 'M', 'O', 'S', 'U', 'I']
const jobTypeOptions = computed(() => jobTypeOrder.map((value) => {
  const info = resolveJobTypeInfo(value)
  return {
    value,
    label: info.labelKey ? t(info.labelKey) : value,
  }
}))

function escapeHtml(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#39;')
}

function overlayRuntimeStatuses(jobs, runtimeJobs) {
  const runtimeById = new Map(
    (runtimeJobs ?? [])
      .map(job => ({
        id: Number(job?.jobid ?? job?.id ?? 0),
        runtimeStatus: job?.status ?? job?.jobstatus,
      }))
      .filter(job => Number.isFinite(job.id) && job.id > 0 && typeof job.runtimeStatus === 'string')
      .map(job => [job.id, job.runtimeStatus])
  )

  return jobs.map((job) => ({
    ...job,
    runtimeStatus: runtimeById.get(job.id) ?? job.runtimeStatus,
  }))
}

const directorOptions = computed(() => {
  return buildDirectorOptions({
    availableDirectors: director.availableDirectors,
    selectedDirectors: settings.selectedDirectors,
    currentDirector: auth.user?.director,
    fallbackDirector: settings.directorName,
  })
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

function canRestoreFromJob(job) {
  return resolveJobTypeCode(job?.type) !== 'R'
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

const isCommonJobs = computed(() => activeDirectors.value.length > 1)
const showDirectorColumn = computed(() => activeDirectors.value.length > 1)
const isSingleDirectorScope = computed(() => activeDirectors.value.length === 1)
const jobsScopeLabel = computed(() => (
  isCommonJobs.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))
const singletonTabDirector = ref('')
const singletonTabDirectorOptions = computed(() => (
  activeDirectors.value.map(value => ({ label: value, value }))
))
const currentSingletonDirector = computed(() => (
  isCommonJobs.value
    ? (singletonTabDirector.value || activeDirectors.value[0] || '')
    : (activeDirectors.value[0] || '')
))
const timelineJobDetailsQuery = computed(() => buildJobDetailsQuery({
  jobsAction: tab.value,
  jobsStatus: statusFilters.value,
  jobsLevel: levelFilters.value,
  jobsType: typeFilters.value,
  jobsSearch: search.value,
}))

function syncSingletonTabDirector() {
  const validDirectors = activeDirectors.value
  if (!validDirectors.length) {
    singletonTabDirector.value = ''
    return
  }

  if (validDirectors.includes(singletonTabDirector.value)) {
    return
  }

  singletonTabDirector.value = validDirectors[0]
}

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

async function ensureSingletonTabDirector() {
  const targetDirector = currentSingletonDirector.value
  if (!targetDirector) {
    return null
  }

  if (auth.user?.director === targetDirector && director.isConnected) {
    return targetDirector
  }

  await switchActiveDirector(targetDirector)
  return targetDirector
}

function syncJobsScopeDirectorQuery() {
  if (typeof route.query.scopeDirector !== 'string' || !route.query.scopeDirector) {
    return
  }

  const query = { ...route.query }
  delete query.scopeDirector
  router.replace({ path: route.path, query })
}

// ── paginated job list ────────────────────────────────────────────────────────
const jobs       = ref([])
const totalJobs  = ref(0)
const loading    = ref(false)
const error      = ref(null)
// rowsNumber must live inside the pagination object for Quasar's server-side
// pagination controls to render correctly.
const pagination = ref({ page: 1, rowsPerPage: 25, sortBy: 'id', descending: true, rowsNumber: 0 })

async function fetchPage() {
  loading.value = true
  error.value   = null
  directorErrors.value = []
  const currentPagination = pagination.value
  const { page, rowsPerPage } = currentPagination
  const offset = (page - 1) * rowsPerPage
  const encodedStatusFilters = encodeJobsStatusFilters(statusFilters.value)
  const encodedLevelFilters = encodeJobsLevelFilters(levelFilters.value)
  const encodedTypeFilters = encodeJobsTypeFilters(typeFilters.value)
  const levelFilter = encodedLevelFilters ? ` joblevel=${encodedLevelFilters}` : ''
  const typeFilter = encodedTypeFilters ? ` jobtype=${encodedTypeFilters}` : ''
  const useDefaultSorting = usesDefaultJobsSorting(currentPagination)
  try {
    if (activeDirectors.value.length === 0) {
      jobs.value = []
      totalJobs.value = 0
      pagination.value = { ...pagination.value, rowsNumber: 0 }
      return
    }

    if (activeDirectors.value.length > 1) {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

        const result = await fetchAggregatedJobsPage(
          credentials,
          activeDirectors.value,
          pagination.value,
          statusFilters.value,
          levelFilters.value,
          typeFilters.value,
        )
      jobs.value = result.jobs
      directorErrors.value = result.directorErrors
      totalJobs.value = result.totalJobs
      pagination.value = { ...pagination.value, rowsNumber: result.totalJobs }
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    if (!encodedStatusFilters.includes(',')) {
      const filter = `${encodedStatusFilters ? ` jobstatus=${encodedStatusFilters}` : ''}${levelFilter}${typeFilter}`
      const countResult = await director.call(`list jobs count${filter}`)
      const count = Number(directorCollection(countResult?.jobs)[0]?.count ?? 0)
      const limit = useDefaultSorting
        ? rowsPerPage
        : Math.max(count, 1)
      const pageResult = await director.call(
        `llist jobs reverse limit=${limit} offset=${useDefaultSorting ? offset : 0}${filter}`
      )
      const fetchedJobs = directorCollection(pageResult?.jobs).map((job) => {
        const normalized = normaliseJob(job)
        return {
          ...normalized,
          director: currentDirector,
          scopeKey: `${currentDirector}:${normalized.id}`,
        }
      })
      const sortedJobs = useDefaultSorting
        ? fetchedJobs
        : sortJobsByPagination(fetchedJobs, currentPagination).slice(offset, offset + rowsPerPage)
      const runningJobIds = sortedJobs.filter(job => isRunning(job.status)).map(job => job.id)
      if (runningJobIds.length > 0) {
        const runtimeStatus = await Promise.allSettled([director.call('status director')])
        jobs.value = runtimeStatus[0].status === 'fulfilled'
          ? overlayRuntimeStatuses(sortedJobs, runtimeStatus[0].value?.running)
          : sortedJobs
      } else {
        jobs.value = sortedJobs
      }
      totalJobs.value = count
      pagination.value = { ...pagination.value, rowsNumber: count }
      return
    }

    const filters = resolveJobsStatusFilters({ status: encodedStatusFilters })
    const countResults = await Promise.allSettled(filters.map(status => (
      director.call(`list jobs count jobstatus=${status}${levelFilter}${typeFilter}`)
    )))
    const pageResults = await Promise.allSettled(filters.map((status, index) => {
      if (useDefaultSorting) {
        return director.call(
          `llist jobs reverse limit=${offset + rowsPerPage} offset=0 jobstatus=${status}${levelFilter}${typeFilter}`
        )
      }

      const countResult = countResults[index]
      if (countResult?.status === 'fulfilled') {
        const count = Number(directorCollection(countResult.value?.jobs)[0]?.count ?? 0)
        if (count === 0) {
          return Promise.resolve({ jobs: [] })
        }

        return director.call(`llist jobs reverse limit=${count} offset=0 jobstatus=${status}${levelFilter}${typeFilter}`)
      }

      return director.call(
        `llist jobs reverse limit=${offset + rowsPerPage} offset=0 jobstatus=${status}${levelFilter}${typeFilter}`
      )
    }))
    const rejectedPageResult = pageResults.find(result => result.status === 'rejected')
    if (rejectedPageResult?.status === 'rejected') {
      throw rejectedPageResult.reason
    }
    const mergedJobs = sortJobsByPagination([...pageResults.flatMap((result) => (
      result.status === 'fulfilled'
        ? directorCollection(result.value?.jobs).map((job) => {
          const normalized = normaliseJob(job)
          return {
            ...normalized,
            director: currentDirector,
            scopeKey: `${currentDirector}:${normalized.id}`,
          }
        })
        : []
    ))], currentPagination)
    const visibleJobs = mergedJobs.slice(offset, offset + rowsPerPage)
    const runningJobIds = visibleJobs.filter(job => isRunning(job.status)).map(job => job.id)
    if (runningJobIds.length > 0) {
      const runtimeStatus = await Promise.allSettled([director.call('status director')])
      jobs.value = runtimeStatus[0].status === 'fulfilled'
        ? overlayRuntimeStatuses(visibleJobs, runtimeStatus[0].value?.running)
        : visibleJobs
    } else {
      jobs.value = visibleJobs
    }
    const count = countResults.reduce((sum, result, index) => (
      sum + (
        result.status === 'fulfilled'
          ? Number(directorCollection(result.value?.jobs)[0]?.count ?? 0)
          : numberOfJobsFromResult(pageResults[index])
      )
    ), 0)
    totalJobs.value = count
    pagination.value = { ...pagination.value, rowsNumber: count }
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

function numberOfJobsFromResult(result) {
  if (result?.status !== 'fulfilled') {
    return 0
  }

  return directorCollection(result.value?.jobs).length
}

function refresh() { fetchPage() }

function onRequest(props) {
  pagination.value = { ...pagination.value, ...props.pagination }
  fetchPage()
}

const filteredJobs = computed(() => {
  if (!search.value) return jobs.value
  const q = search.value.toLowerCase()
  return jobs.value.filter(j =>
    j.name.toLowerCase().includes(q) ||
    j.client.toLowerCase().includes(q) ||
    String(j.id).includes(q)
  )
})

const maxBytes    = computed(() => Math.max(1, ...filteredJobs.value.map(j => j.bytes)))
const maxFiles    = computed(() => Math.max(1, ...filteredJobs.value.map(j => j.files)))
const maxDuration = computed(() => Math.max(1, ...filteredJobs.value.map(j => parseDurationSecs(j.duration))))

function bytesGauge(val)    { return (val || 0) / maxBytes.value }
function filesGauge(val)    { return (val || 0) / maxFiles.value }
function durationGauge(str) { return parseDurationSecs(str) / maxDuration.value }

function jobSpeedBps(row) {
  const secs = parseDurationSecs(row.duration)
  if (!secs) return 0
  const bytes = typeof row.bytes === 'string' ? parseFloat(row.bytes) : (row.bytes || 0)
  return bytes / secs
}
const maxSpeed = computed(() => Math.max(1, ...filteredJobs.value
  .filter(j => !isRunning(j.status))
  .map(j => jobSpeedBps(j))))
function speedGauge(row) { return jobSpeedBps(row) / maxSpeed.value }

function displayJobStatus(job) {
  return job?.runtimeStatus ?? job?.status ?? '?'
}

function isWaitingStatus(status) {
  return typeof status === 'string' && status.toLowerCase().includes('is waiting')
}

function applyStatusFilter(status) {
  if (!status) {
    return
  }

  statusFilters.value = [status]
}

function applyLevelFilter(level) {
  if (!level) {
    return
  }

  levelFilters.value = [level]
}

function applyTypeFilter(type) {
  const normalized = resolveJobTypeCode(type)
  if (!normalized) {
    return
  }

  typeFilters.value = [normalized]
}

function jobLevelLabel(level) {
  return t(jobLevelLabels[level] ?? level)
}

function jobTypeLabel(type) {
  const info = resolveJobTypeInfo(type)
  return info.labelKey ? t(info.labelKey) : String(type ?? '')
}

function jobStatusLabel(status) {
  return t(jobStatusMap[status]?.label ?? status)
}

const runningJobs  = computed(() => jobs.value.filter(j => isRunning(j.status)))

// Rerunable = any finished job (not currently running)
const rerunSearch  = ref('')
const rerunableJobs = computed(() => {
  const base = jobs.value.filter(j => !isRunning(j.status))
  if (!rerunSearch.value) return base
  const q = rerunSearch.value.toLowerCase()
  return base.filter(j =>
    j.name.toLowerCase().includes(q) ||
    j.client.toLowerCase().includes(q) ||
    String(j.id).includes(q)
  )
})

function isRunning(status) {
  // R = Running, l = data saved (still active)
  return status === 'R' || status === 'l'
}

// ── columns ───────────────────────────────────────────────────────────────────
const columns = computed(() => [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true, style: 'width:60px' },
  ...(showDirectorColumn.value ? [{
    name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',   sortable: true },
  { name: 'type',      label: 'Type',     field: 'type',      align: 'center', sortable: true },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center', sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center', sortable: true },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',   sortable: true },
  { name: 'duration',  label: 'Duration', field: 'duration',  align: 'right',  sortable: true,
    sort: (a, b) => parseDurationSecs(a) - parseDurationSecs(b) },
  { name: 'files',     label: 'Files',    field: 'files',     align: 'right',  sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right',  sortable: true },
  { name: 'speed',     label: 'Speed',    field: 'speed',     align: 'right', sortable: true,
    sort: (_a, _b, rowA, rowB) => jobSpeedBps(rowA) - jobSpeedBps(rowB) },
  { name: 'errors',    label: 'Errors',   field: 'errors',    align: 'center', sortable: true },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:100px' },
].map((col) => ({ ...col, label: col.label ? t(col.label) : col.label })))

const runningColumns = computed(() => [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true },
  ...(showDirectorColumn.value ? [{
    name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left', sortable: true },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left', sortable: true },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:90px' },
].map((col) => ({ ...col, label: col.label ? t(col.label) : col.label })))

const rerunColumns = computed(() => [
  { name: 'id',        label: 'ID',       field: 'id',        align: 'right',  sortable: true, style: 'width:60px' },
  ...(showDirectorColumn.value ? [{
    name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',      label: 'Job Name', field: 'name',      align: 'left',   sortable: true },
  { name: 'client',    label: 'Client',   field: 'client',    align: 'left',   sortable: true },
  { name: 'level',     label: 'Level',    field: 'level',     align: 'center', sortable: true },
  { name: 'status',    label: 'Status',   field: 'status',    align: 'center', sortable: true },
  { name: 'starttime', label: 'Start',    field: 'starttime', align: 'left',   sortable: true },
  { name: 'bytes',     label: 'Bytes',    field: 'bytes',     align: 'right', sortable: true },
  { name: 'speed',     label: 'Speed',    field: 'speed',     align: 'right', sortable: true,
    sort: (_a, _b, rowA, rowB) => jobSpeedBps(rowA) - jobSpeedBps(rowB) },
  { name: 'actions',   label: '',         field: 'actions',   align: 'center', style: 'width:60px' },
].map((col) => ({ ...col, label: col.label ? t(col.label) : col.label })))

const defsColumns = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',    label: 'Job Name', field: 'name',    align: 'left',   sortable: true },
  { name: 'type',    label: 'Type',     field: 'type',    align: 'center', sortable: true },
  { name: 'enabled', label: 'Status',   field: 'enabled', align: 'center', sortable: true },
  { name: 'actions', label: '',         field: 'actions', align: 'center', style: 'width:220px' },
].map((col) => ({ ...col, label: col.label ? t(col.label) : col.label })))

// ── job definitions (for enable/disable) ──────────────────────────────────────
const jobDefs       = ref([])
const loadingDefs   = ref(false)

async function loadJobDefs() {
  loadingDefs.value = true
  try {
    const credentials = auth.getCredentials()
    if (!credentials?.password) {
      throw new Error(t('Not logged in.'))
    }

    const results = await Promise.all(activeDirectors.value.map(async (directorName) => {
      const client = await createDirectorCommandClient({
        ...credentials,
        director: directorName,
      })
      try {
        const res = await client.call('show jobs')
        const raw = res?.jobs ?? {}
        const list = Array.isArray(raw) ? raw : Object.values(raw)
        return list.map(j => ({
          name: j.name,
          type: j.type ?? '',
          enabled: j.enabled !== false,
          director: directorName,
          scopeKey: `${directorName}:${j.name}`,
        }))
      } finally {
        client.disconnect()
      }
    }))
    jobDefs.value = results
      .flat()
      .sort((a, b) => (
        a.name.localeCompare(b.name) || (a.director ?? '').localeCompare(b.director ?? '')
      ))
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Could not load job definitions')}: ${e.message}` })
  } finally {
    loadingDefs.value = false
  }
}

// ── row-level actions ─────────────────────────────────────────────────────────
async function switchToJobDirector(job) {
  if (!job?.director) {
    return
  }

  if (auth.user?.director === job.director && director.isConnected) {
    return
  }

  await switchActiveDirector(job.director)
}

async function openJobDetails(job) {
  try {
    await switchToJobDirector(job)
    await router.push({
      name: 'job-details',
      params: { id: job.id },
        query: buildJobDetailsQuery({
          director: job.director,
          jobsAction: tab.value,
          jobsStatus: statusFilters.value,
          jobsLevel: levelFilters.value,
          jobsType: typeFilters.value,
          jobsSearch: search.value,
        }),
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not switch to director {director}: {message}', {
        director: job.director ?? t('unknown'),
        message: error.message,
      }),
    })
  }
}

async function openClientDetails(job) {
  try {
    await switchToJobDirector(job)
    await router.push({
      name: 'client-details',
      params: { name: job.client },
      query: buildClientDetailsQuery({
        director: job.director,
        jobsOrigin: true,
        jobsAction: tab.value,
        jobsStatus: statusFilters.value,
        jobsLevel: encodeJobsLevelFilters(levelFilters.value),
        jobsType: encodeJobsTypeFilters(typeFilters.value),
        jobsSearch: search.value,
      }),
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not switch to director {director}: {message}', {
        director: job.director ?? t('unknown'),
        message: error.message,
      }),
    })
  }
}

async function openRestoreDetails(job) {
  try {
    await switchToJobDirector(job)
    await router.push({
      name: 'restore',
      query: {
        client: job.client,
        director: job.director,
        jobid: job.id,
      },
    })
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: t('Could not switch to director {director}: {message}', {
        director: job.director ?? t('unknown'),
        message: error.message,
      }),
    })
  }
}

function confirmCancel(job) {
  $q.dialog({
    title: t('Cancel Job'),
    message: t('Cancel job <b>{name}</b> (ID&nbsp;{id})?', { name: job.name, id: job.id }),
    html: true,
    ok:     { label: t('Cancel Job'), color: 'negative', flat: true },
    cancel: { label: t('Keep running'), flat: true },
  }).onOk(() => doCancel(job))
}

async function doCancel(job) {
  try {
    await switchToJobDirector(job)
    await director.call(`cancel jobid=${job.id} yes`)
    $q.notify({ type: 'positive', message: t('Job {id} cancelled.', { id: job.id }) })
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: t('Cancel failed: {message}', { message: e.message }) })
  }
}

function confirmRerun(job) {
  $q.dialog({
    title: t('Rerun Job'),
    message: `${t('Rerun job')} <b>${escapeHtml(job.name)}</b> (ID&nbsp;${escapeHtml(job.id)})?`,
    html: true,
    ok:     { label: t('Rerun'), color: 'primary', flat: true },
    cancel: { label: t('Cancel'), flat: true },
  }).onOk(() => doRerun(job))
}

async function doRerun(job) {
  try {
    await switchToJobDirector(job)
    const jobId = normaliseJobId(typeof job === 'object' ? job.id : job)
    if (jobId === null) {
      throw new Error(t('Job ID must be a positive integer.'))
    }

    if (!(typeof job === 'object' && job?.name)) {
      const lookup = await director.call(`llist jobid=${jobId}`)
      if (directorCollection(lookup?.jobs).length === 0) {
        throw new Error(`${t('Job')} ${jobId} ${t('was not found.')}`)
      }
    }

    const res = await director.call(`rerun jobid=${jobId} yes`)
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `${t('Job restarted as ID')} ${newId}.` })
    tab.value = 'list'
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Rerun failed')}: ${e.message}` })
  }
}

// ── bulk cancel ───────────────────────────────────────────────────────────────
function cancelAll() {
  const n = runningJobs.value.length
  $q.dialog({
    title: t('Cancel All Running Jobs'),
    message: t('Cancel all <b>{count}</b> running job(s)?', { count: n }),
    html: true,
    ok:     { label: t('Cancel all jobs'), color: 'negative', flat: true },
    cancel: { label: t('Keep running'), flat: true },
  }).onOk(async () => {
    const results = await Promise.allSettled(
      runningJobs.value.map(j => (async () => {
        await switchToJobDirector(j)
        return director.call(`cancel jobid=${j.id} yes`)
      })())
    )
    const failed = results.filter(r => r.status === 'rejected').length
    if (failed) {
      $q.notify({ type: 'warning', message: `Cancelled ${n - failed} / ${n} jobs (${failed} failed).` })
    } else {
      $q.notify({ type: 'positive', message: `All ${n} running jobs cancelled.` })
    }
    refresh()
  })
}

// ── enable / disable ──────────────────────────────────────────────────────────
async function enableJob(job) {
  try {
    await switchToJobDirector(job)
    await director.call(`enable job=${quoteDirectorString(job.name)} yes`)
    $q.notify({ type: 'positive', message: `Job "${job.name}" enabled.` })
    const j = jobDefs.value.find(d => d.scopeKey === job.scopeKey)
    if (j) j.enabled = true
  } catch (e) {
    $q.notify({ type: 'negative', message: `Enable failed: ${e.message}` })
  }
}

async function disableJob(job) {
  try {
    await switchToJobDirector(job)
    await director.call(`disable job=${quoteDirectorString(job.name)} yes`)
    $q.notify({ type: 'positive', message: `Job "${job.name}" disabled.` })
    const j = jobDefs.value.find(d => d.scopeKey === job.scopeKey)
    if (j) j.enabled = false
  } catch (e) {
    $q.notify({ type: 'negative', message: `Disable failed: ${e.message}` })
  }
}

async function runThisJob(job) {
  if (job?.director && activeDirectors.value.includes(job.director)) {
    singletonTabDirector.value = job.director
  }
  await switchToJobDirector(job)
  if (dotJobs.value.length === 0) await loadRunOptions()
  runForm.value = { job: job.name, client: null, fileset: null, pool: null, storage: null, level: 'Incremental', when: '', priority: 10 }
  await onJobSelected(job.name)
  tab.value = 'run'
}

// ── run form ──────────────────────────────────────────────────────────────────
const dotJobs     = ref([])
const dotClients  = ref([])
const dotFilesets = ref([])
const dotPools    = ref([])
const dotStorages = ref([])
const levels      = ['Full', 'Incremental', 'Differential']
const runLoading  = ref(false)

const runForm = ref({ job: null, client: null, fileset: null, pool: null, storage: null, level: 'Incremental', when: '', priority: 10 })

async function loadRunOptions() {
  try {
    await ensureSingletonTabDirector()
    const [j, c, f, p, s] = await Promise.all([
      director.call('.jobs'),
      director.call('.clients'),
      director.call('.filesets'),
      director.call('.pools'),
      director.call('.storage'),
    ])
    dotJobs.value = directorCollection(j?.jobs).map(x => x.name).sort()
    dotClients.value = directorCollection(c?.clients).map(x => x.name).sort()
    dotFilesets.value = directorCollection(f?.filesets).map(x => x.name).sort()
    dotPools.value = directorCollection(p?.pools).map(x => x.name).sort()
    dotStorages.value = directorCollection(s?.storages).map(x => x.name).sort()
  } catch (e) {
    // non-fatal — user can still type values
    console.warn('Could not load run form options:', e.message)
  }
}

async function onJobSelected(name) {
  if (!name) return
  try {
    await ensureSingletonTabDirector()
    const res = await director.call(`.defaults job=${quoteDirectorString(name)}`)
    const d   = res?.defaults ?? res ?? {}
    if (d.client)   runForm.value.client   = d.client
    if (d.fileset)  runForm.value.fileset  = d.fileset
    if (d.pool)     runForm.value.pool     = d.pool
    if (d.storage)  runForm.value.storage  = d.storage
    if (d.level)    runForm.value.level    = d.level
    if (d.priority) runForm.value.priority = Number(d.priority)
  } catch { /* ignore */ }
}

async function runJob() {
  const f = runForm.value
  if (!f.job) return
  let cmd = `run job=${quoteDirectorString(f.job)}`
  if (f.client)   cmd += ` client=${quoteDirectorString(f.client)}`
  if (f.fileset)  cmd += ` fileset=${quoteDirectorString(f.fileset)}`
  if (f.pool)     cmd += ` pool=${quoteDirectorString(f.pool)}`
  if (f.storage)  cmd += ` storage=${quoteDirectorString(f.storage)}`
  if (f.level)    cmd += ` level=${quoteDirectorString(f.level)}`
  if (f.when)     cmd += ` when=${quoteDirectorString(f.when)}`
  if (f.priority) cmd += ` priority=${f.priority}`
  cmd += ' yes'

  runLoading.value = true
  try {
    await ensureSingletonTabDirector()
    const res   = await director.call(cmd)
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `Job started — ID ${newId}` })
    tab.value = 'list'
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Run failed: ${e.message}` })
  } finally {
    runLoading.value = false
  }
}

// ── rerun tab ─────────────────────────────────────────────────────────────────
const rerunJobId   = ref('')
const rerunLoading = ref(false)
const rerunJobIdValue = computed(() => normaliseJobId(rerunJobId.value))
const rerunJobIdError = computed(() => {
  if (rerunJobId.value === '' || rerunJobId.value === null) {
    return ''
  }

  return rerunJobIdValue.value === null ? t('Enter a positive Job ID.') : ''
})

async function submitRerun() {
  if (rerunJobIdValue.value === null) {
    $q.notify({ type: 'negative', message: t('Enter a positive Job ID.') })
    return
  }

  rerunLoading.value = true
  try {
    const listedMatches = rerunableJobs.value.filter(job => Number(job.id) === rerunJobIdValue.value)
    if (listedMatches.length === 1) {
      await doRerun(listedMatches[0])
      rerunJobId.value = ''
      return
    }
    if (listedMatches.length > 1) {
      throw new Error(`Job ID ${rerunJobIdValue.value} ${t('exists on multiple selected directors. Use the matching row action instead.')}`)
    }

    const credentials = auth.getCredentials()
    if (!credentials?.password) {
      throw new Error(t('Not logged in.'))
    }

    const matches = []
    const results = await Promise.allSettled(activeDirectors.value.map(async (directorName) => {
      const client = await createDirectorCommandClient({
        ...credentials,
        director: directorName,
      })
      try {
        const lookup = await client.call(`llist jobid=${rerunJobIdValue.value}`)
        if (directorCollection(lookup?.jobs).length > 0) {
          matches.push({ id: rerunJobIdValue.value, director: directorName })
        }
      } finally {
        client.disconnect()
      }
    }))

    if (matches.length === 0) {
      const rejected = results.find(result => result.status === 'rejected')
      if (rejected?.status === 'rejected') {
        throw rejected.reason
      }
      throw new Error(`Job ID ${rerunJobIdValue.value} ${t('was not found on the selected directors.')}`)
    }
    if (matches.length > 1) {
      throw new Error(`Job ID ${rerunJobIdValue.value} ${t('exists on multiple selected directors. Use the matching row action instead.')}`)
    }

    await doRerun(matches[0])
    rerunJobId.value = ''
  } catch (error) {
    $q.notify({
      type: 'negative',
      message: error?.message ?? String(error),
    })
  } finally {
    rerunLoading.value = false
  }
}

// ── lifecycle ─────────────────────────────────────────────────────────────────
const countdown = ref(settings.refreshInterval)
let _timer = null

async function loadAvailableDirectors() {
  try {
    await director.fetchAvailableDirectors()
  } catch {
    // Keep the selector usable with the active director when the proxy list is
    // unavailable.
  }
}

function startAutoRefresh() {
  stopAutoRefresh()
  countdown.value = settings.refreshInterval
  _timer = setInterval(() => {
    countdown.value -= 1
    if (countdown.value <= 0) {
      refresh()
      countdown.value = settings.refreshInterval
    }
  }, 1000)
}

function stopAutoRefresh() {
  clearInterval(_timer)
  _timer = null
}

function manualRefresh() {
  refresh()
  countdown.value = settings.refreshInterval
}

onMounted(() => {
  loadAvailableDirectors()
  syncSelectedDirectors()
  syncJobsScopeDirectorQuery()
  syncSingletonTabDirector()
  fetchPage()
  if (director.isConnected && (isSingleDirectorScope.value || tab.value === 'actions')) {
    loadJobDefs()
  }
  if (director.isConnected && (isSingleDirectorScope.value || tab.value === 'run')) {
    loadRunOptions()
  }
  startAutoRefresh()
})

onUnmounted(() => {
  stopAutoRefresh()
})

watch(() => director.isConnected, (connected) => {
  if (connected) {
    fetchPage()
    if (isSingleDirectorScope.value || tab.value === 'actions') {
      loadJobDefs()
    }
    if (isSingleDirectorScope.value || tab.value === 'run') {
      loadRunOptions()
    }
    startAutoRefresh()
  }
})

watch(() => route.query.status, (value) => {
  const next = resolveJobsStatusFilters({ status: value })
  if (!jobsStatusFiltersEqual(statusFilters.value, next)) {
    statusFilters.value = next
  }
})

watch(() => route.query.level, (value) => {
  const next = resolveJobsLevelFilters({ level: value })
  if (!jobsLevelFiltersEqual(levelFilters.value, next)) {
    levelFilters.value = next
  }
})

watch(() => route.query.type, (value) => {
  const next = resolveJobsTypeFilters({ type: value })
  if (!jobsTypeFiltersEqual(typeFilters.value, next)) {
    typeFilters.value = next
  }
})

watch(() => route.query.action, (value) => {
  const next = normaliseTab(value)
  if (tab.value !== next) {
    tab.value = next
  }
})

watch(() => [route.query.search, route.query.name], () => {
  const next = resolveJobsSearchQuery(route.query)
  if (search.value !== next) {
    search.value = next
  }
})

watch(() => route.query.scopeDirector, (value) => {
  if (typeof value === 'string' && value) {
    syncJobsScopeDirectorQuery()
  }
})

watch(search, (next) => {
  const query = withJobsSearchQuery(route.query, next)
  const current = resolveJobsSearchQuery(route.query)
  if (query.search !== current || route.query.name !== undefined) {
    router.replace({ path: route.path, query })
  }
})

watch(statusFilters, (next) => {
  const normalized = resolveJobsStatusFilters({ status: next })
  if (!jobsStatusFiltersEqual(next, normalized)) {
    statusFilters.value = normalized
    return
  }

  const query = withJobsStatusFilterQuery(route.query, normalized)
  if ((query.status ?? '') !== (typeof route.query.status === 'string' ? route.query.status : '')) {
    router.replace({ path: route.path, query })
  }
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
}, { deep: true })

watch(levelFilters, (next) => {
  const normalized = resolveJobsLevelFilters({ level: next })
  if (!jobsLevelFiltersEqual(next, normalized)) {
    levelFilters.value = normalized
    return
  }

  const query = withJobsLevelFilterQuery(route.query, normalized)
  if ((query.level ?? '') !== (typeof route.query.level === 'string' ? route.query.level : '')) {
    router.replace({ path: route.path, query })
  }
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
}, { deep: true })

watch(typeFilters, (next) => {
  const normalized = resolveJobsTypeFilters({ type: next })
  if (!jobsTypeFiltersEqual(next, normalized)) {
    typeFilters.value = normalized
    return
  }

  const query = withJobsTypeFilterQuery(route.query, normalized)
  if ((query.type ?? '') !== (typeof route.query.type === 'string' ? route.query.type : '')) {
    router.replace({ path: route.path, query })
  }
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
}, { deep: true })

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
  syncJobsScopeDirectorQuery()
  syncSingletonTabDirector()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  syncSingletonTabDirector()
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
  if (tab.value === 'actions') {
    loadJobDefs()
  }
  if (tab.value === 'run') {
    loadRunOptions()
  }
})

watch(tab, async (t) => {
  const current = normaliseTab(route.query.action)
  if (current !== t) {
    const query = { ...route.query }
    delete query.action
    if (t !== 'list') {
      query.action = t
    }
    router.replace({ path: route.path, query })
  }

  if (t === 'actions'  && jobDefs.value.length === 0) loadJobDefs()
  if (t === 'run'      && dotJobs.value.length === 0)  loadRunOptions()
})

watch(() => singletonTabDirector.value, async () => {
  if (!isCommonJobs.value) {
    return
  }

  if (tab.value === 'actions') {
    await ensureSingletonTabDirector()
    await loadJobDefs()
  }
  if (tab.value === 'run') {
    await ensureSingletonTabDirector()
    await loadRunOptions()
  }
})
</script>

<style scoped>
.inline-job-filter {
  display: inline-flex;
  align-items: center;
  text-decoration: none;
}

.jobs-list-header {
  gap: 8px;
}

.jobs-list-header__title {
  flex: 0 0 auto;
}

.jobs-list-header__actions {
  gap: 8px;
}

.jobs-list-header__filter {
  flex: 1 1 210px;
  min-width: 180px;
  max-width: 240px;
}

.jobs-list-header__search {
  flex: 1 1 220px;
  min-width: 180px;
  max-width: 260px;
}

.jobs-list-header__countdown {
  opacity: 0.7;
}

.animated-spin {
  animation: hourglass-spin 1.4s ease-in-out infinite;
}

@keyframes hourglass-spin {
  0%   { transform: rotate(0deg); }
  45%  { transform: rotate(0deg); }
  55%  { transform: rotate(180deg); }
  100% { transform: rotate(180deg); }
}
</style>
