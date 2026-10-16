/*
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
 */

// Shared "now" indicator used by both the Scheduler Preview and the Job
// Timeline: a live line drawn across today's 24h axis (Month/Week compact
// cell and the enlarged Day view). Updated once a minute — fine-grained
// enough without the overhead of a per-second re-render.

import { ref, computed, onMounted, onBeforeUnmount } from 'vue'
import { makeDateStr } from '../utils/calendarGrid.js'

export function useNowLine(t) {
  const nowTick = ref(Date.now())
  let nowTimer = null

  onMounted(() => {
    nowTimer = setInterval(() => { nowTick.value = Date.now() }, 60_000)
  })
  onBeforeUnmount(() => {
    if (nowTimer) clearInterval(nowTimer)
  })

  const nowLinePercent = computed(() => {
    const d = new Date(nowTick.value)
    return ((d.getHours() * 60 + d.getMinutes()) / 1440) * 100
  })

  const nowLineLabel = computed(() => {
    const d = new Date(nowTick.value)
    const time = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`
    return t('Now — {time}', { time })
  })

  const todayDateStr = computed(() => {
    const d = new Date(nowTick.value)
    return makeDateStr(d.getFullYear(), d.getMonth(), d.getDate())
  })

  return { nowTick, nowLinePercent, nowLineLabel, todayDateStr }
}
