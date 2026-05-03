<template>
  <q-page class="q-pa-md">
    <q-card v-if="directorOptions.length > 1" flat bordered class="q-mb-md bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Filesets Scope') }}</span>
        <q-space />
        <q-chip dense square color="white" text-color="primary" :label="filesetsScopeLabel" />
      </q-card-section>
      <q-card-section>
        <q-select
          v-model="selectedDirectorsModel"
          data-testid="filesets-directors"
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
          {{ t('Select the directors that contribute to the filesets list.') }}
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

    <q-card flat bordered class="bareos-panel">
      <q-card-section class="panel-header row items-center">
        <span>{{ t('Filesets') }}</span>
        <q-space />
        <q-btn flat round dense icon="refresh" color="white" @click="refresh" />
      </q-card-section>
      <q-card-section class="q-pa-none">
        <q-banner v-if="error" dense class="bg-negative text-white">{{ error }}</q-banner>
        <q-table
          :rows="filesets"
          :columns="columns"
          row-key="scopeKey"
          dense flat
          :loading="loading"
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
                <template v-else-if="col.name === 'director'">
                  <q-chip dense square color="primary" text-color="white" :label="col.value" />
                </template>
                <template v-else>{{ col.value }}</template>
              </q-td>
            </q-tr>
            <q-tr v-if="props.expand" :props="props" class="bg-grey-1">
              <q-td colspan="100%" class="q-pa-md">
                <div class="row q-col-gutter-md text-caption">
                  <div class="col-12 col-md-4" v-if="props.row.include.length">
                    <div class="text-weight-bold q-mb-xs">{{ t('Include') }}</div>
                    <div v-for="p in props.row.include" :key="p" class="q-ml-sm text-mono">{{ p }}</div>
                  </div>
                  <div class="col-12 col-md-4" v-if="props.row.exclude.length">
                    <div class="text-weight-bold q-mb-xs">{{ t('Exclude') }}</div>
                    <div v-for="p in props.row.exclude" :key="p" class="q-ml-sm text-mono">{{ p }}</div>
                  </div>
                  <div class="col-12 col-md-4" v-if="props.row.options">
                    <div class="text-weight-bold q-mb-xs">{{ t('Options') }}</div>
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
import { computed, onMounted, ref, watch } from 'vue'
import { useI18n } from 'vue-i18n'
import { directorCollection } from '../composables/useDirectorFetch.js'
import { fetchAggregatedFilesets } from '../composables/filesetsAggregate.js'
import { switchActiveDirector } from '../composables/useDirectorSession.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'

const auth = useAuthStore()
const director = useDirectorStore()
const settings = useSettingsStore()
const { t } = useI18n()
const rawFilesets = ref([])
const loading = ref(false)
const error = ref(null)
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

const isCommonFilesets = computed(() => activeDirectors.value.length > 1)
const showDirectorColumn = computed(() => isCommonFilesets.value)
const filesetsScopeLabel = computed(() => (
  isCommonFilesets.value
    ? `${activeDirectors.value.length} ${t('directors selected')}`
    : (activeDirectors.value[0] ?? t('No director selected'))
))

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
  loading.value = true
  error.value = null
  directorErrors.value = []
  try {
    if (activeDirectors.value.length === 0) {
      rawFilesets.value = []
      return
    }

    if (isCommonFilesets.value) {
      const credentials = auth.getCredentials()
      if (!credentials) {
        throw new Error(t('Not logged in.'))
      }

      const result = await fetchAggregatedFilesets(credentials, activeDirectors.value)
      rawFilesets.value = result.filesets
      directorErrors.value = result.directorErrors
      return
    }

    const currentDirector = activeDirectors.value[0]
    await ensureSingleScopeDirector()
    const result = await director.call('list filesets')
    rawFilesets.value = directorCollection(result?.filesets).map(entry => ({
      name: entry.fileset ?? entry.name ?? '',
      description: entry.description ?? '',
      createtime: entry.createtime ?? '',
      md5: entry.md5 ?? '',
      include: entry.include ?? [],
      exclude: entry.exclude ?? [],
      options: entry.options ?? '',
      director: currentDirector,
      scopeKey: `${currentDirector}:${entry.fileset ?? entry.name ?? ''}`,
    }))
  } catch (reason) {
    error.value = reason?.message ?? String(reason)
  } finally {
    loading.value = false
  }
}

const filesets = computed(() => directorCollection(rawFilesets.value))

const columns = computed(() => [
  ...(showDirectorColumn.value ? [{
    name: 'director', label: t('Director'), field: 'director', align: 'left', sortable: true,
  }] : []),
  { name: 'name',       label: t('Name'),        field: 'name',       align: 'left', sortable: true },
  { name: 'createtime', label: t('Created'),     field: 'createtime', align: 'left' },
  { name: 'md5',        label: t('Config Hash'), field: 'md5',        align: 'left' },
])

onMounted(() => {
  director.fetchAvailableDirectors().catch(() => {})
  syncSelectedDirectors()
  refresh()
})

watch(() => directorOptions.value, () => {
  syncSelectedDirectors()
})

watch(() => activeDirectors.value.join('\u0000'), () => {
  refresh()
})
</script>
