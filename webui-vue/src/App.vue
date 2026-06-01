<template>
  <router-view />
</template>

<script setup>
import { onMounted, onUnmounted } from 'vue'
import { useAuthStore }     from './stores/auth.js'
import { useDirectorStore } from './stores/director.js'
import {
  CONSOLE_POPUP_AUTH_REQUEST,
  CONSOLE_POPUP_AUTH_RESPONSE,
} from './utils/consolePopupAuth.js'

const auth     = useAuthStore()
const director = useDirectorStore()

function handleConsolePopupAuth(event) {
  if (event.origin !== window.location.origin) {
    return
  }

  if (event.data?.type !== CONSOLE_POPUP_AUTH_REQUEST) {
    return
  }

  const creds = auth.getCredentials()
  if (!creds?.password || typeof event.source?.postMessage !== 'function') {
    return
  }

  const requestedDirector = String(event.data.director ?? '').trim()

  event.source.postMessage({
    type: CONSOLE_POPUP_AUTH_RESPONSE,
    credentials: {
      ...creds,
      director: requestedDirector || creds.director,
    },
  }, window.location.origin)
}

onMounted(() => {
  window.addEventListener('message', handleConsolePopupAuth)
  if (auth.isLoggedIn && !director.isConnected) {
    const creds = auth.getCredentials()
    if (creds?.password) director.connect(creds)
  }
})

onUnmounted(() => {
  window.removeEventListener('message', handleConsolePopupAuth)
})
</script>
