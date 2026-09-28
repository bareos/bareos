<template>
  <router-view />
  <div
    v-if="buildInfo.promote"
    class="unsupported-build-sash"
    data-testid="unsupported-build-sash"
    aria-hidden="true"
  >
    {{ t('Unsupported') }}
  </div>
</template>

<script setup>
import { onMounted, onUnmounted, watch } from 'vue'
import { useRoute } from 'vue-router'
import { useI18n } from 'vue-i18n'
import { useAuthStore }     from './stores/auth.js'
import { useBuildInfoStore } from './stores/buildInfo.js'
import { useDirectorStore } from './stores/director.js'
import { formatDocumentTitle } from './utils/commercialOffering.js'
import {
  CONSOLE_POPUP_AUTH_REQUEST,
  CONSOLE_POPUP_AUTH_RESPONSE,
} from './utils/consolePopupAuth.js'

const auth     = useAuthStore()
const director = useDirectorStore()
const buildInfo = useBuildInfoStore()
const route = useRoute()
const { t } = useI18n()

watch(
  () => [route.meta?.title, buildInfo.promote],
  ([pageTitle, unsupported]) => {
    document.title = formatDocumentTitle(pageTitle, unsupported)
    document.body.classList.toggle('bareos-unsupported-build', unsupported)
  },
  { immediate: true },
)

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
})

watch(
  () => auth.getCredentials(),
  (creds) => {
    if (!creds?.password || director.isConnected) {
      return
    }

    if (director.status === 'connecting' || director.status === 'authenticating') {
      return
    }

    director.connect(creds)
  },
  { immediate: true }
)

onUnmounted(() => {
  window.removeEventListener('message', handleConsolePopupAuth)
})
</script>

<style>
.unsupported-build-sash {
  position: fixed;
  top: 30px;
  left: -56px;
  z-index: 9000;
  width: 200px;
  padding: 3px 0;
  transform: rotate(-45deg);
  background: #ef6c00;
  color: white;
  box-shadow: 0 2px 6px rgba(0, 0, 0, 0.3);
  font-size: 0.66rem;
  font-weight: 700;
  letter-spacing: 0.08em;
  text-align: center;
  text-transform: uppercase;
  pointer-events: none;
  opacity: 0.92;
}
</style>
