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

import { createApp, h, nextTick, reactive, ref } from 'vue'
import { createPinia, setActivePinia } from 'pinia'
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import DirectorPage from '../../src/pages/DirectorPage.vue'
import SubscriptionReport from '../../src/components/SubscriptionReport.vue'
import { useDirectorStore } from '../../src/stores/director.js'

const { ensureScopeDirector, notify } = vi.hoisted(() => ({
  ensureScopeDirector: vi.fn(),
  notify: vi.fn(),
}))
const testRoute = reactive({ query: { tab: 'subscription' } })

vi.mock('vue-router', () => ({
  useRoute: () => testRoute,
  useRouter: () => ({ replace: vi.fn() }),
}))
vi.mock('vue-i18n', async importOriginal => ({
  ...await importOriginal(),
  useI18n: () => ({ t: value => value }),
}))
vi.mock('quasar', () => ({
  useQuasar: () => ({ notify, platform: { has: { touch: false } } }),
}))
vi.mock('../../src/composables/useDirectorScope.js', () => ({
  useDirectorScope: () => ({
    directorOptions: ref([{ label: 'prod-b', value: 'prod-b' }]),
    selectedDirectorsModel: ref(['prod-b']),
    activeDirectors: ref(['prod-b']),
    isCommonScope: ref(false),
    isSingleDirectorScope: ref(true),
    scopeLabel: ref('prod-b'),
    syncSelectedDirectors: vi.fn(),
    ensureScopeDirector,
    ensureSingleScopeDirector: vi.fn(),
  }),
}))

const containerStub = {
  setup: (_, { slots }) => () => h('div', slots.default?.()),
}
const buttonStub = {
  props: ['label', 'loading', 'disable', 'ariaLabel'],
  emits: ['click'],
  setup: (props, { emit }) => () => h('button', {
    disabled: props.loading || props.disable,
    'aria-label': props.ariaLabel,
    onClick: () => emit('click'),
  }, props.label),
}
const checkboxStub = {
  props: ['modelValue', 'label'],
  emits: ['update:modelValue'],
  setup: (props, { emit }) => () => h('label', [
    h('input', {
      type: 'checkbox',
      checked: props.modelValue,
      onChange: event => emit('update:modelValue', event.target.checked),
    }), props.label,
  ]),
}

