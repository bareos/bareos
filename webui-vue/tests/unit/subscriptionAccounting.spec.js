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
import { afterEach, describe, expect, it, vi } from 'vitest'
import { createApp, effectScope, h, nextTick } from 'vue'
import { useSubscriptionAccounting } from '../../src/composables/useSubscriptionAccounting.js'
import SubscriptionAccountingAge from '../../src/components/SubscriptionAccountingAge.vue'
import SubscriptionReport from '../../src/components/SubscriptionReport.vue'

vi.mock('../../src/stores/settings.js', () => ({
  useSettingsStore: () => ({ locale: 'en-EN' }),
}))

vi.mock('vue-i18n', () => ({
  useI18n: () => ({ t: (key, values) => values
    ? key.replace(/\{(\w+)\}/g, (_, name) => values[name]) : key }),
}))

const scopes = []
function setup(call, canRun = () => true) {
  const scope = effectScope()
  scopes.push(scope)
  const onComplete = vi.fn()
  const accounting = scope.run(() => useSubscriptionAccounting({ call, onComplete, canRun, t: value => value }))
  return { ...accounting, onComplete, scope }
}
afterEach(() => {
  scopes.splice(0).forEach(scope => scope.stop())
  vi.useRealTimers()
})
const worker = (state, extra = {}) => ({
  accounting_snapshot: { available: true, refresh_thread_state: state, ...extra },
})
describe('accounting refresh', () => {
  it('does not issue commands without permission or capability', async () => {
    const call = vi.fn()
    const accounting = setup(call, () => false)
    await accounting.run()
    expect(call).not.toHaveBeenCalled()
    expect(accounting.error.value).toBe('Accounting refresh is unavailable or not permitted.')
  })
  it('queues, polls without overlap and reloads once idle', async () => {
    vi.useFakeTimers()
    const call = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce(worker('queued'))
      .mockResolvedValueOnce(worker('running'))
      .mockResolvedValue(worker('idle'))
    const accounting = setup(call)
    await accounting.run()
    expect(call.mock.calls.map(args => args[0])).toEqual([
      'refresh subscriptions accounting', 'status subscriptions accounting',
    ])
    await accounting.run()
    expect(call).toHaveBeenCalledTimes(2)
    await vi.advanceTimersByTimeAsync(3000)
    expect(accounting.state.value).toBe('running')
    await vi.advanceTimersByTimeAsync(3000)
    expect(accounting.onComplete).toHaveBeenCalledTimes(1)
    expect(accounting.busy.value).toBe(false)
  })
  it('reports refresh failure instead of announcing success', async () => {
    const accounting = setup(vi.fn().mockResolvedValueOnce({})
      .mockResolvedValue(worker('idle', { last_refresh_error: 'database error' })))
    await accounting.run()
    expect(accounting.error.value).toBe('database error')
    expect(accounting.busy.value).toBe(false)
  })
  it('allows the first snapshot to be calculated', async () => {
    vi.useFakeTimers()
    const accounting = setup(vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce(worker('running', { available: false }))
      .mockResolvedValue(worker('idle')))
    await accounting.run()
    expect(accounting.busy.value).toBe(true)
    expect(accounting.error.value).toBeNull()
    await vi.advanceTimersByTimeAsync(3000)
    expect(accounting.onComplete).toHaveBeenCalledTimes(1)
    expect(accounting.busy.value).toBe(false)
  })
  it('surfaces failure to reload the completed report', async () => {
    const accounting = setup(vi.fn().mockResolvedValueOnce({})
      .mockResolvedValue(worker('idle')))
    accounting.onComplete.mockRejectedValue(new Error('report reload failed'))
    await accounting.run()
    expect(accounting.error.value).toBe('report reload failed')
    expect(accounting.busy.value).toBe(false)
  })
  it('discards a previous error sampled just before a successful retry finished', async () => {
    const call = vi.fn().mockResolvedValueOnce({})
      .mockResolvedValueOnce(worker('idle', { last_refresh_error: 'old error' }))
      .mockResolvedValueOnce(worker('idle'))
    const accounting = setup(call)
    await accounting.run()
    expect(accounting.error.value).toBeNull()
    expect(accounting.onComplete).toHaveBeenCalledTimes(1)
    expect(call).toHaveBeenCalledTimes(3)
  })
  it('does not poll when the Director changes during the refresh command', async () => {
    let resolve
    const call = vi.fn().mockImplementationOnce(() => new Promise(r => { resolve = r }))
    const accounting = setup(call)
    const running = accounting.run()
    accounting.stop()
    resolve({})
    await running
    expect(call).toHaveBeenCalledTimes(1)
    expect(accounting.onComplete).not.toHaveBeenCalled()
  })
  it.each([
    [worker('unavailable'), 'Accounting background worker is unavailable.'],
    [{}, 'Accounting worker status is unavailable.'],
    [worker('idle', { available: false }), 'No accounting snapshot is available.'],
  ])('surfaces unavailable status %j', async (response, message) => {
    const accounting = setup(vi.fn().mockResolvedValueOnce({}).mockResolvedValue(response))
    await accounting.run()
    expect(accounting.error.value).toBe(message)
  })
  it('surfaces command permission and polling errors', async () => {
    const accounting = setup(vi.fn().mockRejectedValue(new Error('permission denied')))
    await accounting.run()
    expect(accounting.error.value).toBe('permission denied')
    expect(accounting.onComplete).not.toHaveBeenCalled()
    const polling = setup(vi.fn().mockResolvedValueOnce({})
      .mockRejectedValueOnce(new Error('disconnected')))
    await polling.run()
    expect(polling.error.value).toBe('disconnected')
  })

  describe('snapshot age display', () => {
    it('shows expected estimates as information while retaining failure warnings', () => {
      const host = document.createElement('div')
      const app = createApp(SubscriptionReport, {
        data: {
          subscription_accounting: {
            source: 'mixed', calculated_at: '2026-10-05 08:00:00',
            estimated_combinations: 1, combinations: 6, refresh_failed: true,
          },
        },
      })
      app.mount(host)
      try {
        const estimate = [...host.querySelectorAll('div')].find(
          element => element.textContent.trim() === 'Estimated sizes for 1 of 6 Client/FileSet combinations',
        )
        expect(estimate).toBeDefined()
        expect(estimate.classList.contains('text-warning')).toBe(false)
        expect([...host.querySelectorAll('.text-warning')].some(
          element => element.textContent.includes('Latest accounting refresh failed.'),
        )).toBe(true)
      } finally {
        app.unmount()
      }
    })
    it('keeps the age in reports but renders the action only when supplied', () => {
      const data = {
        subscription_accounting: { calculated_at: '2026-10-04 08:00:00', age_seconds: 3600 },
        'report-time': '2026-10-04 09:00:00',
      }
      for (const interactive of [false, true]) {
        const host = document.createElement('div')
        const app = createApp({
          render: () => h(SubscriptionReport, { data },
            interactive ? { 'accounting-action': () => h('button', 'Start accounting now') } : {}),
        })
        app.mount(host)
        try {
          expect(host.textContent).toContain('Accounting snapshot age: 1h 0m')
          expect(!!host.querySelector('button')).toBe(interactive)
        } finally {
          app.unmount()
        }
      }
    })
    it('updates its age and turns red only after 24 hours', async () => {
      vi.useFakeTimers()
      const host = document.createElement('div')
      const app = createApp(SubscriptionAccountingAge, {
        snapshot: { calculated_at: '2026-10-04 08:00:00', age_seconds: 86400 },
        reportTime: '2026-10-05 08:00:00',
      })
      app.mount(host)
      try {
        expect(host.textContent).toContain('24h 0m')
        expect(host.querySelector('.text-negative')).toBeNull()
        await vi.advanceTimersByTimeAsync(60000)
        await nextTick()
        expect(host.textContent).toContain('24h 1m')
        expect(host.querySelector('.text-negative')).not.toBeNull()
        expect(host.querySelector('span').title).toBe('2026-10-04 08:00:00')
        expect(host.querySelector('button')).toBeNull()
      } finally {
        app.unmount()
      }
      expect(vi.getTimerCount()).toBe(0)
    })
    it('shows unavailable without a snapshot', () => {
      const host = document.createElement('div')
      const app = createApp(SubscriptionAccountingAge)
      app.mount(host)
      expect(host.textContent).toContain('Unavailable')
      app.unmount()
    })
  })
  it('ignores late responses after switching Director or disposing', async () => {
    let resolve
    const call = vi.fn().mockResolvedValueOnce({}).mockImplementationOnce(() => new Promise(r => { resolve = r }))
    const accounting = setup(call)
    const running = accounting.run()
    await Promise.resolve()
    accounting.scope.stop()
    resolve(worker('idle'))
    await running
    expect(accounting.onComplete).not.toHaveBeenCalled()
    expect(accounting.busy.value).toBe(false)
  })
  it('clears scheduled polls on stop', async () => {
    vi.useFakeTimers()
    const call = vi.fn().mockResolvedValueOnce({}).mockResolvedValue(worker('running'))
    const accounting = setup(call)
    await accounting.run()
    accounting.stop()
    await vi.advanceTimersByTimeAsync(6000)
    expect(call).toHaveBeenCalledTimes(2)
  })
})
