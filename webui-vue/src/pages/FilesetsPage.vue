<template>
  <q-page class="q-pa-md">
    <DirectorScopePanel
      v-model="selectedDirectorsModel"
      :title="t('Filesets Scope')"
      :summary-label="filesetsScopeLabel"
      :options="directorOptions"
      :help-text="t('Select the directors that contribute to the filesets list.')"
      :errors="directorErrors"
      data-test-id="filesets-directors"
    />

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
import { useDirectorScope } from '../composables/useDirectorScope.js'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import DirectorScopePanel from '../components/DirectorScopePanel.vue'

const auth = useAuthStore()
const director = useDirectorStore()
const { t } = useI18n()
const rawFilesets = ref([])
const loading = ref(false)
const error = ref(null)
const directorErrors = ref([])

const {
  directorOptions,
  selectedDirectorsModel,
  activeDirectors,
  isCommonScope: isCommonFilesets,
  scopeLabel: filesetsScopeLabel,
  syncSelectedDirectors,
  ensureSingleScopeDirector,
} = useDirectorScope({ t })

const showDirectorColumn = computed(() => isCommonFilesets.value)

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
      if (!credentials?.password) {
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
  { name: 'createtime', label: t('Created'),     field: 'createtime', align: 'left', sortable: true },
  { name: 'md5',        label: t('Config Hash'), field: 'md5',        align: 'left', sortable: true },
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
