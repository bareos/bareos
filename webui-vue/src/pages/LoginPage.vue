<template>
  <q-page class="flex flex-center">
    <div style="width: 360px; padding: 24px">
      <div class="text-center q-mb-lg">
        <q-icon name="shield" size="48px" color="white" />
        <div class="text-h5 text-white text-weight-bold q-mt-sm">BAREOS</div>
        <div class="text-subtitle2 text-white">Backup Archiving Recovery Open Sourced</div>
      </div>

      <q-card flat bordered>
        <q-card-section class="bg-primary text-white">
          <div class="text-h6">Sign In</div>
        </q-card-section>
        <q-card-section>
          <q-form @submit.prevent="doLogin">
            <q-select
              v-model="director"
              :options="directors"
              label="Director"
              outlined dense
              class="q-mb-md"
              emit-value map-options
            />
            <q-input
              v-model="username"
              label="Username"
              outlined dense
              class="q-mb-md"
              autocomplete="username"
            />
            <q-input
              v-model="password"
              label="Password"
              type="password"
              outlined dense
              class="q-mb-md"
              autocomplete="current-password"
            />
            <q-btn
              type="submit"
              label="Login"
              color="primary"
              class="full-width"
              :loading="loading"
            />
          </q-form>
        </q-card-section>
      </q-card>

      <div class="text-center text-white q-mt-md" style="font-size:0.75rem; opacity:0.7">
        Bareos WebUI &copy; 2013–2026 Bareos GmbH &amp; Co. KG
      </div>
    </div>
  </q-page>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth.js'

const auth = useAuthStore()
const router = useRouter()

const director = ref('bareos-dir')
const directors = ['bareos-dir', 'backup-dir', 'dr-director']
const username = ref('admin')
const password = ref('')
const loading = ref(false)

async function doLogin() {
  loading.value = true
  // Simulate auth – replace with real API call
  await new Promise(r => setTimeout(r, 400))
  auth.login(username.value, director.value)
  loading.value = false
  router.push({ name: 'dashboard' })
}
</script>
