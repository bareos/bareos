<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />

    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     :label="t('Job History')"  no-caps />
      <q-tab name="timeline" :label="t('Job Timeline')" no-caps />
      <q-tab name="run"      :label="t('Start Job')"    no-caps data-testid="jobs-tab-run" />
    </q-tabs>

    <q-tab-panels v-model="tab" animated :swipeable="$q.platform.has.touch">

      <!-- ── SHOW ─────────────────────────────────────────────────────────── -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header jobs-list-header row items-center">
            <span class="jobs-list-header__title">{{ t('Job List') }}</span>
            <q-select
              :model-value="jobFilter"
              use-input
              fill-input
              hide-selected
              input-debounce="0"
              dense
              outlined
              clearable
              :options="visibleJobFilterOptions"
              :label="t('Job Name')"
              class="jobs-list-header__filter"
              @update:model-value="setJobFilter"
              @filter="filterJobFilterAutocomplete"
              @blur="applyJobFilterInput()"
              @keydown.enter.prevent="applyJobFilterInput()"
            />
            <q-select
              :model-value="clientFilter"
              use-input
              fill-input
              hide-selected
              input-debounce="0"
              dense
              outlined
              clearable
              :options="visibleClientFilterOptions"
              :label="t('Client')"
              class="jobs-list-header__filter"
              @update:model-value="setClientFilter"
              @filter="filterClientFilterAutocomplete"
              @blur="applyClientFilterInput()"
              @keydown.enter.prevent="applyClientFilterInput()"
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
            <q-input
              v-model="search"
              dense
              outlined
              clearable
              :label="t('Search in results')"
              data-testid="jobs-results-search"
              class="jobs-list-header__search"
            >
              <template #prepend><q-icon name="search" /></template>
              <template #append>
                <q-icon name="info" size="16px" class="cursor-help">
                  <q-tooltip>{{ t('Searches only within the jobs already filtered here.') }}</q-tooltip>
                </q-icon>
              </template>
            </q-input>
            <div class="jobs-list-header__actions row items-center no-wrap">
              <span class="text-white text-caption jobs-list-header__countdown panel-refresh-countdown">
                <span class="panel-refresh-countdown__value">{{ countdown }}s</span>
              </span>
              <q-btn flat dense no-caps icon="restart_alt" color="white"
                     :label="$q.screen.gt.sm ? t('Rerun by Job ID') : undefined"
                     :title="t('Rerun by Job ID')"
                     @click="openRerunJobIdDialog" />
              <q-btn flat round dense icon="refresh" color="white" @click="manualRefresh" />
            </div>
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
            <q-banner v-if="jobsTruncated" dense class="bg-warning text-white">
              {{ t('Showing the first {limit} of {total} matching jobs. Narrow your filter to see the rest.', { limit: formatNumber(maxJobsFetchLimit), total: formatNumber(totalJobs) }) }}
            </q-banner>
            <div v-if="search || jobFilter || clientFilter || (statusFilters?.length ?? 0) || (levelFilters?.length ?? 0) || (typeFilters?.length ?? 0)" class="q-px-md q-pt-sm">
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
                v-if="jobFilter"
                removable
                color="deep-purple"
                text-color="white"
                icon="work"
                class="q-mb-xs q-mr-xs"
                @remove="setJobFilter('')"
              >
                {{ t('Job Name') }}: {{ jobFilter }}
              </q-chip>
              <q-chip
                v-if="clientFilter"
                removable
                color="cyan-8"
                text-color="white"
                icon="computer"
                class="q-mb-xs q-mr-xs"
                @remove="setClientFilter('')"
              >
                {{ t('Client') }}: {{ clientFilter }}
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
              :rows="jobs"
              :columns="columns"
              row-key="scopeKey"
              binary-state-sort
              dense flat
              virtual-scroll
              style="max-height: 70vh"
              :rows-per-page-options="jobsRowsPerPageOptions"
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
                    @click.prevent="setJobFilter(props.value ?? '')"
                  >
                    {{ props.value }}
                  </a>
                </q-td>
              </template>
              <template #body-cell-status="props">
                <q-td :props="props" class="text-center">
                  <span v-if="props.row.status" class="row items-center no-wrap q-gutter-x-xs justify-center">
                    <span
                      v-if="isWaitingStatus(displayJobStatus(props.row))"
                      class="row items-center no-wrap q-gutter-x-xs justify-center cursor-pointer"
                      :title="t('Jump to log')"
                      @click="openJobDetails(props.row, resolveJobLogFocus(props.row.status))"
                    >
                      <q-icon name="hourglass_empty" color="orange-7" size="16px" class="animated-spin" />
                      <span class="text-orange-7 text-caption">{{ displayJobStatus(props.row) }}</span>
                    </span>
                    <JobStatusBadge
                      v-else
                      clickable
                      :status="displayJobStatus(props.row)"
                      @click="openJobDetails(props.row, resolveJobLogFocus(props.row.status))"
                    />
                    <q-btn
                      flat round dense size="sm"
                      icon="filter_alt"
                      :title="`${t('Filter by status')}: ${jobStatusLabel(props.row.status)}`"
                      @click="applyStatusFilter(props.row.status)"
                    />
                  </span>
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-client="props">
                <q-td :props="props">
                  <span v-if="props.value" class="row inline items-center no-wrap q-gutter-x-xs">
                    <a
                      href="#"
                      class="text-primary inline-job-filter"
                      :title="`${t('Client')}: ${props.value}`"
                      @click.prevent="applyClientFilter(props.value)"
                    >
                      {{ props.value }}
                    </a>
                    <q-btn
                      flat
                      round
                      dense
                      size="sm"
                      icon="info"
                      :title="t('Client details')"
                      @click="openClientDetails(props.row)"
                    />
                  </span>
                  <span v-else>—</span>
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
                   <q-btn v-if="canRerunJob(props.row)"
                         flat round dense size="sm" icon="restart_alt" :title="t('Rerun')"
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

      <!-- ── TIMELINE ──────────────────────────────────────────────────────── -->
      <q-tab-panel name="timeline" class="q-pa-none">
        <JobTimeline
          :key="`${activeDirectors.join('\u0000') || 'jobs-timeline'}:${clientFilter}`"
          :directors="activeDirectors"
          :client-filter="clientFilter"
          :client-details-query="buildClientDetailsQuery({
            jobsOrigin: true,
            jobsAction: tab,
            jobsStatus: statusFilters,
            jobsLevel: encodeJobsLevelFilters(levelFilters),
            jobsType: encodeJobsTypeFilters(typeFilters),
            jobsJob: jobFilter,
            jobsClient: clientFilter,
            jobsSearch: search,
          })"
          :job-details-query="timelineJobDetailsQuery"
        />
      </q-tab-panel>

      <!-- ── RUN ───────────────────────────────────────────────────────────── -->
      <q-tab-panel name="run" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header available-jobs-header row items-center">
            <span class="available-jobs-header__title">{{ t('Available Jobs') }}</span>
            <q-space />
            <q-input
              v-model="jobDefsSearch"
              dense
              outlined
              clearable
              dark
              standout
              :placeholder="t('Search jobs')"
              class="available-jobs-header__search"
            >
              <template #prepend><q-icon name="search" /></template>
            </q-input>
            <q-btn flat round dense icon="refresh" color="white" @click="loadJobDefs" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table
              :rows="filteredJobDefs"
              :columns="defsColumns"
              row-key="scopeKey"
              dense flat
              :loading="loadingDefs"
              v-model:pagination="jobDefsPagination"
              :rows-per-page-options="jobDefsRowsPerPageOptions"
              @row-click="openConfigureStartJobFromRow"
            >
              <template #body-cell-name="props">
                <q-td :props="props">
                  <span :class="{ 'text-grey-6': !props.row.enabled }">{{ props.value }}</span>
                </q-td>
              </template>
              <template #body-cell-director="props">
                <q-td :props="props">
                  <DirectorLabel :director="props.row.director || props.value || ''" />
                </q-td>
              </template>
              <template #body-cell-type="props">
                <q-td :props="props" class="text-center">
                  <JobTypeBadge v-if="props.value" :type="props.value" />
                  <span v-else>—</span>
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center" @click.stop>
                  <q-toggle
                    :model-value="props.value"
                    :label="props.value ? t('enabled') : t('disabled')"
                    :disable="isJobEnabledSwitchLoading(props.row)"
                    color="positive"
                    dense
                    keep-color
                    @update:model-value="setJobEnabled(props.row, $event)"
                  />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center" style="white-space:nowrap" @click.stop>
                  <q-btn flat dense no-caps size="sm" icon="tune" color="primary"
                         :round="$q.screen.lt.md"
                         :label="$q.screen.lt.md ? undefined : t('Customize & Start')" class="q-mr-xs"
                         @click.stop="openConfigureStartJob(props.row)">
                    <q-tooltip>{{ t('Customize & Start') }}</q-tooltip>
                  </q-btn>
                  <span>
                    <q-btn flat dense no-caps size="sm" icon="rocket_launch"
                         :color="props.row.enabled ? 'positive' : 'grey-6'"
                         :round="$q.screen.lt.md"
                         :label="$q.screen.lt.md ? undefined : t('Quick start')" class="q-mr-xs"
                         :disable="!props.row.enabled"
                         @click.stop="confirmQuickStartJob(props.row)" />
                    <q-tooltip>
                      {{ props.row.enabled ? t('Quick start') : t('Enable this job first.') }}
                    </q-tooltip>
                  </span>
                </q-td>
              </template>
            </q-table>
          </q-card-section>
          <q-card-section class="text-caption text-grey-7">
            {{ t('Restore job templates are handled by the Restore workflow.') }}
            <a href="#" class="text-primary" @click.prevent="openRestorePage">
              {{ t('Open Restore') }}
            </a>
          </q-card-section>
        </q-card>
      </q-tab-panel>

    </q-tab-panels>

    <q-dialog v-model="rerunJobIdDialogOpen">
      <q-card class="rerun-job-id-dialog">
        <q-card-section class="panel-header row items-center">
          <span>{{ t('Rerun by Job ID') }}</span>
          <q-space />
          <q-btn v-close-popup flat round dense icon="close" color="white" />
        </q-card-section>
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
              :hint="t('Enter the ID of any completed job from the selected directors')"
              :error="Boolean(rerunJobIdError)"
              :error-message="rerunJobIdError"
            />
            <div class="row justify-end q-gutter-sm">
              <q-btn v-close-popup flat no-caps :label="t('Cancel')" />
              <q-btn type="submit" color="primary" :label="t('Rerun')" icon="restart_alt"
                     no-caps :loading="rerunLoading" :disable="!rerunJobIdValue" />
            </div>
          </q-form>
        </q-card-section>
      </q-card>
    </q-dialog>

    <q-dialog v-model="configureStartDialogOpen">
      <q-card class="configure-start-dialog">
        <q-card-section class="panel-header row items-center">
          <span>{{ t('Customize & Start') }}</span>
          <q-space />
          <q-btn v-close-popup flat round dense icon="close" color="white" />
        </q-card-section>

        <q-card-section v-if="isCommonJobs" class="q-pb-none">
          <q-select
            v-model="singletonTabDirector"
            :options="visibleSingletonTabDirectorOptions"
            option-label="label"
            option-value="value"
            emit-value
            map-options
            use-input
            fill-input
            hide-selected
            input-debounce="0"
            outlined
            dense
            :label="t('Director')"
            @filter="filterSingletonTabDirectorOptions"
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
            {{ t('The start form and defaults lookups in this tab use the selected director while the jobs list stays aggregated.') }}
          </div>
        </q-card-section>

        <q-card-section class="q-pt-none q-pb-none">
          <div class="text-subtitle2">{{ t('Effective defaults') }}</div>
          <q-markup-table flat bordered dense class="q-mt-sm">
            <tbody>
              <tr v-for="row in runFormSummaryRows" :key="row.label">
                <td class="text-grey-7">{{ row.label }}</td>
                <td>{{ row.value }}</td>
              </tr>
            </tbody>
          </q-markup-table>
        </q-card-section>

        <q-card-section>
          <q-form @submit.prevent="runJob" class="q-gutter-md">
            <q-select v-model="runForm.job"     data-testid="run-job-field" :options="visibleDotJobs"     :label="t('Job *')"     outlined dense use-input fill-input hide-selected input-debounce="0"
                      @filter="filterRunJobs"
                      @update:model-value="onJobSelected" />
            <q-select v-model="runForm.client"  :options="visibleDotClients"  :label="t('Client')"  outlined dense clearable use-input fill-input hide-selected input-debounce="0" @filter="filterRunClients" />
            <q-select v-model="runForm.fileset" :options="visibleDotFilesets" :label="t('Fileset')" outlined dense clearable use-input fill-input hide-selected input-debounce="0" @filter="filterRunFilesets" />
            <q-select v-model="runForm.pool"    :options="visibleDotPools"    :label="t('Pool')"    outlined dense clearable use-input fill-input hide-selected input-debounce="0" @filter="filterRunPools" />
            <q-select v-model="runForm.storage" :options="visibleDotStorages" :label="t('Storage')" outlined dense clearable use-input fill-input hide-selected input-debounce="0" @filter="filterRunStorages" />
            <q-select v-model="runForm.level"   :options="visibleLevels"      :label="t('Level')"   outlined dense use-input fill-input hide-selected input-debounce="0" @filter="filterRunLevels" />
            <q-input  v-model="runForm.when"                           :label="t('When (optional)')" outlined dense
                      :placeholder="t('YYYY-MM-DD HH:MM:SS')">
              <template #append>
                <q-icon name="event" class="cursor-pointer">
                  <q-popup-proxy @before-show="prepareRunWhenPicker">
                    <div class="q-pa-md column q-gutter-md">
                      <q-date v-model="runWhenPickerValue" mask="YYYY-MM-DD HH:mm:ss" />
                      <q-time v-model="runWhenPickerValue" mask="YYYY-MM-DD HH:mm:ss" format24h with-seconds />
                      <div class="row justify-end q-gutter-sm">
                        <q-btn flat no-caps :label="t('Clear')" v-close-popup @click="clearRunWhen" />
                        <q-btn flat no-caps :label="t('Cancel')" v-close-popup />
                        <q-btn color="primary" no-caps :label="t('OK')" v-close-popup @click="applyRunWhenPicker" />
                      </div>
                    </div>
                  </q-popup-proxy>
                </q-icon>
              </template>
            </q-input>
            <q-input  v-model.number="runForm.priority" type="number" :label="t('Priority')" outlined dense style="max-width:140px" />
            <div class="row justify-end q-gutter-sm">
              <q-btn v-close-popup flat no-caps :label="t('Cancel')" />
              <q-btn data-testid="run-job-submit" type="submit" color="primary" :label="t('Start Job')" icon="play_arrow"
                     no-caps :loading="runLoading" :disable="!runForm.job" />
            </div>
          </q-form>
        </q-card-section>
      </q-card>
    </q-dialog>
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
} from '../composables/jobsAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { usePersistedTablePagination } from '../composables/usePersistedTablePagination.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { buildClientDetailsQuery } from '../utils/clients.js'
import { formatNumber } from '../utils/locales.js'
import { resolveJobTypeCode, resolveJobTypeInfo } from '../utils/jobTypes.js'
import {
  buildCancelJobCommand,
  buildJobDefaultsCommand,
  buildJobDetailsQuery,
  buildListJobCommand,
  buildListJobsCommand,
  buildListJobsCountCommand,
  buildRerunJobCommand,
  buildRunJobCommand,
  buildSetJobEnabledCommand,
  canRerunJob,
  encodeJobsLevelFilters,
  encodeJobsStatusFilters,
  encodeJobsTypeFilters,
  filterRunnableJobOptions,
  filterJobsTextOptions,
  formatRunWhenPickerDate,
  MAX_JOBS_FETCH_LIMIT,
  normaliseJobId,
  normaliseJobsSearchTerm,
  normaliseJobsTextOptions,
  normaliseJobsTextFilter,
  paginateJobs,
  resolveJobsSortColumn,
  resolvePermittedRunJobDefault,
  resolveRunWhenPickerValue,
  resolveJobsClientQuery,
  resolveJobsJobQuery,
  resolveJobsLevelFilters,
  resolveJobsSearchQuery,
  resolveJobsStatusFilters,
  resolveJobsTypeFilters,
  resolveConfiguredJobType,
  resolveJobLogFocus,
  withJobsClientQuery,
  withJobsJobQuery,
  withJobsSearchQuery,
  withJobsLevelFilterQuery,
  withJobsStatusFilterQuery,
  withJobsTypeFilterQuery,
} from '../utils/jobs.js'
import DirectorLabel from '../components/DirectorLabel.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'
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
const validTabs = new Set(['list', 'run', 'timeline'])
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

