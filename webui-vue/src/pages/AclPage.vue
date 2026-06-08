<template>
  <q-page class="q-pa-md">
    <DirectorErrorsBanner :errors="directorErrors" />

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
                <DirectorBadge icon="dns" :director="aclScopeLabel">
                  {{ aclScopeLabel }}
                </DirectorBadge>
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
                data-testid="acl-filter"
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

            <div v-else-if="isCommonAcl" class="column q-gutter-lg">
              <q-card
                v-for="section in directorCommandSections"
                :key="section.director"
                flat
                bordered
              >
                <q-card-section class="row items-center q-py-sm">
                  <DirectorBadge icon="dns" :director="section.director">
                    {{ section.director }}
                  </DirectorBadge>
                  <q-space />
                  <q-chip dense square color="positive" text-color="white" icon="check_circle">
                    {{ section.allowedCount }} {{ t('allowed') }}
                  </q-chip>
                  <q-chip dense square color="negative" text-color="white" icon="block">
                    {{ section.deniedCount }} {{ t('denied') }}
                  </q-chip>
                </q-card-section>
                <q-separator />
                <q-card-section class="column q-gutter-md">
                  <q-card
                    v-for="group in section.groups"
                    :key="`${section.director}:${group.name}`"
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
                </q-card-section>
              </q-card>
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
                        <DirectorBadge
                          v-if="isCommonAcl"
                          :director="item.director"
                          class="q-ml-sm"
                        />
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
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useDirectorAclStore } from '../stores/directorAcl.js'
import DirectorBadge from '../components/DirectorBadge.vue'
import DirectorErrorsBanner from '../components/DirectorErrorsBanner.vue'

const auth = useAuthStore()
const director = useDirectorStore()
const acl = useDirectorAclStore()
const { t } = useI18n()

const search = ref('')
const filter = ref('all')
const aggregateCommands = ref([])
const aggregateLoading = ref(false)
const aggregateError = ref(null)
const directorErrors = ref([])

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonAcl,
  scopeLabel: aclScopeLabel,
  syncSelectedDirectors,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

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
      director: auth.user?.director ?? directorOptions.value[0]?.value ?? '',
      scopeKey: `${auth.user?.director ?? directorOptions.value[0]?.value ?? ''}:${item.command}`,
    }))
))
const allowedCount = computed(() => currentCommands.value.filter(item => item.permission).length)
const deniedCount = computed(() => currentCommands.value.filter(item => !item.permission).length)

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

function buildCommandGroups(items) {
  return [t('Commands'), t('Dot commands'), 'BVFS']
    .map(name => ({
      name,
      items: items.filter(item => t(item.category) === name),
    }))
    .filter(group => group.items.length > 0)
}

const commandGroups = computed(() => (
  buildCommandGroups(filteredCommands.value)
))

const directorCommandSections = computed(() => (
  activeDirectors.value
    .map((directorName) => {
      const items = filteredCommands.value.filter(item => item.director === directorName)
      return {
        director: directorName,
        groups: buildCommandGroups(items),
        allowedCount: items.filter(item => item.permission).length,
        deniedCount: items.filter(item => !item.permission).length,
      }
    })
    .filter(section => section.groups.length > 0)
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
