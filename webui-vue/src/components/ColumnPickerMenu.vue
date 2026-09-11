<template>
  <q-btn flat round dense icon="view_column" :title="t('Choose columns')">
    <q-menu anchor="bottom right" self="top right">
      <q-list dense style="min-width:200px">
        <q-item-label header>{{ t('Show columns') }}</q-item-label>
        <q-item
          v-for="column in columns"
          :key="column.name"
          clickable
          @click="$emit('toggle', column.name)"
        >
          <q-item-section side>
            <q-checkbox
              dense
              :model-value="column.visible"
              @update:model-value="$emit('toggle', column.name)"
              @click.stop
            />
          </q-item-section>
          <q-item-section>{{ column.label }}</q-item-section>
        </q-item>
      </q-list>
    </q-menu>
  </q-btn>
</template>

<script setup>
import { useI18n } from 'vue-i18n'

defineProps({
  columns: {
    type: Array,
    required: true,
    // Each entry: { name, label, visible }
  },
})
defineEmits(['toggle'])

const { t } = useI18n()
</script>
