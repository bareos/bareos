import { defineStore } from 'pinia'
import { ref, watch } from 'vue'
import { useQuasar } from 'quasar'

const LS_KEY = 'bareos_settings'

const DEFAULTS = {
  refreshInterval: 10,  // seconds
  darkMode: false,
  relativeTime: false,  // show timestamps as relative ("2 hours ago") or absolute
}

function loadFromStorage() {
  try {
    const raw = localStorage.getItem(LS_KEY)
    if (raw) return { ...DEFAULTS, ...JSON.parse(raw) }
  } catch { /* ignore */ }
  return { ...DEFAULTS }
}

export const useSettingsStore = defineStore('settings', () => {
  const saved = loadFromStorage()

  const refreshInterval = ref(saved.refreshInterval)
  const darkMode        = ref(saved.darkMode)
  const relativeTime    = ref(saved.relativeTime)

  function save() {
    localStorage.setItem(LS_KEY, JSON.stringify({
      refreshInterval: refreshInterval.value,
      darkMode:        darkMode.value,
      relativeTime:    relativeTime.value,
    }))
  }

  watch(refreshInterval, save)
  watch(darkMode, save)
  watch(relativeTime, save)

  return { refreshInterval, darkMode, relativeTime }
})
