import { computed } from 'vue'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'
import { switchActiveDirector } from './useDirectorSession.js'
import { buildDirectorOptions } from '../utils/director.js'

export function useDirectorScope({
  t,
  buildOptions = null,
  syncEmptySelection = 'fallback',
} = {}) {
  const auth = useAuthStore()
  const director = useDirectorStore()
  const settings = useSettingsStore()

  const fallbackDirector = computed(() => (
    auth.user?.director || settings.directorName || ''
  ))

  const directorOptions = computed(() => (
    buildOptions
      ? buildOptions({
        auth,
        director,
        settings,
        fallbackDirector: fallbackDirector.value,
      })
      : buildDirectorOptions({
        availableDirectors: director.availableDirectors,
        selectedDirectors: settings.selectedDirectors,
        currentDirector: auth.user?.director,
        fallbackDirector: settings.directorName,
      })
  ))

  function validDirectors() {
    return directorOptions.value.map(option => option.value)
  }

  function applyEmptySelection(mode = 'fallback') {
    const valid = validDirectors()
    if (mode === 'all' && valid.length > 0) {
      settings.setSelectedDirectors(valid)
      return
    }

    settings.setSelectedDirectors(fallbackDirector.value ? [fallbackDirector.value] : [])
  }

  function syncSelectedDirectors() {
    const valid = validDirectors()
    const selected = settings.selectedDirectors.filter(value => valid.includes(value))

    if (selected.length > 0) {
      if (selected.length !== settings.selectedDirectors.length) {
        settings.setSelectedDirectors(selected)
      }
      return
    }

    applyEmptySelection(syncEmptySelection)
  }

  const selectedDirectorsModel = computed({
    get: () => settings.selectedDirectors,
    set: (value) => {
      const selected = Array.isArray(value) ? value : []
      if (selected.length > 0) {
        settings.setSelectedDirectors(selected)
        return
      }

      applyEmptySelection('fallback')
    },
  })

  const activeDirectors = computed(() => {
    const valid = validDirectors()
    const selected = settings.selectedDirectors.filter(value => valid.includes(value))

    if (selected.length > 0) {
      return selected
    }

    return fallbackDirector.value ? [fallbackDirector.value] : []
  })

  const isCommonScope = computed(() => activeDirectors.value.length > 1)
  const isSingleDirectorScope = computed(() => activeDirectors.value.length === 1)
  const scopeLabel = computed(() => (
    isCommonScope.value
      ? `${activeDirectors.value.length} ${t('directors selected')}`
      : (activeDirectors.value[0] ?? t('No director selected'))
  ))

  async function ensureScopeDirector(targetDirector) {
    if (!targetDirector) {
      return
    }

    if (auth.user?.director === targetDirector && director.isConnected) {
      return
    }

    await switchActiveDirector(targetDirector)
  }

  async function ensureSingleScopeDirector() {
    if (!isSingleDirectorScope.value) {
      return
    }

    await ensureScopeDirector(activeDirectors.value[0])
  }

  return {
    directorOptions,
    selectedDirectorsModel,
    activeDirectors,
    isCommonScope,
    isSingleDirectorScope,
    scopeLabel,
    syncSelectedDirectors,
    ensureScopeDirector,
    ensureSingleScopeDirector,
  }
}
