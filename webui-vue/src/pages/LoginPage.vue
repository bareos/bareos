<template>
  <q-page class="flex flex-center">
    <div style="width: 380px; padding: 24px">
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
            <q-input
              v-model="host"
              label="Director Host"
              outlined dense
              class="q-mb-sm"
              placeholder="localhost"
              autocomplete="off"
            >
              <template #append>
                <q-input
                  v-model.number="port"
                  dense borderless
                  style="width:60px"
                  type="number"
                  :min="1" :max="65535"
                />
              </template>
            </q-input>
            <q-select
              v-model="directorRef"
              :options="directors"
              label="Director Name"
              outlined dense
              emit-value map-options
              class="q-mb-sm"
              use-input
              input-debounce="0"
              @new-value="(val, done) => done(val)"
            />
            <q-input
              v-model="username"
              label="Username"
              outlined dense
              class="q-mb-sm"
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

            <q-banner v-if="errorMsg" dense class="bg-negative text-white q-mb-md rounded-borders">
              <template #avatar><q-icon name="error" /></template>
              {{ errorMsg }}
            </q-banner>

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
import { ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'

const auth     = useAuthStore()
const director = useDirectorStore()
const router   = useRouter()

const host      = ref(import.meta.env.VITE_DIRECTOR_HOST || 'localhost')
const port      = ref(Number(import.meta.env.VITE_DIRECTOR_PORT) || 9101)
const dirName   = 'bareos-dir'
const directors = ref(['bareos-dir', 'backup-dir', 'dr-director'])
const directorRef = ref(dirName)
const username  = ref('admin')
const password  = ref('')
const loading   = ref(false)
const errorMsg  = ref(null)

async function doLogin() {
  loading.value = true
  errorMsg.value = null

  // Connect WebSocket proxy → director
  director.connect({
    username:  username.value,
    password:  password.value,
    director:  directorRef.value,
    host:      host.value,
    port:      port.value,
  })

  // Wait up to 8 s for auth result
  const ok = await new Promise((resolve) => {
    const stop = watch(
      () => director.status,
      (s) => {
        if (s === 'connected')   { stop(); resolve(true)  }
        if (s === 'error')       { stop(); resolve(false) }
      }
    )
    setTimeout(() => { stop(); resolve(false) }, 8000)
  })

  if (!ok) {
    errorMsg.value = director.errorMsg || 'Could not connect to director. Is the proxy running?'
    loading.value = false
    return
  }

  auth.login(username.value, directorRef.value, password.value, host.value, port.value)
  loading.value = false
  router.push({ name: 'dashboard' })
}
</script>

