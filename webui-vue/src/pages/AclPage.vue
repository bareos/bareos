<template>
  <q-page class="q-pa-md">
    <q-card v-if="directorOptions.length > 1" flat bordered class="q-mb-md bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('ACL Scope') }}</span>
        <q-space />
        <q-chip dense square color="white" text-color="primary" :label="aclScopeLabel" />
      </q-card-section>
      <q-card-section>
        <q-select
          v-model="selectedDirectorsModel"
          data-testid="acl-directors"
          :options="directorOptions"
          option-label="label"
          option-value="value"
          emit-value
          map-options
          multiple
          use-chips
          outlined
          dense
          :label="t('Directors')"
        />
        <div class="text-caption text-grey-6 q-mt-sm">
          {{ t('Select the directors that contribute to the ACL view.') }}
        </div>
        <q-banner
          v-if="directorErrors.length"
          rounded
          dense
          class="bg-warning text-black q-mt-md"
        >
          <template #avatar>
            <q-icon name="warning" />
          </template>
          <div v-for="item in directorErrors" :key="item.director">
            <strong>{{ item.director }}</strong>: {{ item.message }}
          </div>
        </q-banner>
      </q-card-section>
    </q-card>

    <div class="row q-col-gutter-md">
      <div class="col-12">
        <q-card flat bordered class="bareos-panel">
          <q-card-section class="panel-header row items-center">
            <span>{{ t('User Command ACL') }}</span>
            <q-space />
            <q-btn
              flat
              round
              dense
              color="white"
              icon="refresh"
              :loading="loading"
              @click="refresh"
            />
          </q-card-section>

          <q-card-section class="row q-col-gutter-md items-start">
            <div class="col-12 col-md-7">
              <div class="row q-gutter-sm items-center">
                <q-chip dense square color="primary" text-color="white" icon="person">
                  {{ auth.user?.username ?? 'user' }}
                </q-chip>
                <q-chip dense square color="blue-7" text-color="white" icon="dns">
                  {{ aclScopeLabel }}
                </q-chip>
                <q-chip dense square color="positive" text-color="white" icon="check_circle">
                  {{ allowedCount }} {{ t('allowed') }}
                </q-chip>
                <q-chip dense square color="negative" text-color="white" icon="block">
                  {{ deniedCount }} {{ t('denied') }}
                </q-chip>
              </div>
              <div class="text-caption text-grey-7 q-mt-sm">
                {{ t("Permissions come directly from the Director's") }} <code>.help</code>
                {{ t('response for the current console.') }}
              </div>
            </div>

            <div class="col-12 col-md-5">
              <q-input
                v-model="search"
                dense
                outlined
                clearable
                debounce="150"
                :label="t('Filter commands')"
                :hint="t('Search by command, description, or arguments')"
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

            <q-banner v-if="error" dense rounded class="bg-negative text-white q-mb-md">
              {{ t('Could not load command permissions') }}: {{ error }}
            </q-banner>

            <q-inner-loading :showing="loading" />

            <div v-if="!loading && !filteredCommands.length" class="text-grey-7 q-pa-md">
              {{ t('No commands match the current filter.') }}
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
                  <q-item v-for="item in group.items" :key="item.scopeKey" class="q-py-sm">
                    <q-item-section avatar top>
                      <q-icon
                        :name="item.permission ? 'check_circle' : 'block'"
                        :color="item.permission ? 'positive' : 'negative'"
                      />
                    </q-item-section>
                    <q-item-section>
                      <q-item-label class="text-weight-medium">
                        <code>{{ item.command }}</code>
                        <q-chip
                          v-if="isCommonAcl"
                          dense
                          square
                          color="primary"
                          text-color="white"
                          class="q-ml-sm"
                        >
                          {{ item.director }}
                        </q-chip>
                      </q-item-label>
                      <q-item-label caption class="text-grey-8">
                        {{ item.description || t('No description provided.') }}
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
                        {{ item.permission ? t('Allowed') : t('Denied') }}
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
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { fetchAggregatedAcl } from '../composables/aclAggregate.js'
import { normaliseDirectorCommandPermissions } from '../composables/useDirectorFetch.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useDirectorAclStore } from '../stores/directorAcl.js'
import { useSettingsStore } from '../stores/settings.js'

const auth = useAuthStore()
const director = useDirectorStore()
const acl = useDirectorAclStore()
const settings = useSettingsStore()
const { t } = useI18n()

const search = ref('')
const filter = ref('all')
const aggregateCommands = ref([])
const aggregateLoading = ref(false)
const aggregateError = ref(null)
const directorErrors = ref([])

