<template>
  <q-page class="flex flex-center">
    <div style="width: 380px; padding: 24px">
      <div class="text-center q-mb-lg">
        <img :src="bareosLogo" alt="Bareos" style="height:48px" />
        <div class="text-h5 text-white text-weight-bold q-mt-sm">BAREOS</div>
        <div class="text-subtitle2 text-white">Backup Archiving Recovery Open Sourced</div>
      </div>

      <q-card flat bordered>
        <q-card-section class="bg-primary text-white">
          <div class="text-h6">Sign In</div>
        </q-card-section>
        <q-card-section>
          <q-form data-testid="login-form" @submit.prevent="doLogin">
            <q-input
              v-model="username"
              data-testid="login-username"
              label="Username"
              outlined dense
              class="q-mb-sm"
              autocomplete="username"
            />
            <q-input
              v-model="password"
              data-testid="login-password"
              label="Password"
              type="password"
              outlined dense
              class="q-mb-md"
              autocomplete="current-password"
            />

            <q-expansion-item
              data-testid="login-advanced"
              label="Advanced connection settings"
              icon="tune"
              dense
              header-class="text-primary"
              class="q-mb-md rounded-borders"
            >
              <div class="q-pt-sm">
                <q-input
                  v-model="host"
                  data-testid="login-host"
                  label="Director Host"
                  outlined dense
                  class="q-mb-sm"
                  placeholder="localhost"
                  autocomplete="off"
                >
                  <template #append>
                    <q-input
                      v-model.number="port"
                      data-testid="login-port"
                      dense borderless
                      style="width:60px"
                      type="number"
                      :min="1" :max="65535"
                    />
                  </template>
                </q-input>
                <q-input
                  v-model="directorRef"
                  data-testid="login-director"
                  label="Director Name"
                  outlined dense
                />
              </div>
            </q-expansion-item>

            <q-banner v-if="errorMsg" data-testid="login-error" dense class="bg-negative text-white q-mb-md rounded-borders">
              <template #avatar><q-icon name="error" /></template>
              {{ errorMsg }}
            </q-banner>

            <q-btn
              data-testid="login-submit"
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
import {
  DEFAULT_DIRECTOR_HOST,
  DEFAULT_DIRECTOR_NAME,
  DEFAULT_DIRECTOR_PORT,
  useAuthStore,
} from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import bareosLogo from '../assets/bareos-logo-small.png'

const auth     = useAuthStore()
const director = useDirectorStore()
const router   = useRouter()

const host      = ref(DEFAULT_DIRECTOR_HOST)
const port      = ref(DEFAULT_DIRECTOR_PORT)
const directorRef = ref(DEFAULT_DIRECTOR_NAME)
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