describe('Director subscription calculation refresh', () => {
  let app
  let root
  let director

  beforeEach(async () => {
    vi.clearAllMocks()
    testRoute.query = { tab: 'subscription' }
    localStorage.clear()
    const pinia = createPinia()
    setActivePinia(pinia)
    director = useDirectorStore()
    vi.spyOn(director, 'call').mockImplementation(async command => {
      const accountingStatus = {
        generation: 1,
        worker_available: true,
        queued: false,
        running: false,
        refresh_thread_state: 'idle',
      }
      if (command === 'status subscriptions revision') {
        return { accounting_status: accountingStatus }
      }
      return {
        subscription_accounting: {
          calculated_at: '2026-11-06 10:00:00',
          age_seconds: 300,
        },
        accounting_snapshot: {
          ...accountingStatus,
          available: true,
        },
        accounting: [{ client: 'demo', files: 1, bytes: 2e9, rule_code: 'A' }],
      }
    })

    vi.spyOn(director, 'fetchAvailableDirectors').mockResolvedValue([])
    app = createApp(DirectorPage)
    app.use(pinia)
    app.config.warnHandler = () => {}
    for (const name of [
      'QPage', 'QTabs', 'QTab', 'QTabPanels', 'QTabPanel', 'QCard',
      'QCardSection', 'QSpace', 'QBanner', 'QTable', 'QDialog',
      'QInnerLoading', 'QSelect', 'QIcon', 'QTooltip', 'QSpinner',
    ]) {
      app.component(name, containerStub)
    }
    app.component('QBtn', buttonStub)
    app.component('QCheckbox', checkboxStub)
    root = document.createElement('div')
    document.body.append(root)
    app.mount(root)
    director.status = 'connected'
    await vi.waitFor(() => {
      expect(root.querySelector('button[aria-label="Refresh calculation"]')).not.toBeNull()
    })
    await nextTick()
  })

  afterEach(() => {
    app?.unmount()
    root?.remove()
  })

  function refreshButton() {
    const button = root.querySelector('button[aria-label="Refresh calculation"]')
    expect(button, 'Subscription page must offer a calculation refresh').not.toBeNull()
    return button
  }

  it('requests background recalculation from the selected director', async () => {
    refreshButton().click()
    await vi.waitFor(() => {
      expect(director.call).toHaveBeenCalledWith('status subscriptions recalculate')
    })
    expect(ensureScopeDirector).toHaveBeenCalledWith('prod-b')
    await vi.waitFor(() => {
      expect(notify).toHaveBeenCalledWith(expect.objectContaining({
        type: 'positive',
        message: 'Subscription calculation refresh requested in the background.',
      }))
    })
  })

  it('shows command errors instead of claiming the refresh succeeded', async () => {
    director.call.mockRejectedValueOnce(new Error('Access denied'))
    refreshButton().click()
    await vi.waitFor(() => {
      expect(root.textContent).toContain('Access denied')
    })
    expect(notify).not.toHaveBeenCalled()
    expect(refreshButton().disabled).toBe(false)
  })

  it('polls the lightweight status without reloading an unchanged report', async () => {
    const reportCallsBefore = director.call.mock.calls.filter(([command]) => (
      command === 'status subscriptions all'
    )).length
    await new Promise(resolve => setTimeout(resolve, 5200))
    expect(director.call).toHaveBeenCalledWith('status subscriptions revision')
    expect(director.call.mock.calls.filter(([command]) => (
      command === 'status subscriptions all'
    ))).toHaveLength(reportCallsBefore)
  }, 8000)

  it('reloads the full report when explicitly requested', async () => {
    const reportCallsBefore = director.call.mock.calls.filter(([command]) => (
      command === 'status subscriptions all'
    )).length
    root.querySelector('button[aria-label="Reload report"]').click()
    await vi.waitFor(() => {
      expect(director.call.mock.calls.filter(([command]) => (
        command === 'status subscriptions all'
      ))).toHaveLength(reportCallsBefore + 1)
    })
  })

  it('keeps refresh disabled while queued and polls until the worker is idle', async () => {
    let generation = 1
    let state = 'idle'
    director.call.mockImplementation(async command => {
      const accountingStatus = {
        generation,
        worker_available: true,
        queued: state === 'queued',
        running: state === 'running',
        refresh_thread_state: state,
      }
      if (command === 'status subscriptions recalculate') {
        state = 'queued'
        generation += 1
        return {}
      }
      if (command === 'status subscriptions revision') {
        return { accounting_status: accountingStatus }
      }
      return {
        subscription_accounting: { calculated_at: '2026-11-06 10:00:00', age_seconds: 1 },
        accounting_snapshot: { ...accountingStatus, available: true },
      }
    })
    refreshButton().click()
    await vi.waitFor(() => {
      expect(director.call).toHaveBeenCalledWith('status subscriptions recalculate')
      expect(refreshButton().disabled).toBe(true)
      expect(root.textContent).toContain('queued')
    })
    state = 'idle'
    generation += 1
    await vi.waitFor(() => expect(refreshButton().disabled).toBe(false), {
      timeout: 7000,
    })
  }, 10000)

  it('prevents duplicate requests while the command is in flight', async () => {
    let resolve
    director.call.mockImplementationOnce(() => new Promise(done => { resolve = done }))
    const button = refreshButton()
    button.click()
    await vi.waitFor(() => expect(button.disabled).toBe(true))
    button.click()
    expect(director.call).toHaveBeenCalledWith('status subscriptions recalculate')
    expect(director.call.mock.calls.filter(([command]) => (
      command === 'status subscriptions recalculate'
    ))).toHaveLength(1)
    resolve({})
    await vi.waitFor(() => expect(button.disabled).toBe(false))
  })

  it('keeps logical display selected across refresh but resets it on tab exit', async () => {
    const checkbox = root.querySelector('input[type="checkbox"]')
    expect(checkbox.closest('.sub-accounting-heading').textContent).toContain('Accounting Details')
    expect(checkbox.checked).toBe(false)
    checkbox.click()
    await nextTick()
    refreshButton().click()
    await vi.waitFor(() => expect(refreshButton().disabled).toBe(false))
    expect(checkbox.checked).toBe(true)
    testRoute.query.tab = 'status'
    await nextTick()
    expect(checkbox.checked).toBe(false)
    testRoute.query.tab = 'subscription'
    await nextTick()
    expect(checkbox.checked).toBe(false)
  })

  it.each([
    [false, false], [false, true], [true, false], [true, true],
  ])('passes the logical selection to PDF reports (anonymized=%s, logical=%s)', async (anonymized, logical) => {
    const report = {
      accounting: [{ client: 'demo', files: 1, bytes: 2e9, logical_bytes: 3e9, rule_code: 'A' }],
      summary: { total_files: 1, total_bytes: 2e9, total_logical_bytes: 3e9 },
    }
    director.call.mockResolvedValue(report)
    let printedText
    const print = vi.spyOn(window, 'print').mockImplementation(() => {
      printedText = document.querySelector('#subscription-print-root').textContent
    })
    try {
      if (logical) root.querySelector('input[type="checkbox"]').click()
      await nextTick()
      const label = anonymized ? 'Download Anonymized PDF' : 'Download PDF'
      const button = [...root.querySelectorAll('button')].find(button => button.textContent === label)
      button.click()
      await vi.waitFor(() => expect(print).toHaveBeenCalledOnce())
      expect(printedText.includes('Logical size (GB)')).toBe(logical)
      expect(printedText.includes('3.00')).toBe(logical)
      expect(director.call).toHaveBeenCalledWith(
        anonymized ? 'status subscriptions all anonymize' : 'status subscriptions all',
      )
    } finally {
      print.mockRestore()
    }
  })
})

