<template>
  <q-select
    v-model="model"
    :options="localeOptions"
    :label="label"
    outlined
    dense
    emit-value
    map-options
  >
    <template #selected-item="scope">
      <div class="row items-center no-wrap">
        <span class="language-flag q-mr-sm" aria-hidden="true">
          {{ scope.opt.flag }}
        </span>
        <span>{{ scope.opt.label }}</span>
      </div>
    </template>

    <template #option="scope">
      <q-item v-bind="scope.itemProps">
        <q-item-section avatar>
          <span class="language-flag" aria-hidden="true">{{ scope.opt.flag }}</span>
        </q-item-section>
        <q-item-section>
          <q-item-label>{{ scope.opt.label }}</q-item-label>
        </q-item-section>
      </q-item>
    </template>
  </q-select>
</template>

<script setup>
import { computed } from 'vue'
import { useI18n } from 'vue-i18n'
import { localeOptions } from '../utils/locales.js'

const props = defineProps({
  modelValue: {
    type: String,
    default: '',
  },
  label: {
    type: String,
    default: '',
  },
})

const emit = defineEmits(['update:modelValue'])
const { t } = useI18n()

const model = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value),
})

const label = computed(() => props.label || t('Language'))
</script>

<style scoped>
.language-flag {
  font-size: 1.25rem;
  line-height: 1;
}
</style>