function jobsTextFiltersEqual(left, right) {
  return normaliseJobsTextFilter(left) === normaliseJobsTextFilter(right)
}

const tab          = ref(normaliseTab(route.query.action))
const search       = ref(resolveJobsSearchQuery(route.query))
const statusFilters = ref(resolveJobsStatusFilters(route.query))
const levelFilters = ref(resolveJobsLevelFilters(route.query))
const typeFilters = ref(resolveJobsTypeFilters(route.query))
const jobFilter = ref(resolveJobsJobQuery(route.query))
const clientFilter = ref(resolveJobsClientQuery(route.query))
const jobFilterInput = ref(jobFilter.value)
const clientFilterInput = ref(clientFilter.value)
const jobFilterOptions = ref([])
const clientFilterOptions = ref([])
const directorErrors = ref([])
const fmtBytes  = formatBytes
const maxJobsFetchLimit = MAX_JOBS_FETCH_LIMIT
const fmtSpeed  = formatSpeed
const jobsRowsPerPageOptions = [10, 25, 50]
const jobDefsRowsPerPageOptions = [10, 15, 25, 50]
const pagination = usePersistedTablePagination('jobs.list', {
  page: 1,
  rowsPerPage: 25,
  sortBy: 'id',
  descending: true,
  rowsNumber: 0,
}, { allowedRowsPerPage: jobsRowsPerPageOptions })
const jobDefsPagination = usePersistedTablePagination('jobs.defs', {
  page: 1,
  rowsPerPage: 15,
  sortBy: 'name',
  descending: false,
}, {
  allowedRowsPerPage: jobDefsRowsPerPageOptions,
  persistSort: true,
})
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
const visibleJobFilterOptions = computed(() => filterJobsTextOptions([
  ...jobFilterOptions.value,
  ...jobs.value.map(job => job.name),
], jobFilterInput.value))
const visibleClientFilterOptions = computed(() => filterJobsTextOptions([
  ...clientFilterOptions.value,
  ...jobs.value.map(job => job.client),
], clientFilterInput.value))

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

