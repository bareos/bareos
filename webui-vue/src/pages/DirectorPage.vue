<template>
  <q-page class="q-pa-md">
    <q-tabs v-model="tab" dense align="left" class="q-mb-md page-tabs" indicator-color="primary">
      <q-tab name="status"       label="Status"       no-caps />
      <q-tab name="messages"     label="Messages"     no-caps />
      <q-tab name="subscription" label="Subscription" no-caps />
    </q-tabs>

    <q-tab-panels v-model="tab" animated>
      <!-- STATUS -->
      <q-tab-panel name="status" class="q-pa-none">
        <div class="row q-col-gutter-md">
          <div class="col-12 col-md-8">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header row items-center">
                <span>Director Status</span>
                <q-space />
                <q-btn flat round dense icon="refresh" color="white" />
              </q-card-section>
              <q-card-section>
                <div class="console-output">{{ dirStatus }}</div>
              </q-card-section>
            </q-card>
          </div>
          <div class="col-12 col-md-4">
            <q-card flat bordered class="bareos-panel">
              <q-card-section class="panel-header">Version</q-card-section>
              <q-card-section>
                <q-badge color="positive" label="Up to date" class="q-mb-sm" />
                <div class="text-h6">Bareos Director</div>
                <div class="text-caption text-grey">Version 23.0.2 (01 March 2026)</div>
              </q-card-section>
            </q-card>
          </div>
        </div>
      </q-tab-panel>

      <!-- MESSAGES -->
      <q-tab-panel name="messages" class="q-pa-none">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header">Director Messages</q-card-section>
          <q-card-section>
            <div class="console-output">{{ messages }}</div>
          </q-card-section>
        </q-card>
      </q-tab-panel>

      <!-- SUBSCRIPTION -->
      <q-tab-panel name="subscription">
        <q-card flat bordered class="bareos-panel" style="max-width:600px">
          <q-card-section class="panel-header">Subscription</q-card-section>
          <q-card-section>
            <q-list dense separator>
              <q-item><q-item-section class="text-weight-medium" style="max-width:180px">Subscription Status</q-item-section><q-item-section><q-badge color="info" label="Community" /></q-item-section></q-item>
              <q-item><q-item-section class="text-weight-medium" style="max-width:180px">Clients in use</q-item-section><q-item-section>7</q-item-section></q-item>
              <q-item><q-item-section class="text-weight-medium" style="max-width:180px">Clients allowed</q-item-section><q-item-section>Unlimited (Community)</q-item-section></q-item>
            </q-list>
            <q-btn class="q-mt-md" color="primary" label="Get Official Support" icon="open_in_new"
                   href="https://www.bareos.com/subscription/" target="_blank" no-caps />
          </q-card-section>
        </q-card>
      </q-tab-panel>
    </q-tab-panels>
  </q-page>
</template>

<script setup>
import { ref } from 'vue'
import { directorStatus } from '../mock/index.js'

const tab = ref('status')
const dirStatus = directorStatus
const messages = `23-Mar-26 14:00 bareos-dir JobId 7: Start Backup JobId 7, Job=BackupClient1.2026-03-23_14.00.00_04
23-Mar-26 14:00 bareos-dir JobId 7: Using Device "FileStorage" to write.
23-Mar-26 13:22 bareos-dir JobId 6: Bareos bareos-dir 23.0.2 (01Mar26):
  Build OS:               x86_64-linux-gnu ubuntu focal
  JobId:                  6
  Job:                    BackupClient5.2026-03-23_13.00.00_03
  Client:                 "web-fd" 23.0.2 (01Mar26) x86_64-linux-gnu,ubuntu,focal
  Start time:             23-Mar-26 13:00:00
  End time:               23-Mar-26 13:22:15
  Elapsed time:           22 mins 15 secs
  FD Files Written:       23,451
  SD Files Written:       23,451
  FD Bytes Written:       5,368,709,120 (5.368 GB)
  SD Bytes Written:       5,368,709,120 (5.368 GB)
  Termination:            Backup OK`
</script>
