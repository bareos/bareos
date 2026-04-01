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
        <tr>
          <th>Version</th>
          <td>{{ data.version }}</td>
        </tr>
        <tr>
          <th>OS</th>
          <td>{{ data.os }}</td>
        </tr>
        <tr>
          <th>Binary Info</th>
          <td>{{ data['binary-info'] }}</td>
        </tr>
        <tr>
          <th>Report Time</th>
          <td>{{ data['report-time'] }}</td>
        </tr>
      </tbody>
    </table>

    <!-- Backup unit totals -->
    <div class="text-subtitle2 q-mb-xs">Backup Unit Totals</div>
    <table class="sub-table q-mb-md">
      <tbody>
        <tr v-for="(val, key) in data['total-units-required']" :key="key">
          <th class="text-capitalize">{{ key }}</th>
          <td :class="key === 'remaining' && Number(val) < 0 ? 'text-negative' : ''">
            {{ val }}
          </td>
        </tr>
      </tbody>
    </table>

    <!-- Per-client backup unit detail -->
    <div class="text-subtitle2 q-mb-xs">Backup Unit Detail</div>
    <table class="sub-table sub-table--full q-mb-md">
      <thead>
        <tr>
          <th>Client</th>
          <th>Fileset</th>
          <th class="text-right">Normal Units</th>
          <th class="text-right">DB Units</th>
          <th class="text-right">VM Units</th>
          <th class="text-right">Filer Units</th>
        </tr>
      </thead>
      <tbody>
        <tr v-for="(row, i) in (data['unit-detail'] ?? [])" :key="i">
          <td>{{ row.client }}</td>
          <td>{{ row.fileset }}</td>
          <td class="text-right">{{ row.normal_units }}</td>
          <td class="text-right">{{ row.db_units }}</td>
          <td class="text-right">{{ row.vm_units }}</td>
          <td class="text-right">{{ row.filer_units }}</td>
        </tr>
      </tbody>
    </table>

    <!-- Uncategorized clients/filesets -->
    <template v-if="data['filesets-not-catogorized']?.length">
      <div class="text-subtitle2 q-mb-xs">
        Clients/Filesets Not Yet Categorized
      </div>
      <table class="sub-table sub-table--full q-mb-md">
        <thead>
          <tr>
            <th v-for="key in Object.keys(data['filesets-not-catogorized'][0])"
                :key="key">{{ key }}</th>
          </tr>
        </thead>
        <tbody>
          <tr v-for="(row, i) in data['filesets-not-catogorized']" :key="i">
            <td v-for="key in Object.keys(row)" :key="key">{{ row[key] }}</td>
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
