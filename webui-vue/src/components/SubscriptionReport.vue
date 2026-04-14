<template>
  <div class="subscription-report">
    <!-- Print-only header -->
    <div class="subscription-print-header">
      <div class="subscription-print-title">Bareos Subscription Status Report</div>
      <div v-if="anonymized" class="subscription-print-anon">ANONYMIZED REPORT</div>
      <div class="subscription-print-date">Generated: {{ generatedAt }}</div>
    </div>

    <!-- Meta info -->
    <table class="sub-meta-table q-mb-md">
      <tbody>
        <tr><th>Version</th>     <td>{{ data.version }}</td></tr>
        <tr><th>OS</th>          <td>{{ data.os }}</td></tr>
        <tr><th>Binary Info</th> <td>{{ data['binary-info'] }}</td></tr>
        <tr><th>Report Time</th> <td>{{ data['report-time'] }}</td></tr>
      </tbody>
    </table>

    <!-- Unit Summary -->
    <template v-if="data['unit-summary']">
      <div class="text-h6 q-mb-sm q-mt-sm">Unit Summary</div>
      <div class="sub-summary-card q-mb-md">
        <div class="sub-summary-stats">
          <div class="sub-summary-stat">
            <div class="sub-summary-value">{{ data['unit-summary'].used }}</div>
            <div class="sub-summary-label">Used</div>
          </div>
          <div class="sub-summary-stat">
            <div class="sub-summary-value">{{ data['unit-summary'].configured }}</div>
            <div class="sub-summary-label">Configured</div>
          </div>
          <div class="sub-summary-stat">
            <div class="sub-summary-value"
                 :class="Number(data['unit-summary'].remaining) < 0 ? 'text-negative' : 'text-positive'">
              {{ data['unit-summary'].remaining }}
            </div>
            <div class="sub-summary-label">Remaining</div>
          </div>
        </div>
        <div class="sub-summary-mode">
          {{ data['unit-summary'].accounting_mode }}
        </div>
      </div>
    </template>

    <!-- Per-client detail -->
    <template v-if="data['unit-clients']?.length">
      <div class="text-h6 q-mb-xs q-mt-sm">Units by Client</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Client</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (GB)</th>
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
      <div class="text-h6 q-mb-xs q-mt-sm">Units by Plugin</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Plugin</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (GB)</th>
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
      <div class="text-h6 q-mb-xs q-mt-sm">Full Detail</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Client</th>
            <th>Plugin</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (GB)</th>
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

    <!-- Checksum -->
    <div class="text-caption text-grey q-mb-md">
      Checksum: {{ data.checksum }}
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  data:      { type: Object, required: true },
  anonymized:{ type: Boolean, default: false },
})

const generatedAt = new Date().toLocaleString()

function fmtGB(gbStr) {
  return parseFloat(gbStr).toLocaleString(undefined, {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2,
  })
}
</script>

<style scoped>
/* Print header — hidden on screen, shown when printing */
.subscription-print-header {
  display: none;
}

.sub-meta-table th {
  text-align: left;
  font-weight: 600;
  padding: 4px 12px 4px 0;
  min-width: 140px;
}
.sub-meta-table td {
  padding: 4px 0;
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