function canRestoreFromJob(job) {
  return resolveJobTypeCode(job?.type) !== 'R'
}

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonJobs,
  isSingleDirectorScope,
  scopeLabel: jobsScopeLabel,
  syncSelectedDirectors,
  ensureScopeDirector,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

const showDirectorColumn = computed(() => activeDirectors.value.length > 1)
const singletonTabDirector = ref('')
const singletonTabDirectorOptions = computed(() => (
  activeDirectors.value.map(value => ({ label: value, value }))
))
const visibleSingletonTabDirectorOptions = ref([])
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
  jobsJob: jobFilter.value,
  jobsClient: clientFilter.value,
  jobsSearch: search.value,
}))

function syncSingletonTabDirector() {
  const validDirectors = activeDirectors.value
  visibleSingletonTabDirectorOptions.value = [...singletonTabDirectorOptions.value]
  if (!validDirectors.length) {
    singletonTabDirector.value = ''
    return
  }

  if (validDirectors.includes(singletonTabDirector.value)) {
    return
  }

  singletonTabDirector.value = validDirectors[0]
}

function filterSingletonTabDirectorOptions(value, update) {
  const needle = String(value ?? '').trim().toLowerCase()
  update(() => {
    visibleSingletonTabDirectorOptions.value = singletonTabDirectorOptions.value.filter(
      option => !needle || option.label.toLowerCase().includes(needle)
    )
  })
}

