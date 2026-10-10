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

import { onScopeDispose, ref } from 'vue'

export function useSubscriptionAccounting({ call, onComplete, canRun, t }) {
  const busy = ref(false)
  const state = ref('')
  const error = ref(null)
  let generation = 0
  let timer

  function stop() {
    generation++
    clearTimeout(timer)
    busy.value = false
    state.value = ''
    error.value = null
  }

  async function poll(token) {
    try {
      const result = await call('status subscriptions accounting')
      if (token !== generation) return
      let snapshot = result?.accounting_snapshot
      if (snapshot?.refresh_thread_state === 'idle') {
        // Metadata is read before worker state. Fetch again after observing
        // idle so a just-completed retry cannot retain the previous error.
        const confirmed = await call('status subscriptions accounting')
        if (token !== generation) return
        snapshot = confirmed?.accounting_snapshot
      }
      if (!snapshot || !['idle', 'queued', 'running', 'unavailable'].includes(snapshot.refresh_thread_state)) {
        throw new Error(t('Accounting worker status is unavailable.'))
      }
      state.value = snapshot.refresh_thread_state
      if (state.value === 'unavailable') {
        throw new Error(t('Accounting background worker is unavailable.'))
      }
      if (state.value === 'idle') {
        await onComplete()
        if (token !== generation) return
        if (snapshot.last_refresh_error) throw new Error(snapshot.last_refresh_error)
        if (!snapshot.available) throw new Error(t('No accounting snapshot is available.'))
        busy.value = false
        return
      }
      timer = setTimeout(() => poll(token), 3000)
    } catch (e) {
      if (token !== generation) return
      error.value = e.message
      busy.value = false
    }
  }

  async function run() {
    if (busy.value) return
    if (!canRun()) {
      error.value = t('Accounting refresh is unavailable or not permitted.')
      return
    }
    stop()
    const token = generation
    busy.value = true
    state.value = 'queued'
    try {
      await call('refresh subscriptions accounting')
      if (token === generation) await poll(token)
    } catch (e) {
      if (token !== generation) return
      error.value = e.message
      busy.value = false
    }
  }

  onScopeDispose(stop)
  return { busy, state, error, run, stop }
}
