<template>
  <router-view />
</template>

<script setup>
import { onMounted } from 'vue'
import { useAuthStore }     from './stores/auth.js'
import { useDirectorStore } from './stores/director.js'

const auth     = useAuthStore()
const director = useDirectorStore()

// On page reload the WS connection is gone but sessionStorage still holds
// the credentials.  Reconnect automatically so pages can load their data.
onMounted(() => {
  if (auth.isLoggedIn && !director.isConnected) {
    const creds = auth.getCredentials()
    if (creds?.password) director.connect(creds)
  }
})
</script>