async function ensureSingletonTabDirector() {
  const targetDirector = currentSingletonDirector.value
  if (!targetDirector) {
    return null
  }

  await ensureScopeDirector(targetDirector)
  return targetDirector
}

function syncJobsScopeDirectorQuery() {
  if (typeof route.query.scopeDirector !== 'string' || !route.query.scopeDirector) {
    return
  }

  const requestedDirector = route.query.scopeDirector
  const requestedDirectorIsAvailable = directorOptions.value.some(
    option => option.value === requestedDirector
  )
  if (requestedDirectorIsAvailable) {
    settings.setSelectedDirectors([requestedDirector])
  }

  const query = { ...route.query }
  delete query.scopeDirector
  router.replace({ path: route.path, query })
}

// ── paginated job list ────────────────────────────────────────────────────────
const jobs       = ref([])
const totalJobs  = ref(0)
const jobsTruncated = ref(false)
const loading    = ref(false)
const error      = ref(null)
async function fetchPage() {
  loading.value = true
  error.value   = null
  directorErrors.value = []
  const currentPagination = pagination.value
  const { page, rowsPerPage, sortBy, descending } = currentPagination
  const offset = (page - 1) * rowsPerPage
  const encodedStatusFilters = encodeJobsStatusFilters(statusFilters.value)
  const normalizedSearchTerm = normaliseJobsSearchTerm(search.value)
  // Sorting and searching are resolved on the director whenever the sort
  // column has a real catalog equivalent (see the `sortby=` whitelist in
  // core/src/cats/sql_list.cc), so the browser only ever fetches
  // `rowsPerPage` rows. `duration`/`speed` (derived client-side) and
  // `director` (n/a for a single director) have no server-side
  // equivalent, and `rowsPerPage === 0` ("All", no longer reachable from
  // the UI but still defended against) has no bounded page to request in
  // the first place -- both fall back to a capped, client-sorted fetch
  // further down.
  const serverSortColumn = rowsPerPage > 0 ? resolveJobsSortColumn(sortBy) : null
  try {
    if (activeDirectors.value.length === 0) {
      jobs.value = []
      totalJobs.value = 0
      jobsTruncated.value = false
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
          jobFilter.value,
          clientFilter.value,
          normalizedSearchTerm,
        )
      jobs.value = result.jobs
      directorErrors.value = result.directorErrors
      totalJobs.value = result.totalJobs
      jobsTruncated.value = result.truncated
      pagination.value = { ...pagination.value, rowsNumber: result.totalJobs }
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    // A single combined query is used regardless of how many statuses are
    // selected: `jobstatus=T,E,f` is sent as-is and the director resolves it
    // natively via `Job.JobStatus IN (...)` (see sql_list.cc). Splitting this
    // into one director call per status used to multiply catalog/director
    // load for no benefit.
    const countResult = await director.call(buildListJobsCountCommand({
      statusFilter: encodedStatusFilters,
      levelFilter: levelFilters.value,
      typeFilter: typeFilters.value,
      jobFilter: jobFilter.value,
      clientFilter: clientFilter.value,
      searchTerm: normalizedSearchTerm,
    }))
    const count = Number(directorCollection(countResult?.jobs)[0]?.count ?? 0)
    if (count === 0) {
      jobs.value = []
      totalJobs.value = 0
      jobsTruncated.value = false
      pagination.value = { ...pagination.value, rowsNumber: 0 }
      return
    }
    // "Fetch everything" is now limited to the rare columns without a
    // server-side sort (duration/speed): those are still capped so a very
    // large job history never ships its entire table to the browser.
    const truncated = !serverSortColumn && count > MAX_JOBS_FETCH_LIMIT
    const limit = serverSortColumn
      ? rowsPerPage
      : Math.min(count, MAX_JOBS_FETCH_LIMIT)
    const pageResult = await director.call(
      buildListJobsCommand({
        limit,
        offset: serverSortColumn ? offset : 0,
        statusFilter: encodedStatusFilters,
        levelFilter: levelFilters.value,
        typeFilter: typeFilters.value,
        jobFilter: jobFilter.value,
        clientFilter: clientFilter.value,
        sortColumn: serverSortColumn ?? '',
        descending,
        searchTerm: normalizedSearchTerm,
      })
    )
    const fetchedJobs = directorCollection(pageResult?.jobs).map((job) => {
      const normalized = normaliseJob(job)
      return {
        ...normalized,
        director: currentDirector,
        scopeKey: `${currentDirector}:${normalized.id}`,
      }
    })
    const visibleJobs = serverSortColumn
      ? fetchedJobs
      : paginateJobs(sortJobsByPagination(fetchedJobs, currentPagination), currentPagination)
    const runningJobIds = visibleJobs.filter(job => isRunning(job.status)).map(job => job.id)
    if (runningJobIds.length > 0) {
      const runtimeStatus = await Promise.allSettled([director.call('status director')])
      jobs.value = runtimeStatus[0].status === 'fulfilled'
        ? overlayRuntimeStatuses(visibleJobs, runtimeStatus[0].value?.running)
        : visibleJobs
    } else {
      jobs.value = visibleJobs
    }
    totalJobs.value = count
    jobsTruncated.value = truncated
    pagination.value = { ...pagination.value, rowsNumber: count }
  } catch (e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

function refresh() { fetchPage() }

function onRequest(props) {
  pagination.value = { ...pagination.value, ...props.pagination }
  fetchPage()
}

function maxOf(values) {
  return values.reduce((max, value) => (value > max ? value : max), 1)
}

const maxBytes    = computed(() => maxOf(jobs.value.map(j => j.bytes)))
const maxFiles    = computed(() => maxOf(jobs.value.map(j => j.files)))
const maxDuration = computed(() => maxOf(jobs.value.map(j => parseDurationSecs(j.duration))))

function bytesGauge(val)    { return (val || 0) / maxBytes.value }
function filesGauge(val)    { return (val || 0) / maxFiles.value }
function durationGauge(str) { return parseDurationSecs(str) / maxDuration.value }

function jobSpeedBps(row) {
  const secs = parseDurationSecs(row.duration)
  if (!secs) return 0
  const bytes = typeof row.bytes === 'string' ? parseFloat(row.bytes) : (row.bytes || 0)
  return bytes / secs
}
const maxSpeed = computed(() => maxOf(jobs.value
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

function setJobFilter(value) {
  const normalized = normaliseJobsTextFilter(value)
  if (!jobsTextFiltersEqual(jobFilter.value, normalized)) {
    jobFilter.value = normalized
  }
  if (!jobsTextFiltersEqual(jobFilterInput.value, normalized)) {
    jobFilterInput.value = normalized
  }
}

function setClientFilter(value) {
  const normalized = normaliseJobsTextFilter(value)
  if (!jobsTextFiltersEqual(clientFilter.value, normalized)) {
    clientFilter.value = normalized
  }
  if (!jobsTextFiltersEqual(clientFilterInput.value, normalized)) {
    clientFilterInput.value = normalized
  }
}

function applyJobFilterInput() {
  setJobFilter(jobFilterInput.value)
}

function applyClientFilterInput() {
  setClientFilter(clientFilterInput.value)
}

function filterJobFilterAutocomplete(value, update) {
  jobFilterInput.value = normaliseJobsTextFilter(value)
  update(() => {})
}

function filterClientFilterAutocomplete(value, update) {
  clientFilterInput.value = normaliseJobsTextFilter(value)
  update(() => {})
}

function applyClientFilter(client) {
  if (!client) {
    return
  }

  setClientFilter(client)
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

const defsColumns = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: 'Director', field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',    label: 'Job Name', field: 'name',    align: 'left',   sortable: true },
  { name: 'type',    label: 'Type',     field: 'type',    align: 'center', sortable: true },
  { name: 'enabled', label: 'Status',   field: 'enabled', align: 'center', sortable: true },
  { name: 'actions', label: '',         field: 'actions', align: 'center', style: 'width:260px' },
].map((col) => ({ ...col, label: col.label ? t(col.label) : col.label })))

// ── job definitions (for start and enable/disable) ────────────────────────────
const jobDefs       = ref([])
const loadingDefs   = ref(false)
const enabledSwitchLoadingScopeKeys = ref(new Set())
const jobDefsSearch = ref('')
const filteredJobDefs = computed(() => {
  const needle = normaliseJobsSearchTerm(jobDefsSearch.value).toLowerCase()
  if (!needle) {
    return jobDefs.value
  }

  return jobDefs.value.filter((job) => [
    job.name,
    job.director,
    job.type,
    jobTypeLabel(job.type),
    job.enabled ? t('enabled') : t('disabled'),
  ].some(value => String(value ?? '').toLowerCase().includes(needle)))
})

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
        const [res, jobDefsResponse] = await Promise.all([
          client.call('show jobs'),
          client.call('show jobdefs'),
        ])
        const raw = res?.jobs ?? {}
        const list = Array.isArray(raw) ? raw : Object.values(raw)
        const jobDefs = jobDefsResponse?.jobdefs ?? {}
        return list
          .map((job) => {
            const type = resolveConfiguredJobType(job, jobDefs)
            return {
              name: job.name,
              type,
              enabled: job.enabled !== false,
              director: directorName,
              scopeKey: `${directorName}:${job.name}`,
            }
          })
          .filter(job => resolveJobTypeCode(job.type) !== 'R')
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

async function loadFilterOptions() {
  try {
    const credentials = auth.getCredentials()
    if (!credentials?.password || activeDirectors.value.length === 0) {
      jobFilterOptions.value = []
      clientFilterOptions.value = []
      return
    }

    const results = await Promise.all(activeDirectors.value.map(async (directorName) => {
      const client = await createDirectorCommandClient({
        ...credentials,
        director: directorName,
      })
      try {
        const [jobsResult, clientsResult] = await Promise.allSettled([
          client.call('.jobs'),
          client.call('.clients'),
        ])

        return {
          jobs: jobsResult.status === 'fulfilled'
            ? directorCollection(jobsResult.value?.jobs).map(job => job.name)
            : [],
          clients: clientsResult.status === 'fulfilled'
            ? directorCollection(clientsResult.value?.clients).map(currentClient => currentClient.name)
            : [],
        }
      } finally {
        client.disconnect()
      }
    }))

    jobFilterOptions.value = normaliseJobsTextOptions(results.flatMap(result => result.jobs))
    clientFilterOptions.value = normaliseJobsTextOptions(results.flatMap(result => result.clients))
  } catch (e) {
    console.warn('Could not load jobs filter options:', e.message)
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

async function openJobDetails(job, logFocus) {
  await router.push({
    name: 'job-details',
    params: { id: job.id },
    query: buildJobDetailsQuery({
      director: job.director,
      jobsAction: tab.value,
      jobsStatus: statusFilters.value,
      jobsLevel: levelFilters.value,
      jobsType: typeFilters.value,
      jobsJob: jobFilter.value,
      jobsClient: clientFilter.value,
      jobsSearch: search.value,
      logFocus,
    }),
  })
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
        jobsJob: jobFilter.value,
        jobsClient: clientFilter.value,
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

async function openRestorePage() {
  await router.push({ name: 'restore' })
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
    await director.call(buildCancelJobCommand(job.id))
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
      const lookup = await director.call(buildListJobCommand(jobId))
      if (directorCollection(lookup?.jobs).length === 0) {
        throw new Error(`${t('Job')} ${jobId} ${t('was not found.')}`)
      }
    }

    const res = await director.call(buildRerunJobCommand(jobId))
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `${t('Job restarted as ID')} ${newId}.` })
    tab.value = 'list'
    refresh()
    return true
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Rerun failed')}: ${e.message}` })
    return false
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
        return director.call(buildCancelJobCommand(j.id))
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

function isJobEnabledSwitchLoading(job) {
  return enabledSwitchLoadingScopeKeys.value.has(job?.scopeKey)
}

async function setJobEnabled(job, enabled) {
  const scopeKey = job?.scopeKey
  if (!scopeKey || isJobEnabledSwitchLoading(job) || job.enabled === enabled) {
    return
  }

  enabledSwitchLoadingScopeKeys.value = new Set([
    ...enabledSwitchLoadingScopeKeys.value,
    scopeKey,
  ])

  try {
    await switchToJobDirector(job)
    await director.call(buildSetJobEnabledCommand(job.name, enabled))
    const j = jobDefs.value.find(d => d.scopeKey === job.scopeKey)
    if (j) j.enabled = enabled
    $q.notify({
      type: 'positive',
      message: enabled
        ? `Job "${job.name}" enabled.`
        : `Job "${job.name}" disabled.`,
    })
  } catch (e) {
    $q.notify({
      type: 'negative',
      message: `${enabled ? 'Enable' : 'Disable'} failed: ${e.message}`,
    })
  } finally {
    const nextLoadingScopeKeys = new Set(enabledSwitchLoadingScopeKeys.value)
    nextLoadingScopeKeys.delete(scopeKey)
    enabledSwitchLoadingScopeKeys.value = nextLoadingScopeKeys
  }
}

function defaultRunForm(jobName = null) {
  return {
    job: jobName,
    client: null,
    fileset: null,
    pool: null,
    storage: null,
    level: 'Incremental',
    when: '',
    priority: 10,
  }
}

async function prepareRunFormForJob(job) {
  selectedConfiguredJob.value = job ?? null
  if (job?.director && activeDirectors.value.includes(job.director)) {
    singletonTabDirector.value = job.director
  }
  await switchToJobDirector(job)
  await loadRunOptions({ throwOnError: true })
  runForm.value = defaultRunForm(job.name)
  await applyJobDefaults(job.name, { throwOnError: true })
  return { ...runForm.value }
}

async function openConfigureStartJob(job) {
  try {
    await prepareRunFormForJob(job)
    configureStartDialogOpen.value = true
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Could not load job defaults')}: ${e.message}` })
  }
}

function openConfigureStartJobFromRow(_event, row) {
  openConfigureStartJob(row)
}

async function confirmQuickStartJob(job) {
  let preparedForm
  try {
    preparedForm = await prepareRunFormForJob(job)
  } catch (e) {
    $q.notify({ type: 'negative', message: `${t('Could not load job defaults')}: ${e.message}` })
    return
  }

  $q.dialog({
    title: t('Quick Start Job'),
    message: `${t('Start job')} <b>${escapeHtml(job.name)}</b>?${runFormSummaryHtml(preparedForm)}`,
    html: true,
    ok:     { label: t('Start Job'), color: 'primary', flat: true },
    cancel: { label: t('Cancel'), flat: true },
  }).onOk(() => submitRunJob(preparedForm))
}

// ── run form ──────────────────────────────────────────────────────────────────
const dotJobs     = ref([])
const dotClients  = ref([])
const dotFilesets = ref([])
const dotPools    = ref([])
const dotStorages = ref([])
const levels      = ['Full', 'Incremental', 'Differential']
const visibleDotJobs = ref([])
const visibleDotClients = ref([])
const visibleDotFilesets = ref([])
const visibleDotPools = ref([])
const visibleDotStorages = ref([])
const visibleLevels = ref([...levels])
const runLoading  = ref(false)

const runForm = ref(defaultRunForm())
const configureStartDialogOpen = ref(false)
const selectedConfiguredJob = ref(null)
const runWhenPickerValue = ref(formatRunWhenPickerDate(new Date()))
const runFormSummaryRows = computed(() => runFormSummary(runForm.value))

function displayRunFormValue(value) {
  const normalized = String(value ?? '').trim()
  return normalized || '—'
}

function runFormSummary(form, job = selectedConfiguredJob.value) {
  return [
    { label: t('Director'), value: displayRunFormValue(currentSingletonDirector.value) },
    { label: t('Job'), value: displayRunFormValue(form?.job) },
    { label: t('Type'), value: displayRunFormValue(jobTypeLabel(job?.type)) },
    { label: t('Status'), value: displayRunFormValue(job ? (job.enabled ? t('enabled') : t('disabled')) : '') },
    { label: t('Client'), value: displayRunFormValue(form?.client) },
    { label: t('Fileset'), value: displayRunFormValue(form?.fileset) },
    { label: t('Pool'), value: displayRunFormValue(form?.pool) },
    { label: t('Storage'), value: displayRunFormValue(form?.storage) },
    { label: t('Level'), value: displayRunFormValue(form?.level) },
    { label: t('When'), value: displayRunFormValue(form?.when) },
    { label: t('Priority'), value: displayRunFormValue(form?.priority) },
  ]
}

function runFormSummaryHtml(form) {
  const rows = runFormSummary(form)
    .map(row => `<tr><td>${escapeHtml(row.label)}</td><td>${escapeHtml(row.value)}</td></tr>`)
    .join('')
  return `<div class="q-mt-md"><b>${escapeHtml(t('Effective defaults'))}</b><table>${rows}</table></div>`
}

function prepareRunWhenPicker() {
  runWhenPickerValue.value = resolveRunWhenPickerValue(runForm.value.when)
}

function applyRunWhenPicker() {
  runForm.value.when = runWhenPickerValue.value
}

function clearRunWhen() {
  runForm.value.when = ''
}

function matchingRunOptions(options, value) {
  const needle = String(value ?? '').trim().toLowerCase()
  if (!needle) {
    return [...options]
  }

  return options.filter(option => String(option).toLowerCase().includes(needle))
}

function filterRunOptions(options, visibleOptions, value, update) {
  update(() => {
    visibleOptions.value = matchingRunOptions(options, value)
  })
}

function filterRunJobs(value, update) {
  filterRunOptions(dotJobs.value, visibleDotJobs, value, update)
}

function filterRunClients(value, update) {
  filterRunOptions(dotClients.value, visibleDotClients, value, update)
}

function filterRunFilesets(value, update) {
  filterRunOptions(dotFilesets.value, visibleDotFilesets, value, update)
}

function filterRunPools(value, update) {
  filterRunOptions(dotPools.value, visibleDotPools, value, update)
}

function filterRunStorages(value, update) {
  filterRunOptions(dotStorages.value, visibleDotStorages, value, update)
}

function filterRunLevels(value, update) {
  filterRunOptions(levels, visibleLevels, value, update)
}

async function loadRunOptions({ throwOnError = false } = {}) {
  try {
    await ensureSingletonTabDirector()
    const [j, restoreJobs, c, f, p, s] = await Promise.all([
      director.call('.jobs'),
      director.call('.jobs type=R'),
      director.call('.clients'),
      director.call('.filesets'),
      director.call('.pools'),
      director.call('.storage'),
    ])
    dotJobs.value = filterRunnableJobOptions(
      directorCollection(j?.jobs).map(x => x.name),
      directorCollection(restoreJobs?.jobs).map(x => x.name)
    )
    dotClients.value = directorCollection(c?.clients).map(x => x.name).sort()
    dotFilesets.value = directorCollection(f?.filesets).map(x => x.name).sort()
    dotPools.value = directorCollection(p?.pools).map(x => x.name).sort()
    dotStorages.value = directorCollection(s?.storages).map(x => x.name).sort()
    visibleDotJobs.value = [...dotJobs.value]
    visibleDotClients.value = [...dotClients.value]
    visibleDotFilesets.value = [...dotFilesets.value]
    visibleDotPools.value = [...dotPools.value]
    visibleDotStorages.value = [...dotStorages.value]
    visibleLevels.value = [...levels]
  } catch (e) {
    if (throwOnError) {
      throw e
    }
    // non-fatal — user can still type values
    console.warn('Could not load run form options:', e.message)
  }
}

async function onJobSelected(name) {
  await applyJobDefaults(name)
}

async function applyJobDefaults(name, { throwOnError = false } = {}) {
  if (!name) return
  try {
    await ensureSingletonTabDirector()
    const res = await director.call(buildJobDefaultsCommand(name))
    const d   = res?.defaults ?? res ?? {}
    if (Object.hasOwn(d, 'client')) {
      runForm.value.client = resolvePermittedRunJobDefault(dotClients.value, d.client)
    }
    if (Object.hasOwn(d, 'fileset')) {
      runForm.value.fileset = resolvePermittedRunJobDefault(dotFilesets.value, d.fileset)
    }
    if (Object.hasOwn(d, 'pool')) {
      runForm.value.pool = resolvePermittedRunJobDefault(dotPools.value, d.pool)
    }
    if (Object.hasOwn(d, 'storage')) {
      runForm.value.storage = resolvePermittedRunJobDefault(dotStorages.value, d.storage)
    }
    if (d.level)    runForm.value.level    = d.level
    if (d.priority) runForm.value.priority = Number(d.priority)
  } catch (e) {
    if (throwOnError) {
      throw e
    }
  }
}

async function submitRunJob(form) {
  const f = form
  if (!f.job) return
  runLoading.value = true
  try {
    await ensureSingletonTabDirector()
    const res = await director.call(buildRunJobCommand(f))
    const newId = res?.run?.jobid ?? res?.jobid ?? '?'
    $q.notify({ type: 'positive', message: `Job started — ID ${newId}` })
    configureStartDialogOpen.value = false
    tab.value = 'list'
    refresh()
  } catch (e) {
    $q.notify({ type: 'negative', message: `Run failed: ${e.message}` })
  } finally {
    runLoading.value = false
  }
}

async function runJob() {
  await submitRunJob({ ...runForm.value })
}

// ── manual rerun dialog ───────────────────────────────────────────────────────
const rerunJobId   = ref('')
const rerunLoading = ref(false)
const rerunJobIdDialogOpen = ref(false)
const rerunJobIdValue = computed(() => normaliseJobId(rerunJobId.value))
const rerunJobIdError = computed(() => {
  if (rerunJobId.value === '' || rerunJobId.value === null) {
    return ''
  }

  return rerunJobIdValue.value === null ? t('Enter a positive Job ID.') : ''
})

function openRerunJobIdDialog() {
  rerunJobIdDialogOpen.value = true
}

async function submitRerun() {
  if (rerunJobIdValue.value === null) {
    $q.notify({ type: 'negative', message: t('Enter a positive Job ID.') })
    return
  }

  rerunLoading.value = true
  try {
    const listedMatches = jobs.value.filter(job => (
      Number(job.id) === rerunJobIdValue.value && canRerunJob(job)
    ))
    if (listedMatches.length === 1) {
      if (await doRerun(listedMatches[0])) {
        rerunJobId.value = ''
        rerunJobIdDialogOpen.value = false
      }
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
        const lookup = await client.call(buildListJobCommand(rerunJobIdValue.value))
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

    if (await doRerun(matches[0])) {
      rerunJobId.value = ''
      rerunJobIdDialogOpen.value = false
    }
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
    // Skip the tick while a fetch is already in flight or the tab isn't
    // visible, so a slow response can't overlap with the next auto-refresh
    // and the page doesn't keep polling in the background.
    if (loading.value || document.hidden) {
      return
    }
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
  loadFilterOptions()
  fetchPage()
  if (director.isConnected && (isSingleDirectorScope.value || tab.value === 'run')) {
    loadJobDefs()
  }
  if (director.isConnected && (isSingleDirectorScope.value || tab.value === 'run')) {
    loadRunOptions()
  }
  startAutoRefresh()
})

onUnmounted(() => {
  jobFilterOptions.value = []
  clientFilterOptions.value = []
  stopAutoRefresh()
})

watch(() => director.isConnected, (connected) => {
  if (connected) {
    loadFilterOptions()
    fetchPage()
    if (isSingleDirectorScope.value || tab.value === 'run') {
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

watch(() => route.query.job, (value) => {
  const next = resolveJobsJobQuery({ job: value })
  if (!jobsTextFiltersEqual(jobFilter.value, next)) {
    jobFilter.value = next
  }
  if (!jobsTextFiltersEqual(jobFilterInput.value, next)) {
    jobFilterInput.value = next
  }
})

watch(() => route.query.client, (value) => {
  const next = resolveJobsClientQuery({ client: value })
  if (!jobsTextFiltersEqual(clientFilter.value, next)) {
    clientFilter.value = next
  }
  if (!jobsTextFiltersEqual(clientFilterInput.value, next)) {
    clientFilterInput.value = next
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
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
})

watch(jobFilterInput, (next) => {
  if (normaliseJobsTextFilter(next) === '' && !jobsTextFiltersEqual(jobFilter.value, '')) {
    setJobFilter('')
  }
})

watch(clientFilterInput, (next) => {
  if (normaliseJobsTextFilter(next) === '' && !jobsTextFiltersEqual(clientFilter.value, '')) {
    setClientFilter('')
  }
})

watch(jobFilter, (next) => {
  const normalized = normaliseJobsTextFilter(next)
  if (!jobsTextFiltersEqual(next, normalized)) {
    jobFilter.value = normalized
    return
  }

  const query = withJobsJobQuery(route.query, normalized)
  if ((query.job ?? '') !== (typeof route.query.job === 'string' ? route.query.job : '')) {
    router.replace({ path: route.path, query })
  }
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
})

watch(clientFilter, (next) => {
  const normalized = normaliseJobsTextFilter(next)
  if (!jobsTextFiltersEqual(next, normalized)) {
    clientFilter.value = normalized
    return
  }

  const query = withJobsClientQuery(route.query, normalized)
  if ((query.client ?? '') !== (typeof route.query.client === 'string' ? route.query.client : '')) {
    router.replace({ path: route.path, query })
  }
  pagination.value = { ...pagination.value, page: 1 }
  fetchPage()
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
   loadFilterOptions()
  fetchPage()
  if (tab.value === 'run') {
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

  if (t === 'run' && jobDefs.value.length === 0) loadJobDefs()
  if (t === 'run' && dotJobs.value.length === 0)  loadRunOptions()
})

watch(() => singletonTabDirector.value, async () => {
  if (!isCommonJobs.value) {
    return
  }

  if (tab.value === 'run') {
    await ensureSingletonTabDirector()
    await loadJobDefs()
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
  flex-wrap: wrap;
  gap: 8px;
}

.jobs-list-header__title {
  flex: 0 0 auto;
  margin-right: auto;
}

.jobs-list-header__actions {
  flex: 0 0 auto;
  gap: 8px;
  margin-left: auto;
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

.configure-start-dialog {
  width: 640px;
  max-width: 95vw;
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
