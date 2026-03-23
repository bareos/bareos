<template>
  <q-page class="q-pa-md">
    <q-card flat bordered class="bareos-panel" style="max-width:860px">
      <q-card-section class="panel-header">Restore Files</q-card-section>
      <q-card-section>
        <q-form class="q-gutter-md">
          <div class="row q-col-gutter-md">
            <div class="col-12 col-md-6">
              <q-select v-model="form.client" :options="clientNames" label="Client" outlined dense emit-value map-options />
            </div>
            <div class="col-12 col-md-6">
              <q-select v-model="form.restoreclient" :options="clientNames" label="Restore to Client" outlined dense />
            </div>
            <div class="col-12 col-md-6">
              <q-select v-model="form.restorejob" :options="restoreJobs" label="Restore Job" outlined dense />
            </div>
            <div class="col-12 col-md-6">
              <q-select v-model="form.fileset" :options="filesets" label="Fileset" outlined dense />
            </div>
            <div class="col-12 col-md-6">
              <q-select v-model="form.replace" :options="replaceOptions" label="Replace" outlined dense />
            </div>
            <div class="col-12 col-md-6">
              <q-input v-model="form.where" label="Restore to (Where)" outlined dense placeholder="/ or /tmp/bareos-restores" />
            </div>
          </div>

          <!-- File tree placeholder -->
          <div class="text-subtitle2 q-mt-md q-mb-sm">Select Files</div>
          <q-card flat bordered>
            <q-card-section style="min-height:200px; background:#fafafa">
              <q-tree
                :nodes="filetree"
                node-key="id"
                tick-strategy="leaf"
                v-model:ticked="ticked"
                v-model:expanded="expanded"
                class="text-caption"
              />
            </q-card-section>
          </q-card>

          <div class="row q-gutter-sm q-mt-sm">
            <q-btn color="primary" label="Restore" icon="restore" @click="doRestore" />
            <q-btn flat label="Reset" @click="reset" />
          </div>
        </q-form>
      </q-card-section>
    </q-card>
  </q-page>
</template>

<script setup>
import { ref } from 'vue'

const clientNames = ['bareos-fd', 'fileserver-fd', 'db-server-fd', 'mail-fd', 'web-fd', 'nas-fd', 'win-client-fd']
const restoreJobs  = ['RestoreFiles', 'RestoreData', 'RestoreArchive']
const filesets     = ['Linux All', 'Windows All', 'Catalog', 'SelfTest']
const replaceOptions = ['Always', 'IfNewer', 'IfOlder', 'Never']

const form = ref({
  client:        'bareos-fd',
  restoreclient: 'bareos-fd',
  restorejob:    'RestoreFiles',
  fileset:       'Linux All',
  replace:       'Always',
  where:         '/tmp/bareos-restores',
})

const ticked   = ref([])
const expanded = ref(['root'])

const filetree = [
  {
    id: 'root', label: '/',
    children: [
      { id: 'etc',  label: 'etc/',  children: [
        { id: 'etc-hosts',  label: 'hosts' },
        { id: 'etc-passwd', label: 'passwd' },
      ]},
      { id: 'home', label: 'home/', children: [
        { id: 'home-user1', label: 'user1/', children: [
          { id: 'home-user1-docs', label: 'Documents/' },
        ]},
      ]},
      { id: 'var',  label: 'var/',  children: [
        { id: 'var-log', label: 'log/' },
      ]},
    ]
  }
]

function doRestore() {}
function reset() { ticked.value = []; expanded.value = ['root'] }
</script>
