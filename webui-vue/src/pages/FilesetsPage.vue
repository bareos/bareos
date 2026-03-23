<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel">
      <q-card-section class="panel-header">Filesets</q-card-section>
      <q-card-section class="q-pa-none">
        <q-table
          :rows="filesets"
          :columns="columns"
          row-key="name"
          dense flat
          :pagination="{ rowsPerPage: 15 }"
        >
          <template #body="props">
            <q-tr :props="props">
              <q-td v-for="col in props.cols" :key="col.name" :props="props">
                <template v-if="col.name === 'name'">
                  <span
                    class="text-primary cursor-pointer"
                    @click="props.expand = !props.expand"
                  >{{ col.value }}</span>
                  <q-icon
                    :name="props.expand ? 'expand_less' : 'expand_more'"
                    class="q-ml-xs text-grey cursor-pointer"
                    @click="props.expand = !props.expand"
                  />
                </template>
                <template v-else>{{ col.value }}</template>
              </q-td>
            </q-tr>
            <q-tr v-if="props.expand" :props="props" class="bg-grey-1">
              <q-td colspan="100%" class="q-pa-md">
                <div class="row q-col-gutter-md text-caption">
                  <div class="col-12 col-md-4" v-if="props.row.include.length">
                    <div class="text-weight-bold q-mb-xs">Include</div>
                    <div v-for="p in props.row.include" :key="p" class="q-ml-sm text-mono">{{ p }}</div>
                  </div>
                  <div class="col-12 col-md-4" v-if="props.row.exclude.length">
                    <div class="text-weight-bold q-mb-xs">Exclude</div>
                    <div v-for="p in props.row.exclude" :key="p" class="q-ml-sm text-mono">{{ p }}</div>
                  </div>
                  <div class="col-12 col-md-4" v-if="props.row.options">
                    <div class="text-weight-bold q-mb-xs">Options</div>
                    <div class="q-ml-sm text-mono">{{ props.row.options }}</div>
                  </div>
                </div>
              </q-td>
            </q-tr>
          </template>
        </q-table>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { mockFilesets } from '../mock/index.js'

const filesets = mockFilesets
const columns = [
  { name: 'name',        label: 'Name',        field: 'name',        align: 'left', sortable: true },
  { name: 'description', label: 'Description', field: 'description', align: 'left' },
]
</script>