describe('Subscription snapshot age and refresh eligibility', () => {
  let app
  let root

  beforeEach(() => {
    setActivePinia(createPinia())
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-11-06T10:05:00Z'))
    root = document.createElement('div')
    document.body.append(root)
  })

  afterEach(() => {
    app?.unmount()
    root.remove()
    vi.useRealTimers()
  })

  function mountReport(age, options = {}) {
    const refresh = vi.fn()
    app = createApp(SubscriptionReport, {
      data: { subscription_accounting: {
        calculated_at: '2026-11-06 10:00:01',
        age_seconds: age,
      }, accounting_snapshot: { refresh_thread_state: 'idle' } },
      refreshable: true,
      onRefresh: refresh,
      ...options,
    })
    app.component('QBtn', buttonStub)
    app.component('QSpinner', containerStub)
    app.component('QIcon', containerStub)
    app.component('QCheckbox', checkboxStub)
    app.mount(root)
    return refresh
  }

  it('allows immediate refresh when the worker is idle regardless of snapshot age', async () => {
    const refresh = mountReport(299)
    const button = root.querySelector('button')
    expect(root.querySelector('.sub-status-details').textContent).toContain('4 minutes ago')
    expect(root.querySelector('.sub-context-date').textContent).toBe('2026-11-06 10:00:01')
    expect(button.parentElement.textContent).toContain('Accounting snapshot')
    expect(button.textContent).toBe('')
    expect(button.disabled).toBe(false)
    button.click()
    expect(refresh).toHaveBeenCalledOnce()
    await vi.advanceTimersByTimeAsync(1000)
    await nextTick()
    expect(root.querySelector('.sub-status-details').textContent).toContain('5 minutes ago')
    expect(button.disabled).toBe(false)
    button.click()
    expect(refresh).toHaveBeenCalledTimes(2)
  })

  it('allows refresh with unknown snapshot age when the worker is idle', () => {
    const refresh = mountReport(undefined)
    expect(root.textContent).toContain('Unknown age')
    expect(root.querySelector('button').disabled).toBe(false)
    root.querySelector('button').click()
    expect(refresh).toHaveBeenCalledOnce()
  })

  it.each(['running', 'queued', 'unavailable', undefined])(
    'does not allow refresh when worker state is %s', state => {
      const refresh = mountReport(600, {
        data: {
          subscription_accounting: { calculated_at: '2026-11-06 10:00:01', age_seconds: 600 },
          accounting_snapshot: { refresh_thread_state: state },
        },
      })
      expect(root.querySelector('button').disabled).toBe(true)
      root.querySelector('button').click()
      expect(refresh).not.toHaveBeenCalled()
    },
  )

  it('shows a busy indicator until the worker becomes idle', async () => {
    const data = reactive({
      subscription_accounting: { calculated_at: '2026-11-06 10:00:01' },
      accounting_snapshot: { refresh_thread_state: 'queued' },
    })
    mountReport(600, { data })
    expect(root.querySelector('[role="status"]')).not.toBeNull()
    data.accounting_snapshot.refresh_thread_state = 'running'
    await nextTick()
    expect(root.querySelector('[role="status"]')).not.toBeNull()
    data.accounting_snapshot.refresh_thread_state = 'idle'
    await nextTick()
    expect(root.querySelector('[role="status"]')).toBeNull()
    expect(root.querySelector('button').disabled).toBe(false)
  })

  it('omits the action from downloaded and printed reports', () => {
    mountReport(600, { refreshable: false })
    expect(root.querySelector('button')).toBeNull()
    expect(root.querySelector('.sub-status-details').textContent).toContain('10 minutes ago')
    expect(root.querySelector('.sub-context-date').textContent).toBe('2026-11-06 10:00:01')
  })

  it('uses the console rule codes and explains them below the accounting table', () => {
    mountReport(600, {
      data: {
        accounting_rule_legend: 'A = allocated size; L = logical size; J = job size of the latest Full',
        accounting: ['A', 'L', 'J', 'future_rule'].map(rule_code => ({
          client: 'demo', fileset: 'Files', files: 1, bytes: 1e9,
          logical_bytes: 1e9, rule_code, jobs_in_chain: 1,
        })),
      },
    })
    const table = [...root.querySelectorAll('table')]
      .find(element => element.textContent.includes('Chain jobs'))
    const ruleCells = [...table.querySelectorAll('tbody tr')]
      .map(row => row.children[4].textContent)
    expect(ruleCells).toEqual(['A', 'L', 'J', 'future_rule'])
    expect(root.textContent).toContain('A = allocated size')
    expect(root.textContent).toContain('L = logical size')
    expect(root.textContent).toContain('J = job size of the latest Full')
  })

  it('renders accounting values, exclusions, duration and the renamed breakdown', () => {
    mountReport(600, {
      data: {
        subscription_accounting: { calculated_at: '2026-11-06 10:00:01' },
        'unit-detail': [{ client: 'demo', plugin: 'bpipe', count: 1, size_gb: 2 }],
        accounting_snapshot: {
          last_full_duration_ms: 1250,
          last_full_completed_at: '2026-11-06 10:00:01',
          refresh_thread_state: 'idle',
        },
        accounting: [
          { client: 'demo', fileset: 'Files', files: 10, bytes: 2e9,
            logical_bytes: 3e9, rule: 'st_blocks*512', jobs_in_chain: 2 },
          { client: 'ndmp', fileset: 'Archive', excluded: true,
            accounting_issue: 'Size unavailable: NDMP has neither file history nor a usable Full-job byte count.' },
          { client: 'opaque', fileset: 'Image', excluded: true,
            accounting_issue: 'Size unavailable: per-file sizes and a usable baseline Full-job byte count are both missing.' },
        ],
        summary: { total_files: 10, total_bytes: 2e9,
          total_logical_bytes: 3e9, incomplete: true },
      },
    })
    expect(root.textContent).toContain('Client / Plugin Breakdown')
    expect(root.textContent).toContain('Accounting Details')
    const durationLabel = [...root.querySelectorAll('dt')]
      .find(element => element.textContent === 'Last successful full recalculation')
    expect(durationLabel.nextElementSibling.textContent).toContain('1.25 seconds')
    expect(root.textContent).toContain('Size unavailable: NDMP has neither file history nor a usable Full-job byte count.')
    expect(root.textContent).not.toContain('ndmp_no_file_history')
    expect(root.textContent).toContain('Size unavailable: per-file sizes and a usable baseline Full-job byte count are both missing.')
    expect(root.textContent).not.toContain('opaque_backup_image')
    const table = [...root.querySelectorAll('table')]
      .find(element => element.textContent.includes('Chain jobs'))
    expect(table.textContent).toContain('2.00')
    expect(table.textContent).not.toContain('3.00')
    expect(table.textContent).not.toContain('Logical size')
  })

  it('explains job-size estimates and missing backup chains', () => {
    mountReport(600, {
      data: {
        accounting: [
          { client: 'image', estimated: true, rule_code: 'J',
            rule_explanation: 'Estimated from the baseline Full job' },
          { client: 'missing', excluded: true,
            accounting_issue: 'Excluded: no usable backup chain starting with a Full backup could be found; required catalog records may have been purged.' },
        ],
      },
    })
    const table = [...root.querySelectorAll('table')]
      .find(element => element.textContent.includes('Chain jobs'))
    expect(table.querySelector('tbody tr').children).toHaveLength(6)
    expect(table.querySelector('tbody tr').children[4].title).toContain('Estimated from the baseline Full job')
    expect(root.textContent).not.toContain('Accounting issues')
    expect(root.textContent).toContain('Excluded: no usable backup chain starting with a Full backup could be found; required catalog records may have been purged.')
    expect(root.textContent).not.toContain('no_usable_chain')
  })

  it('links the baseline Full for missing sizes and warns about incomplete totals', () => {
    const openJob = vi.fn()
    mountReport(600, {
      data: { summary: { warning: 'Accounting incomplete' }, accounting: [{
        client: 'image', excluded: true, exclusion_reason: 'opaque_backup_image',
        accounting_issue: 'Size unavailable: per-file sizes and a usable baseline Full-job (id 42) byte count are both missing.',
        accounting_issue_before_jobid: 'Size unavailable: per-file sizes and a usable baseline Full-job (id ',
        accounting_issue_after_jobid: ') byte count are both missing.',
        baseline_full_jobid: 42,
      }] },
      onOpenJob: openJob,
    })
    expect(root.querySelector('[role="alert"]').textContent).toContain('Accounting incomplete')
    const table = [...root.querySelectorAll('table')]
      .find(element => element.textContent.includes('Chain jobs'))
    expect(table.querySelectorAll('thead th')).toHaveLength(6)
    const issueRow = table.querySelector('.sub-accounting-issue')
    expect(issueRow.parentElement.classList.contains('sub-accounting-group--issue')).toBe(true)
    expect(issueRow.parentElement.children).toHaveLength(2)
    expect(issueRow.previousElementSibling.children[0].textContent).toBe('image')
    expect(issueRow.children[0].colSpan).toBe(6)
    const link = root.querySelector('a')
    expect(link.textContent).toBe('42')
    expect(issueRow.textContent.trim()).toBe('Size unavailable: per-file sizes and a usable baseline Full-job (id 42) byte count are both missing.')
    link.click()
    expect(openJob).toHaveBeenCalledWith(42)
  })

  it('keeps printed issues grouped with their accounting row and baseline ID', () => {
    mountReport(600, {
      refreshable: false,
      data: { accounting: [
        { client: 'measured', rule_code: 'A' },
        { client: 'missing', fileset: 'Archive', excluded: true,
          accounting_issue: 'Size unavailable: baseline Full-job (id 42) byte count missing.', baseline_full_jobid: 42 },
      ] },
    })
    const group = root.querySelector('.sub-accounting-group--issue')
    expect(root.querySelectorAll('.sub-accounting-group--issue')).toHaveLength(1)
    expect(group.children[0].textContent).toContain('missingArchive')
    expect(group.children[1].textContent).toContain('Size unavailable')
    expect(group.children[1].textContent).toContain('baseline Full-job (id 42)')
    expect(group.querySelector('a')).toBeNull()
  })

  it('shows logical values and totals only when requested, including in printed reports', () => {
    mountReport(600, {
      refreshable: false,
      showLogicalSize: true,
      data: {
        accounting: [
          { client: 'demo', files: 1, bytes: 2e9, logical_bytes: 3e9, rule_code: 'A' },
          { client: 'missing', excluded: true, accounting_issue: 'Size unavailable' },
        ],
        summary: { total_files: 1, total_bytes: 2e9, total_logical_bytes: 3e9 },
      },
    })
    const table = [...root.querySelectorAll('table')]
      .find(element => element.textContent.includes('Chain jobs'))
    expect(table.querySelectorAll('thead th')).toHaveLength(7)
    expect(table.textContent).toContain('Logical size (GB)')
    expect(table.querySelector('tbody tr').children[4].textContent).toBe('3.00')
    expect(table.querySelector('.sub-row--total').children[4].textContent).toBe('3.00')
    expect(table.querySelector('.sub-accounting-issue td').colSpan).toBe(7)
  })
})