const directorOptions = computed(() => {
  const values = new Set([
    ...director.availableDirectors,
    ...settings.selectedDirectors,
    auth.user?.director,
    settings.directorName,
  ].filter(Boolean))
  return [...values].map(value => ({ label: value, value }))
})

function syncSelectedDirectors() {
  const validDirectors = directorOptions.value.map(option => option.value)
  const selected = settings.selectedDirectors.filter(value => validDirectors.includes(value))

  if (selected.length > 0) {
    if (selected.length !== settings.selectedDirectors.length) {
      settings.setSelectedDirectors(selected)
    }
    return
  }

  const fallbackDirector = auth.user?.director || settings.directorName
  if (fallbackDirector) {
    settings.setSelectedDirectors([fallbackDirector])
  }
}

const selectedDirectorsModel = computed({
  get: () => settings.selectedDirectors,
  set: (value) => {
    const selected = Array.isArray(value) ? value : []
    if (selected.length > 0) {
      settings.setSelectedDirectors(selected)
      return
    }

    const fallbackDirector = auth.user?.director || settings.directorName
    settings.setSelectedDirectors(fallbackDirector ? [fallbackDirector] : [])
  },
})

const activeDirectors = computed(() => {
  const selected = settings.selectedDirectors.filter(value => (
    directorOptions.value.some(option => option.value === value)
  ))

  if (selected.length > 0) {
    return selected
  }

  const currentDirector = auth.user?.director || settings.directorName
  return currentDirector ? [currentDirector] : []
})

const isCommonAcl = computed(() => activeDirectors.value.length > 1)
const aclScopeLabel = computed(() => (
  isCommonAcl.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))

const loading = computed(() => (
  isCommonAcl.value ? aggregateLoading.value : acl.loading
))
const error = computed(() => (
  isCommonAcl.value ? aggregateError.value : acl.error
))
const currentCommands = computed(() => (
  isCommonAcl.value
    ? aggregateCommands.value
    : normaliseDirectorCommandPermissions(acl.commands).map(item => ({
      ...item,
      director: auth.user?.director ?? settings.directorName ?? '',
      scopeKey: `${auth.user?.director ?? settings.directorName ?? ''}:${item.command}`,
    }))
))
const allowedCount = computed(() => currentCommands.value.filter(item => item.permission).length)
const deniedCount = computed(() => currentCommands.value.filter(item => !item.permission).length)

async function ensureSingleScopeDirector() {
  if (activeDirectors.value.length !== 1) {
    return
  }

  const scopeDirector = activeDirectors.value[0]
  if (!scopeDirector) {
    return
  }

  if (auth.user?.director === scopeDirector && director.isConnected) {
    return
  }

  await switchActiveDirector(scopeDirector)
}

async function refresh() {
  directorErrors.value = []

  if (activeDirectors.value.length === 0) {
    aggregateCommands.value = []
    acl.reset()
    return
  }

  if (isCommonAcl.value) {
    aggregateLoading.value = true
    aggregateError.value = null
    try {
      const credentials = auth.getCredentials()
      if (!credentials?.password) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedAcl(credentials, activeDirectors.value)
      aggregateCommands.value = result.commands
      directorErrors.value = result.directorErrors
    } catch (reason) {
      aggregateCommands.value = []
      aggregateError.value = reason?.message ?? String(reason)
    } finally {
      aggregateLoading.value = false
    }
    return
  }

  aggregateCommands.value = []
  aggregateError.value = null
  await ensureSingleScopeDirector()
  await acl.refresh()
}

const filterOptions = computed(() => [
  { label: t('All'), value: 'all' },
  { label: t('Allowed'), value: 'allowed' },
  { label: t('Denied'), value: 'denied' },
])

const filteredCommands = computed(() => {
  const needle = search.value.trim().toLowerCase()

  return currentCommands.value.filter((item) => {
    if (filter.value === 'allowed' && !item.permission) return false
    if (filter.value === 'denied' && item.permission) return false

    if (!needle) return true
    return [
      item.command,
      item.description,
      item.arguments,
      item.category,
      item.director,
    ].some(value => String(value ?? '').toLowerCase().includes(needle))
  })
})

const commandGroups = computed(() => (
  [t('Commands'), t('Dot commands'), 'BVFS']
    .map(name => ({
      name,
      items: filteredCommands.value.filter(item => t(item.category) === name),
    }))
    .filter(group => group.items.length > 0)
))

watch(() => director.isConnected, (connected) => {
  if (connected && !isCommonAcl.value) {
    acl.ensureLoaded()
  }
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  refresh()
})

onMounted(() => {
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  refresh()
})
</script>

<style scoped>
.acl-arguments {
  white-space: pre-wrap;
  word-break: break-word;
  font-family: monospace;
}
</style>
