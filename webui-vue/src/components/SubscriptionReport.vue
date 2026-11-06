<template>
  <div class="subscription-report">
    <!-- Print-only header -->
    <div class="subscription-print-header">
      <div class="subscription-print-title">{{ t('Bareos Subscription Status Report') }}</div>
      <div v-if="anonymized" class="subscription-print-anon">{{ t('ANONYMIZED REPORT') }}</div>
      <div class="subscription-print-date">{{ t('Generated') }}: {{ generatedAt }}</div>
    </div>

    <div class="sub-report-context q-mb-md">
      <section v-if="data.subscription_accounting" class="sub-status-panel">
        <div class="sub-status-heading">
          <h3>{{ t('Accounting snapshot') }}</h3>
          <span v-if="data.accounting_snapshot?.refresh_thread_state" class="sub-worker-state"
                :class="{ 'sub-worker-state--busy': workerBusy }">
            {{ t('Accounting worker') }}: {{ data.accounting_snapshot.refresh_thread_state }}
            <q-spinner v-if="workerBusy" size="xs" class="q-ml-xs"
                       :aria-label="t('Accounting worker is busy')" role="status" />
          </span>
          <q-btn v-if="refreshable && data.subscription_accounting.calculated_at" flat round dense size="sm" icon="refresh"
                 class="subscription-refresh"
                 :aria-label="t('Refresh calculation')"
                 :title="canRefresh ? t('Refresh calculation') : t('Refresh is available when the accounting worker is idle.')"
                 :loading="refreshLoading"
                 :disable="!canRefresh || refreshDisabled"
                 @click="requestRefresh" />
        </div>
        <dl class="sub-status-details">
          <div>
            <dt>{{ t('Size source') }}</dt>
            <dd>{{ data.subscription_accounting.source === 'measured'
              ? t('Measured')
              : data.subscription_accounting.source === 'mixed'
                ? t('Measured and estimated')
                : t('Estimated') }}</dd>
          </div>
          <div v-if="data.subscription_accounting.calculated_at">
            <dt>{{ t('Accounting snapshot') }}</dt>
            <dd>{{ snapshotAgeLabel }} <span class="sub-context-date">{{ data.subscription_accounting.calculated_at }}</span></dd>
          </div>
          <div v-if="data.accounting_snapshot?.last_full_duration_ms != null">
            <dt>{{ t('Last successful full recalculation') }}</dt>
            <dd>
              {{ formatNumber(data.accounting_snapshot.last_full_duration_ms / 1000, settings.locale) }}
              {{ t('seconds') }} <span class="sub-context-date">{{ data.accounting_snapshot.last_full_completed_at }}</span>
            </dd>
          </div>
        </dl>
        <div class="sub-status-notices">
          <div v-if="data.accounting_snapshot?.pending_jobs" class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('Pending accounting jobs') }}: {{ data.accounting_snapshot.pending_jobs }}
          </div>
          <div v-if="data.accounting_snapshot?.last_refresh_error" class="text-negative">
            {{ data.accounting_snapshot.last_refresh_error }}
          </div>
          <div v-if="Number(data.subscription_accounting.estimated_combinations) > 0" class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('Estimated sizes for {count} of {total} Client/FileSet combinations', {
              count: data.subscription_accounting.estimated_combinations,
              total: data.subscription_accounting.combinations,
            }) }}
          </div>
          <div v-if="!data.subscription_accounting.calculated_at" class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('No accounting snapshot is available; all sizes are estimates.') }}
          </div>
          <div v-if="data.subscription_accounting.calculated_at && data.subscription_accounting.stale"
               class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('Accounting snapshot is more than 24 hours old.') }}
          </div>
          <div v-if="data.subscription_accounting.refresh_failed" class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('Latest accounting refresh failed.') }}
          </div>
          <div v-if="data.subscription_accounting.multiple_catalogs" class="sub-warning">
            <q-icon name="warning" aria-hidden="true" />
            {{ t('Multiple catalogs are not supported; only the first configured catalog is refreshed.') }}
          </div>
        </div>
      </section>

      <!-- Meta info -->
      <dl class="sub-report-meta">
        <div><dt>{{ t('Version') }}</dt><dd>{{ data.version }}</dd></div>
        <div><dt>{{ t('OS') }}</dt><dd>{{ data.os }}</dd></div>
        <div><dt>{{ t('Binary Info') }}</dt><dd>{{ data['binary-info'] }}</dd></div>
        <div><dt>{{ t('Report Time') }}</dt><dd>{{ data['report-time'] }}</dd></div>
      </dl>
    </div>

    <!-- Unit Summary -->
    <template v-if="data['unit-summary']">
      <div class="text-h6 q-mb-sm q-mt-sm">{{ t('Unit Summary') }}</div>
      <div class="sub-summary-card q-mb-md">
        <div class="sub-summary-stats">
          <div class="sub-summary-stat">
            <div class="sub-summary-value">{{ data['unit-summary'].used }}</div>
            <div class="sub-summary-label">{{ t('Used') }}</div>
          </div>
          <div class="sub-summary-stat">
            <div class="sub-summary-value">{{ data['unit-summary'].configured }}</div>
            <div class="sub-summary-label">{{ t('Configured') }}</div>
          </div>
          <div class="sub-summary-stat">
            <div class="sub-summary-value"
                 :class="remainingSummary.isOverLimit ? 'text-negative' : 'text-positive'">
              {{ remainingSummary.value }}
            </div>
            <div class="sub-summary-label">{{ t(remainingSummary.labelKey) }}</div>
          </div>
        </div>
        <div class="sub-summary-mode">
          {{ data['unit-summary'].accounting_mode }}
        </div>
      </div>
    </template>

    <!-- Per-client detail -->
    <template v-if="data['unit-clients']?.length">
      <div class="text-h6 q-mb-xs q-mt-sm">{{ t('Units by Client') }}</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>{{ t('Client') }}</th>
            <th class="text-right">{{ t('Count') }}</th>
            <th class="text-right">{{ t('Size (GB)') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-clients']" :key="i"
              :class="row.client === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.client }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtGB(row.size_gb) }}</td>
          </tr>
        </tbody>
      </table>
    </template>

    <!-- Per-plugin detail -->
    <template v-if="data['unit-plugins']?.length">
      <div class="text-h6 q-mb-xs q-mt-sm">{{ t('Units by Plugin') }}</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>{{ t('Plugin') }}</th>
            <th class="text-right">{{ t('Count') }}</th>
            <th class="text-right">{{ t('Size (GB)') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-plugins']" :key="i"
              :class="row.plugin === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.plugin }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtGB(row.size_gb) }}</td>
          </tr>
        </tbody>
      </table>
    </template>

    <!-- Full detail -->
    <template v-if="data['unit-detail']?.length">
      <div class="text-h6 q-mb-xs q-mt-sm">{{ t('Client / Plugin Breakdown') }}</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>{{ t('Client') }}</th>
            <th>{{ t('Plugin') }}</th>
            <th class="text-right">{{ t('Count') }}</th>
            <th class="text-right">{{ t('Size (GB)') }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-detail']" :key="i"
              :class="row.client === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.client }}</td>
            <td>{{ row.plugin }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtGB(row.size_gb) }}</td>
          </tr>
        </tbody>
      </table>
    </template>

    <template v-if="data.accounting?.length">
      <div class="sub-accounting-heading q-mb-xs q-mt-sm">
        <div class="text-h6">{{ t('Accounting Details') }}</div>
        <q-checkbox v-if="refreshable" :model-value="showLogicalSize"
                    :label="t('Show logical size')" class="print-hide"
                    @update:model-value="emit('update:showLogicalSize', $event)" />
      </div>
      <div v-if="data.summary?.warning" class="sub-warning q-mb-md" role="alert">
        <q-icon name="warning" aria-hidden="true" />
        {{ data.summary.warning }}
      </div>
      <table class="sub-table q-mb-md">
        <thead>
          <tr>
            <th>{{ t('Client') }}</th><th>{{ t('FileSet') }}</th>
            <th>{{ t('Files') }}</th><th>{{ t('Accounted size (GB)') }}</th>
            <th v-if="showLogicalSize">{{ t('Logical size (GB)') }}</th><th>{{ t('Rule') }}</th>
            <th>{{ t('Chain jobs') }}</th>
          </tr>
        </thead>
        <tbody v-for="(row, i) in data.accounting" :key="i"
               :class="row.accounting_issue ? 'sub-accounting-group--issue' : ''">
          <tr>
            <td>{{ row.client }}</td><td>{{ row.fileset }}</td>
            <td>{{ row.excluded ? '—' : formatNumber(row.files, settings.locale) }}</td>
            <td>{{ row.excluded ? '—' : fmtGB(row.bytes / 1e9) }}</td>
            <td v-if="showLogicalSize">{{ row.excluded ? '—' : fmtGB(row.logical_bytes / 1e9) }}</td>
            <td :title="row.rule_explanation">{{ row.rule_code }}</td><td>{{ row.jobs_in_chain }}</td>
          </tr>
          <tr v-if="row.accounting_issue" class="sub-accounting-issue">
            <td :colspan="showLogicalSize ? 7 : 6">
              <q-icon name="warning" size="xs" class="q-mr-xs" />
              <template v-if="refreshable && row.baseline_full_jobid && row.accounting_issue_before_jobid"><span>{{ row.accounting_issue_before_jobid }}</span><a href="#" @click.prevent="emit('open-job', row.baseline_full_jobid)">{{ row.baseline_full_jobid }}</a><span>{{ row.accounting_issue_after_jobid }}</span></template>
              <template v-else>{{ row.accounting_issue }}</template>
            </td>
          </tr>
        </tbody>
        <tbody>
          <tr v-if="data.summary" class="sub-row--total">
            <td>{{ t('TOTAL') }}</td><td></td>
            <td>{{ formatNumber(data.summary.total_files, settings.locale) }}</td>
            <td>{{ fmtGB(data.summary.total_bytes / 1e9) }}</td>
            <td v-if="showLogicalSize">{{ fmtGB(data.summary.total_logical_bytes / 1e9) }}</td>
            <td colspan="2"></td>
          </tr>
        </tbody>
      </table>
      <div class="text-caption q-mb-md">
        {{ data.accounting_rule_legend }}
      </div>
    </template>

    <!-- Checksum -->
    <div class="text-caption text-grey q-mb-md">
      {{ t('Checksum') }}: {{ data.checksum }}
    </div>
  </div>
</template>

<script setup>
import { computed, ref, watch, onMounted, onUnmounted } from 'vue'
import { useSettingsStore } from '../stores/settings.js'
import { useI18n } from 'vue-i18n'
import { formatLocalDateTime, formatNumber, formatRelativeDate } from '../utils/locales.js'
import { subscriptionRemainingSummary } from '../utils/subscription.js'

const props = defineProps({
  data:      { type: Object, required: true },
  anonymized:{ type: Boolean, default: false },
  refreshable: { type: Boolean, default: false },
  refreshLoading: { type: Boolean, default: false },
  refreshDisabled: { type: Boolean, default: false },
  showLogicalSize: { type: Boolean, default: false },
})
const emit = defineEmits(['refresh', 'open-job', 'update:showLogicalSize'])

const settings = useSettingsStore()
const { t } = useI18n()
const generatedAt = formatLocalDateTime(new Date(), settings.locale)
const remainingSummary = computed(() => subscriptionRemainingSummary(props.data?.['unit-summary']))
const now = ref(Date.now())
const receivedAt = ref(now.value)
watch(() => props.data, () => {
  now.value = Date.now()
  receivedAt.value = now.value
})
const snapshotAgeSeconds = computed(() => {
  const age = props.data.subscription_accounting?.age_seconds
  if (age === undefined || age === null || !Number.isFinite(Number(age))) return null
  return Number(age) + (now.value - receivedAt.value) / 1000
})
const snapshotAgeLabel = computed(() => (
  snapshotAgeSeconds.value === null
    ? t('Unknown age')
    : formatRelativeDate(new Date(Date.now() - snapshotAgeSeconds.value * 1000), settings.locale)
))
const canRefresh = computed(() => (
  props.data.accounting_snapshot?.refresh_thread_state === 'idle'
))
const workerBusy = computed(() => (
  ['running', 'queued'].includes(props.data.accounting_snapshot?.refresh_thread_state)
))
let ageTimer
onMounted(() => { ageTimer = setInterval(() => { now.value = Date.now() }, 1000) })
onUnmounted(() => { clearInterval(ageTimer) })

function requestRefresh() {
  if (canRefresh.value && !props.refreshDisabled && !props.refreshLoading) emit('refresh')
}

function fmtGB(gbStr) {
  return formatNumber(parseFloat(gbStr), settings.locale, {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  })
}
</script>

<style scoped>
.sub-warning {
  display: flex;
  align-items: baseline;
  gap: 8px;
  padding: 8px 12px;
  border-left: 3px solid var(--q-warning);
  border-radius: 4px;
  background: #fff8e6;
  color: #333;
}
.sub-warning > .q-icon,
.sub-accounting-issue .q-icon {
  color: var(--q-warning);
  flex-shrink: 0;
}
.sub-accounting-heading {
  display: flex;
  align-items: center;
  justify-content: space-between;
  flex-wrap: wrap;
  gap: 8px 16px;
}
.subscription-refresh {
  @media print {
    display: none;
  }
}
/* Print header — hidden on screen, shown when printing */
.subscription-print-header {
  display: none;
}

.sub-report-context {
  display: grid;
  grid-template-columns: minmax(0, 2fr) minmax(220px, 1fr);
  gap: 20px;
}
.sub-status-panel {
  border: 1px solid #ddd;
  border-radius: 8px;
  padding: 16px;
}
.sub-status-heading {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
  margin-bottom: 12px;
}
.sub-status-heading h3 {
  font-size: 1rem;
  font-weight: 600;
  line-height: 1.4;
  margin: 0 auto 0 0;
}
.sub-worker-state {
  display: inline-flex;
  align-items: center;
  border-radius: 4px;
  padding: 3px 8px;
  background: #f0f0f0;
  font-size: 0.8rem;
}
.sub-worker-state--busy {
  background: #e6f2fa;
}
.sub-status-details,
.sub-report-meta {
  margin: 0;
  font-size: 0.85rem;
}
.sub-status-details > div,
.sub-report-meta > div {
  display: grid;
  grid-template-columns: minmax(100px, 1fr) minmax(0, 2fr);
  gap: 8px;
  padding: 5px 0;
}
.sub-status-details dt,
.sub-report-meta dt {
  color: #666;
}
.sub-status-details dd,
.sub-report-meta dd {
  margin: 0;
  overflow-wrap: anywhere;
}
.sub-context-date {
  display: block;
  color: #666;
  font-size: 0.8rem;
}
.sub-status-notices:not(:empty) {
  border-top: 1px solid #eee;
  margin-top: 12px;
  padding-top: 10px;
  font-size: 0.85rem;
}
.sub-status-notices > div + div {
  margin-top: 4px;
}
@media (max-width: 800px) {
  .sub-report-context {
    grid-template-columns: minmax(0, 1fr);
  }
}

/* Unit Summary card */
.sub-summary-card {
  border: 2px solid #0075be;
  border-radius: 8px;
  padding: 16px 20px;
  background: #f0f7ff;
}
.sub-summary-stats {
  display: flex;
  gap: 40px;
  margin-bottom: 12px;
}
.sub-summary-stat {
  text-align: center;
}
.sub-summary-value {
  font-size: 2rem;
  font-weight: 700;
  line-height: 1.1;
}
.sub-summary-label {
  font-size: 0.75rem;
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: #555;
  margin-top: 2px;
}
.sub-summary-mode {
  font-size: 0.85rem;
  color: #444;
  border-top: 1px solid #c0d8ee;
  padding-top: 8px;
}

.sub-table {
  border-collapse: collapse;
  font-size: 0.85rem;
}
.sub-table th,
.sub-table td {
  border: 1px solid #ddd;
  padding: 4px 10px;
  text-align: left;
}
.sub-table th.text-right,
.sub-table td.text-right {
  text-align: right;
}
.sub-table thead th {
  background: #f0f0f0;
  font-weight: 600;
}
.sub-table--full {
  width: 100%;
}
.sub-row--total td {
  font-weight: 600;
  border-top: 2px solid #bbb;
}
.sub-accounting-group--issue {
  background: #fff8e6;
  border-left: 3px solid var(--q-warning);
  break-inside: avoid;
}
.sub-accounting-group--issue > tr:first-child > td {
  border-bottom: 0;
}
.sub-accounting-group--issue > .sub-accounting-issue > td {
  border-top: 0;
  padding: 0 10px 8px 24px;
  color: #333;
}

@media print {
  .subscription-print-header {
    display: block;
    margin-bottom: 16px;
    border-bottom: 2px solid #0075be;
    padding-bottom: 8px;
  }
  .subscription-print-title {
    font-size: 1.3rem;
    font-weight: 700;
    color: #0075be;
  }
  .subscription-print-anon {
    font-size: 0.9rem;
    font-weight: 700;
    color: #c00;
    margin-top: 2px;
    letter-spacing: 0.08em;
  }
  .subscription-print-date {
    font-size: 0.8rem;
    color: #555;
    margin-top: 2px;
  }

  /* Let content flow across multiple pages */
  .subscription-report {
    overflow: visible !important;
  }
  .sub-summary-card {
    break-inside: avoid;
  }
  table {
    page-break-inside: auto;
  }
  tr {
    page-break-inside: avoid;
  }
}
</style>
