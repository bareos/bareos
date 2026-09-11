<!--
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
<!--
  Placeholder rows shown in place of a q-table's body while its initial
  data load is in flight (i.e. no rows are available yet). Mimics the
  eventual table shape so the layout doesn't jump once real data arrives.
-->
<template>
  <div class="table-skeleton" role="presentation">
    <div v-for="row in rows" :key="row" class="table-skeleton__row">
      <q-skeleton
        v-for="col in columns"
        :key="col"
        type="text"
        class="table-skeleton__cell"
        :style="{ width: cellWidth(col) }"
      />
    </div>
  </div>
</template>

<script setup>
const props = defineProps({
  // Number of skeleton columns to render per row.
  columns: { type: Number, default: 4 },
  // Number of skeleton rows to render.
  rows: { type: Number, default: 5 },
})

// Vary widths a bit so the placeholder doesn't look like a uniform grid.
function cellWidth(col) {
  const widths = ['85%', '60%', '70%', '50%', '90%', '65%']
  return widths[(col + props.columns) % widths.length]
}
</script>

<style scoped>
.table-skeleton {
  padding: 12px 16px;
}

.table-skeleton__row {
  display: flex;
  align-items: center;
  gap: 24px;
  padding: 8px 0;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
}

.table-skeleton__row:last-child {
  border-bottom: none;
}

.table-skeleton__cell {
  flex: 1 1 0;
  min-width: 0;
}
</style>
