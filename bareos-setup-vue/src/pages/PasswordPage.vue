<template>
  <div>
    <div class="step-header">Configure Passwords</div>
    <p class="text-body2 q-mb-md">
      Set passwords for Bareos components. Use the generate button for secure
      random passwords.
    </p>

    <q-card flat bordered class="q-mb-lg">
      <q-card-section class="q-gutter-md">
        <div v-for="field in passwordFields" :key="field.key">
          <q-input
            v-model="store.passwords[field.key]"
            :label="field.label"
            type="password"
            dense outlined
          >
            <template #append>
              <q-btn flat round dense icon="casino" size="sm"
                     @click="generate(field.key)" />
            </template>
          </q-input>
        </div>
      </q-card-section>
    </q-card>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <div class="row q-gutter-sm">
        <q-btn label="Generate All" flat icon="casino" @click="generateAll" />
        <q-btn label="Continue" color="primary" icon-right="arrow_forward"
               :disable="!allSet" @click="next" />
      </div>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'

const router = useRouter()
const store  = useSetupStore()

const passwordFields = computed(() => {
  const fields = []
  if (store.components.director)   fields.push({ key: 'director',   label: 'Director password' })
  if (store.components.storage)    fields.push({ key: 'storage',    label: 'Storage Daemon password' })
  if (store.components.filedaemon) fields.push({ key: 'filedaemon', label: 'File Daemon password' })
  if (store.components.webui)      fields.push({ key: 'webui',      label: 'WebUI password' })
  return fields
})

const allSet = computed(() =>
  passwordFields.value.every(f => store.passwords[f.key]?.length >= 8)
)

function randomPwd(len = 24) {
  const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789!@#$%^&*'
  let pw = ''
  const arr = new Uint8Array(len)
  crypto.getRandomValues(arr)
  arr.forEach(b => (pw += chars[b % chars.length]))
  return pw
}

function generate(key) {
  store.passwords[key] = randomPwd()
}

function generateAll() {
  passwordFields.value.forEach(f => generate(f.key))
}

onMounted(() => {
  // Auto-generate on first visit if empty
  if (passwordFields.value.every(f => !store.passwords[f.key])) generateAll()
})

function back() { store.prevStep(); router.push('/database') }
function next() { store.nextStep(); router.push('/summary') }
</script>
