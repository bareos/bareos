<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="list"     label="Show"     no-caps />
      <q-tab name="timeline" label="Timeline" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- SHOW -->
      <q-tab-panel name="list" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>Client List</span>
            <q-space />
            <q-btn flat round dense icon="refresh" color="white" />
          </q-card-section>
          <q-card-section class="q-pa-none">
            <q-table :rows="clients" :columns="columns" row-key="name" dense flat :pagination="{ rowsPerPage: 15 }">
              <template #body-cell-name="props">
                <q-td :props="props">
                  <router-link :to="{ name: 'client-details', params: { name: props.value } }" class="text-primary">
                    {{ props.value }}
                  </router-link>
                </q-td>
              </template>
              <template #body-cell-os="props">
                <q-td :props="props">
                  <q-icon :name="props.value === 'windows' ? 'computer' : 'terminal'" class="q-mr-xs" />
                  {{ props.row.uname }}
                </q-td>
              </template>
              <template #body-cell-enabled="props">
                <q-td :props="props" class="text-center">
                  <q-badge :color="props.value ? 'positive' : 'negative'" :label="props.value ? 'Enabled' : 'Disabled'" />
                </q-td>
              </template>
              <template #body-cell-actions="props">
                <q-td :props="props" class="text-center">
                  <q-btn flat round dense size="sm"
                         :icon="props.row.enabled ? 'pause' : 'play_arrow'"
                         :color="props.row.enabled ? 'warning' : 'positive'"
                         :title="props.row.enabled ? 'Disable' : 'Enable'" />
                  <q-btn flat round dense size="sm" icon="info" title="Details"
                         :to="{ name: 'client-details', params: { name: props.row.name } }" />
                </q-td>
              </template>
            </q-table>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- TIMELINE -->
      <q-tab-panel name="timeline">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Client Timeline</q-card-section>
          <q-card-section class="text-center text-grey q-py-xl">
            <q-icon name="timeline" size="48px" class="q-mb-md" />
            <div>Timeline chart will be rendered here</div>
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref } from 'vue'
import { mockClients } from '../mock/index.js'

const tab = ref('list')
const clients = mockClients

const columns = [
  { name: 'name',    label: 'Name',    field: 'name',    align: 'left',   sortable: true },
  { name: 'os',      label: 'OS',      field: 'os',      align: 'left'    },
  { name: 'version', label: 'Version', field: 'version', align: 'left',   sortable: true },
  { name: 'enabled', label: 'Status',  field: 'enabled', align: 'center'  },
  { name: 'actions', label: '',        field: 'actions', align: 'center',  style: 'width:80px' },
]
</script>
