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
      <div class="text-subtitle2 q-mb-xs">Unit Summary</div>
      <table class="sub-table q-mb-md">
        <tbody>
          <tr>
            <th>Accounting Mode</th>
            <td>{{ data['unit-summary'].accounting_mode }}</td>
          </tr>
          <tr>
            <th>Used</th>
            <td>{{ data['unit-summary'].used }}</td>
          </tr>
          <tr>
            <th>Configured</th>
            <td>{{ data['unit-summary'].configured }}</td>
          </tr>
          <tr>
            <th>Remaining</th>
            <td :class="Number(data['unit-summary'].remaining) < 0 ? 'text-negative' : 'text-positive'">
              {{ data['unit-summary'].remaining }}
            </td>
          </tr>
        </tbody>
      </table>
    </template>

    <!-- Per-client detail -->
    <template v-if="data['unit-clients']?.length">
      <div class="text-subtitle2 q-mb-xs">Units by Client</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Client</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (TB)</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-clients']" :key="i"
              :class="row.client === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.client }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtTB(row.size_gb) }}</td>
          </tr>
        </tbody>
      </table>
    </template>

    <!-- Per-plugin detail -->
    <template v-if="data['unit-plugins']?.length">
      <div class="text-subtitle2 q-mb-xs">Units by Plugin</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Plugin</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (TB)</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-plugins']" :key="i"
              :class="row.plugin === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.plugin }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtTB(row.size_gb) }}</td>
          </tr>
        </tbody>
      </table>
    </template>

    <!-- Full detail -->
    <template v-if="data['unit-detail']?.length">
      <div class="text-subtitle2 q-mb-xs">Full Detail</div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th>Client</th>
            <th>Plugin</th>
            <th class="text-right">Count</th>
            <th class="text-right">Size (TB)</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['unit-detail']" :key="i"
              :class="row.client === 'TOTAL' ? 'sub-row--total' : ''">
            <td>{{ row.client }}</td>
            <td>{{ row.plugin }}</td>
            <td class="text-right">{{ row.count }}</td>
            <td class="text-right">{{ fmtTB(row.size_gb) }}</td>
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

function fmtTB(gbStr) {
  const tb = parseFloat(gbStr) / 1000
  return tb.toLocaleString(undefined, {
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
}
</style>
