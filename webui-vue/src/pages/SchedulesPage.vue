<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel">
      <q-card-section class="panel-header">Schedules</q-card-section>
      <q-card-section class="q-pa-none">
        <q-table :rows="schedules" :columns="columns" row-key="name" dense flat :pagination="{ rowsPerPage: 15 }">
          <template #body-cell-enabled="props">
            <q-td :props="props" class="text-center">
              <EnabledBadge :enabled="props.value" />
            </q-td>
          </template>
          <template #body-cell-level="props">
            <q-td :props="props">{{ props.value }}</q-td>
          </template>
          <template #body-cell-actions="props">
            <q-td :props="props" class="text-center">
              <q-btn flat round dense size="sm"
                     :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                     :color="props.row.enabled ? 'warning' : 'positive'"
                     :title="props.row.enabled ? 'Disable' : 'Enable'" />
              <q-btn flat round dense size="sm" icon="info" title="Details" />
            </q-td>
          </template>
        </q-table>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { computed } from 'vue'
import EnabledBadge from '../components/EnabledBadge.vue'
import { useDirectorFetch } from '../composables/useDirectorFetch.js'

// "list schedules" returns {schedules:[{name,...}]} or may be empty in some configs.
// Fall back to .schedules dot command result if needed.
const { data: rawSchedules, loading, error, refresh } = useDirectorFetch('list schedules', 'schedules')

const schedules = computed(() => (rawSchedules.value ?? []).map(s => ({
  name:    s.name    ?? '',
  level:   s.level   ?? s.scheduletime ?? '',
  runs:    s.runs    ?? s.runhour ?? '',
  enabled: s.enabled !== '0' && s.enabled !== false,
})))

const columns = [
  { name: 'name',    label: 'Name',      field: 'name',    align: 'left',  sortable: true },
  { name: 'level',   label: 'Level',     field: 'level',   align: 'left'   },
  { name: 'runs',    label: 'Run Times', field: 'runs',    align: 'left'   },
  { name: 'enabled', label: 'Status',    field: 'enabled', align: 'center' },
  { name: 'actions', label: '',          field: 'actions', align: 'center', style: 'width:80px' },
]
</script>
