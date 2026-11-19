<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
-->

<template>
  <div class="editable-comment row items-center no-wrap" @click.stop>
    <span class="editable-comment-text" :title="comment">{{ comment }}</span>
    <q-btn
      flat round dense size="xs" icon="edit"
      class="editable-comment-edit"
      :title="t('Edit comment')"
      :aria-label="t('Edit comment')"
      :data-testid="testid"
      @click.stop="open = true"
    />

    <CommentEditDialog
      v-model="open"
      :title="title"
      :comment="comment"
      :save="save"
    />
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useI18n } from 'vue-i18n'

import CommentEditDialog from './CommentEditDialog.vue'

defineProps({
  comment: { type: String, default: '' },
  title: { type: String, default: '' },
  // async (comment) => void; a rejection keeps the dialog open.
  save: { type: Function, required: true },
  testid: { type: String, default: 'comment-edit-open' },
})

const { t } = useI18n()
const open = ref(false)
</script>

<style scoped>
.editable-comment {
  gap: 2px;
}

.editable-comment-text {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.editable-comment-edit {
  opacity: 0;
  transition: opacity 0.12s ease;
}

.editable-comment:hover .editable-comment-edit,
.editable-comment-edit:focus-visible {
  opacity: 1;
}
</style>
