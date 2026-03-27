<template>
  <div>
    <div class="step-header">OS Detection</div>
    <p class="text-body2 q-mb-md">
      Detecting your operating system and package manager.
    </p>

    <div v-if="detecting" class="row items-center q-gutter-sm q-mb-lg">
      <q-spinner color="primary" size="24px" />
      <span>Detecting…</span>
    </div>

    <q-card v-else-if="store.osInfo" flat bordered class="q-mb-lg">
      <q-card-section>
        <div class="row q-gutter-md">
          <div class="col">
            <div class="text-caption text-grey">Distribution</div>
            <div class="text-body1">{{ store.osInfo.pretty_name || store.osInfo.distro }}</div>
          </div>
          <div class="col">
            <div class="text-caption text-grey">Version</div>
            <div class="text-body1">{{ store.osInfo.version }}</div>
          </div>
          <div class="col">
            <div class="text-caption text-grey">Architecture</div>
            <div class="text-body1">{{ store.osInfo.arch }}</div>
          </div>
          <div class="col">
            <div class="text-caption text-grey">Package Manager</div>
            <div class="text-body1">{{ store.osInfo.pkg_mgr }}</div>
          </div>
        </div>
      </q-card-section>
    </q-card>

    <q-banner v-else-if="error" class="bg-negative text-white q-mb-lg" rounded>
      <template #avatar><q-icon name="error" /></template>
      {{ error }}
    </q-banner>

    <q-banner v-if="store.osInfo && !supported" class="bg-warning text-white q-mb-lg" rounded>
      <template #avatar><q-icon name="warning" /></template>
      Your OS may not be fully supported. Proceed with caution.
    </q-banner>

    <div class="row justify-between">
      <q-btn flat label="Back" icon="arrow_back" @click="back" />
      <q-btn label="Continue" color="primary" icon-right="arrow_forward"
             :disable="!store.osInfo" @click="next" />
    </div>
  </div>
</template>

<script setup>
import { ref, computed, watch, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useSetupStore } from '../stores/setup.js'
import { useSetupWs } from '../composables/useSetupWs.js'

const router   = useRouter()
const store    = useSetupStore()
const { send, messages } = useSetupWs()

const detecting = ref(true)
const error     = ref('')

const SUPPORTED = ['ubuntu', 'debian', 'centos', 'rhel', 'fedora', 'opensuse', 'sles']
const supported = computed(() =>
  store.osInfo ? SUPPORTED.some(d => store.osInfo.distro?.toLowerCase().includes(d)) : false
)

onMounted(() => {
  send({ action: 'detect_os' })
  watch(messages, (msgs) => {
    const last = msgs[msgs.length - 1]
    if (!last) return
    if (last.type === 'os_info') {
      store.osInfo = last
      detecting.value = false
    } else if (last.type === 'error') {
      error.value = last.message
      detecting.value = false
    }
  }, { deep: true })
})

function back() { store.prevStep(); router.push('/welcome') }
function next() { store.nextStep(); router.push('/components') }
</script>
