<template>
  <q-page class="q-pa-md">
    <div class="row q-col-gutter-md">
      <div class="col-12">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>User Command ACL</span>
            <q-space />
            <q-btn
              flat
              round
              dense
              color="white"
              icon="refresh"
              :loading="acl.loading"
              @click="acl.refresh"
            />
          </q-card-section>

          <q-card-section class="row q-col-gutter-md items-start">
            <div class="col-12 col-md-7">
              <div class="row q-gutter-sm items-center">
                <q-chip dense square color="primary" text-color="white" icon="person">
                  {{ auth.user?.username ?? 'user' }}
                </q-chip>
                <q-chip dense square color="blue-7" text-color="white" icon="dns">
                  {{ auth.user?.director ?? 'director' }}
                </q-chip>
                <q-chip dense square color="positive" text-color="white" icon="check_circle">
                  {{ acl.allowedCount }} allowed
                </q-chip>
                <q-chip dense square color="negative" text-color="white" icon="block">
                  {{ acl.deniedCount }} denied
                </q-chip>
              </div>
              <div class="text-caption text-grey-7 q-mt-sm">
                Permissions come directly from the Director's <code>.help</code>
                response for the current console.
              </div>
            </div>

            <div class="col-12 col-md-5">
              <q-input
                v-model="search"
                dense
                outlined
                clearable
                debounce="150"
                label="Filter commands"
                hint="Search by command, description, or arguments"
              >
                <template #prepend>
                  <q-icon name="search" />
                </template>
              </q-input>
            </div>
          </q-card-section>

          <q-separator />

          <q-card-section>
            <div class="row q-gutter-sm q-mb-md">
              <q-btn-toggle
                v-model="filter"
                unelevated
                toggle-color="primary"
                :options="filterOptions"
              />
            </div>

            <q-banner v-if="acl.error" dense rounded class="bg-negative text-white q-mb-md">
              Could not load command permissions: {{ acl.error }}
            </q-banner>

            <q-inner-loading :showing="acl.loading" />

            <div v-if="!acl.loading && !filteredCommands.length" class="text-grey-7 q-pa-md">
              No commands match the current filter.
            </div>

            <div v-else class="column q-gutter-md">
              <q-card
                v-for="group in commandGroups"
                :key="group.name"
                flat
                bordered
              >
                <q-card-section class="row items-center q-py-sm">
                  <div class="text-subtitle2">{{ group.name }}</div>
                  <q-space />
                  <q-chip dense square color="primary" text-color="white">
                    {{ group.items.length }}
                  </q-chip>
                </q-card-section>
                <q-separator />
                <q-list separator>
                  <q-item v-for="item in group.items" :key="item.command" class="q-py-sm">
                    <q-item-section avatar top>
                      <q-icon
                        :name="item.permission ? 'check_circle' : 'block'"
                        :color="item.permission ? 'positive' : 'negative'"
                      />
                    </q-item-section>
                    <q-item-section>
                      <q-item-label class="text-weight-medium">
                        <code>{{ item.command }}</code>
                      </q-item-label>
                      <q-item-label caption class="text-grey-8">
                        {{ item.description || 'No description provided.' }}
                      </q-item-label>
                      <q-item-label
                        v-if="item.arguments"
                        caption
                        class="text-grey-7 q-mt-xs acl-arguments"
                      >
                        {{ item.arguments }}
                      </q-item-label>
                    </q-item-section>
                    <q-item-section side top>
                      <q-chip
                        dense
                        square
                        :color="item.permission ? 'positive' : 'grey-6'"
                        text-color="white"
                      >
                        {{ item.permission ? 'Allowed' : 'Denied' }}
                      </q-chip>
                    </q-item-section>
                  </q-item>
                </q-list>
              </q-card>
            </div>
          </q-card-section>
        </q-card>
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useDirectorAclStore } from '../stores/directorAcl.js'
import { normaliseDirectorCommandPermissions } from '../composables/useDirectorFetch.js'

const auth = useAuthStore()
const director = useDirectorStore()
const acl = useDirectorAclStore()

const search = ref('')
const filter = ref('all')
const filterOptions = [
  { label: 'All', value: 'all' },
  { label: 'Allowed', value: 'allowed' },
  { label: 'Denied', value: 'denied' },
]

const filteredCommands = computed(() => {
  const needle = search.value.trim().toLowerCase()

  return normaliseDirectorCommandPermissions(acl.commands).filter((item) => {
    if (filter.value === 'allowed' && !item.permission) return false
    if (filter.value === 'denied' && item.permission) return false

    if (!needle) return true
    return [
      item.command,
      item.description,
      item.arguments,
      item.category,
    ].some(value => value.toLowerCase().includes(needle))
  })
})

const commandGroups = computed(() => (
  ['Commands', 'Dot commands', 'BVFS']
    .map(name => ({
      name,
      items: filteredCommands.value.filter(item => item.category === name),
    }))
    .filter(group => group.items.length > 0)
))

acl.ensureLoaded()

watch(() => director.isConnected, (connected) => {
  if (connected) {
    acl.ensureLoaded()
  }
})
</script>

<style scoped>
.acl-arguments {
  white-space: pre-wrap;
  word-break: break-word;
  font-family: monospace;
}
</style>
