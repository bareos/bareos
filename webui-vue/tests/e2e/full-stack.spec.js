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

import { test, expect } from '@playwright/test'
import { readFile, rm, mkdir } from 'node:fs/promises'
import { basename, dirname, relative, sep, join } from 'node:path'

const profile = process.env.BAREOS_WEBUI_PROFILE ?? 'admin'
const username = process.env.BAREOS_WEBUI_USERNAME ?? 'admin'
const password = process.env.BAREOS_WEBUI_PASSWORD ?? 'admin'
const expectedDirectorTransport
  = process.env.BAREOS_WEBUI_EXPECTED_DIRECTOR_TRANSPORT
const isReadonlyProfile = profile === 'readonly'
const isMultiDirectorProfile
  = process.env.BAREOS_WEBUI_PROXY_ENABLE_MULTI_DIRECTOR === '1'
const LOGIN_ATTEMPTS = 3
const LOGIN_RESULT_TIMEOUT_MS = 10_000
const directorResponseCompletions = new WeakMap()

// Exercise the public application with an isolated, entirely intercepted
// transport. In particular, no mutation can reach a real Director.
async function browserFixture(page, commandHandler = () => undefined) {
  const directors = ['bareos-dir', 'bareos-dir-2']
  const responseCompletions = new Map()
  directorResponseCompletions.set(page, responseCompletions)
  let responseSequence = 0
  await page.route('**/api/**', route => route.fulfill({
    json: route.request().url().endsWith('/directors')
      ? { type: 'director_list', directors }
      : { authenticatedDirectors: directors.map(director => ({ director, username: 'admin' })) },
  }))
  await page.routeWebSocket(/\/ws/, socket => {
    let director = directors[0]
    socket.onMessage(async message => {
      const request = JSON.parse(message)
      if (request.type === 'session') {
        director = request.director
        socket.send(JSON.stringify({ type: 'auth_ok' }))
        return
      }
      if (request.type !== 'command') return
      const responseToken = String(++responseSequence)
      let resolveSent
      const sent = new Promise(resolve => { resolveSent = resolve })
      responseCompletions.set(responseToken, { sent })
      try {
        const data = await commandHandler(request.command, director, { responseToken, socket })
        const job = {
          jobid: '101', name: 'Browser-fixture', jobtype: 'B', jobstatus: 'T',
          comment: 'Original comment',
        }
        socket.send(JSON.stringify({
          type: 'response', id: request.id, fixtureResponseToken: responseToken,
          data: data ?? (
            request.command.startsWith('llist jobs count') ? { jobs: [{ count: '1' }] }
              : request.command.startsWith('llist job') ? { jobs: [job] }
                : request.command.startsWith('list joblog') ? { joblog: [{ logtext: 'Termination: Backup OK' }] }
                  : {}
          ),
        }))
        resolveSent()
      } catch (error) {
        socket.send(JSON.stringify({
          type: 'error', id: request.id, fixtureResponseToken: responseToken, message: error.message,
        }))
        resolveSent()
      }
    })
  })
  await page.addInitScript(() => {
    window.__fixtureMessages = []
    const NativeWebSocket = window.WebSocket
    window.WebSocket = class extends NativeWebSocket {
      constructor(...args) {
        super(...args)
        this.addEventListener('message', event => {
          queueMicrotask(() => window.__fixtureMessages.push(event.data))
        })
      }
    }
  })
  await page.goto('/#/dashboard')
  await expect(page.getByTestId('director-status-label')).toContainText('Connected')
}

async function waitForDirectorResponseProcessing(page, responseToken) {
  const completion = directorResponseCompletions.get(page)?.get(responseToken)
  if (!completion) throw new Error(`Unknown Director response token: ${responseToken}`)
  await completion.sent
  await page.waitForFunction(token => window.__fixtureMessages.some(message =>
    JSON.parse(message).fixtureResponseToken === token
  ), responseToken)
}

test('browser regression: the 24-hour Jobs filter applies to counts and lists', async ({ page }) => {
  const commands = []
  await browserFixture(page, command => {
    commands.push(command)
    if (command.startsWith('llist jobs count')) return { jobs: [{ count: '1' }] }
    if (command.startsWith('llist jobs ')) {
      return { jobs: [{ jobid: '1', name: 'Example', jobstatus: 'T' }] }
    }
  })
  commands.length = 0
  await page.goto('/#/jobs?days=1&status=T')
  await expect(page.getByText('Last 24 hours', { exact: true })).toBeVisible()
  await expect.poll(() => commands.some(command =>
    command.startsWith('llist jobs count') && command.includes('days=1')
  )).toBe(true)
  await expect.poll(() => commands.some(command =>
    command.startsWith('llist jobs ') && !command.startsWith('llist jobs count') && command.includes('days=1')
  )).toBe(true)
  commands.length = 0
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await expect.poll(() => commands.filter(command =>
    command.startsWith('llist jobs count') && command.includes('days=1')
  ).length).toBeGreaterThanOrEqual(2)
  await expect.poll(() => commands.filter(command =>
    command.startsWith('llist jobs ') && !command.startsWith('llist jobs count') && command.includes('days=1')
  ).length).toBeGreaterThanOrEqual(2)
  commands.length = 0
  await page.getByText('Last 24 hours', { exact: true }).locator('..').getByRole('button').click()
  await expect(page.getByText('Last 24 hours', { exact: true })).toHaveCount(0)
  await expect.poll(() => commands.some(command =>
    command.startsWith('llist jobs count') && !command.includes('days=1')
  )).toBe(true)
})

test('browser regression: job details return to the 24-hour Jobs filter', async ({ page }) => {
  const commands = []
  await browserFixture(page, command => {
    commands.push(command)
    if (command.startsWith('llist jobs count')) return { jobs: [{ count: '1' }] }
    if (command.startsWith('llist jobs ')) {
      return { jobs: [{ jobid: '1', name: 'Example', jobstatus: 'T' }] }
    }
  })
  await page.goto('/#/jobs?days=1&status=T')
  await page.locator('tbody tr a.text-primary').first().click()
  await expect(page).toHaveURL(/jobsDays=1/)
  commands.length = 0
  await page.getByRole('navigation', { name: 'Breadcrumb' }).getByRole('link', { name: 'Jobs' }).click()
  await expect(page.getByText('Last 24 hours', { exact: true })).toBeVisible()
  await expect.poll(() => commands.some(command =>
    command.startsWith('llist jobs count') && command.includes('days=1')
  )).toBe(true)

  await page.getByText('Last 24 hours', { exact: true }).locator('..').getByRole('button').click()
  await page.locator('tbody tr a.text-primary').first().click()
  await expect(page).not.toHaveURL(/jobsDays=/)
  await page.getByRole('navigation', { name: 'Breadcrumb' }).getByRole('link', { name: 'Jobs' }).click()
  await expect(page.getByText('Last 24 hours', { exact: true })).toHaveCount(0)
})

test('browser regression: the last dashboard can be renamed but not deleted', async ({ page }) => {
  await page.addInitScript(() => {
    localStorage.setItem('bareos_dashboards', JSON.stringify({
      dashboards: [{ id: 'only', name: 'Only', widgets: [] }],
      activeDashboardId: 'only',
      seededDashboardIds: ['default', 'analytics', 'job-schedule-timeline'],
    }))
  })
  await browserFixture(page)
  await page.getByTitle('Edit layout').click()
  await expect(page.getByRole('button', { name: 'Rename dashboard' })).toBeVisible()
  await expect(page.getByRole('button', { name: 'Delete dashboard' })).toHaveCount(0)
})

test('browser regression: dashboard widgets drag only from their headers', async ({ page }) => {
  await browserFixture(page)
  await page.getByTitle('Edit layout').click()
  const item = page.locator('.vgl-item').first()
  await expect(item.locator('.widget-drag-handle')).toBeVisible()
  const before = await item.boundingBox()
  if (!before) throw new Error('Could not get widget bounds')
  await page.mouse.move(before.x + before.width / 2, before.y + before.height / 2)
  await page.mouse.down()
  await page.mouse.move(before.x + before.width / 2 + 120, before.y + before.height / 2 + 90, { steps: 10 })
  await page.mouse.up()
  const after = await item.boundingBox()
  expect(after?.x).toBe(before.x)
  expect(after?.y).toBe(before.y)
})

test('browser regression: column checkboxes have accessible names', async ({ page }) => {
  await browserFixture(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByRole('button', { name: 'Choose columns', exact: true }).click()
  await expect(page.getByRole('checkbox', { name: 'Comment', exact: true })).toBeVisible()
  for (const checkbox of await page.getByRole('checkbox').all()) {
    await expect(checkbox).toHaveAccessibleName(/\S/)
  }
})

test('browser regression: passive job status retains its accessible name', async ({ page }) => {
  await browserFixture(page)
  await page.goto('/#/jobs/101?director=bareos-dir')
  await expect(page.getByText('Job #101 — Browser-fixture')).toBeVisible()
  await expect(page.getByRole('status', { name: 'OK', exact: true })).toBeVisible()
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await expect(page.getByRole('button', { name: 'OK: Jump to log', exact: true }).first()).toBeVisible()
})

test('browser regression: capped multi-Director results show the fetched total', async ({ page }) => {
  await browserFixture(page, command => {
    if (command.startsWith('llist jobs count')) return { jobs: [{ count: '1100' }] }
    if (command.startsWith('llist jobs ')) {
      const limit = Number(command.match(/\blimit=(\d+)/)?.[1] ?? 1000)
      return { jobs: Array.from({ length: Math.min(limit, 1000) }, (_, index) => ({
        jobid: String(index + 1), name: `Job-${index + 1}`, jobstatus: 'T',
      })) }
    }
  })
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByRole('columnheader', { name: /^Duration\b/ }).click()
  await expect(page.getByText('Fetched 2,000 jobs. Results may be incomplete. Narrow your filters to see all matching jobs.')).toBeVisible()
})

test('browser regression: dependent readiness failure does not repeat selection', async ({ page }) => {
  await page.setContent(`
    <div class="q-field">
      <span data-testid="source"></span>
      <button role="combobox" aria-controls="options">Choose</button>
    </div>
    <div id="options" role="listbox"><button role="option">Option</button></div>
  `)
  await page.evaluate(() => {
    window.optionClicks = 0
    document.querySelector('[role="option"]').addEventListener('click', () => { window.optionClicks++ })
  })
  await expect(selectFirstQOption(page, 'source', {
    selected: () => Promise.reject(new Error('dependent field failed')),
  })).rejects.toThrow('dependent field failed')
  expect(await page.evaluate(() => window.optionClicks)).toBe(1)
})

test('browser regression: comment editor is reachable and operable by keyboard', async ({ page }) => {
  await browserFixture(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await checkCommentEditorKeyboard(page)
})

test('browser regression: schedule and job toggles refresh upcoming runs', async ({ page }) => {
  let scheduleEnabled = true
  let jobEnabled = true
  let upcomingPreviewCalls = 0

  await browserFixture(page, command => {
    if (command === 'show schedules') {
      return { schedules: { daily: { name: 'DailyCycle', enabled: scheduleEnabled } } }
    }
    if (command === 'show schedules all') {
      return { jobs: { daily: { name: 'DailyJob', schedule: 'DailyCycle', enabled: jobEnabled } } }
    }
    if (command === '.schedule') {
      return { schedules: [{ name: 'DailyCycle', enabled: scheduleEnabled }] }
    }
    if (command.startsWith('status scheduler days=')) {
      if (command === 'status scheduler days=0,60') upcomingPreviewCalls += 1
      return {
        schedules: [{
          name: 'DailyCycle',
          enabled: scheduleEnabled,
          jobs: [{ name: 'DailyJob', enabled: jobEnabled }],
        }],
        preview: scheduleEnabled && jobEnabled ? [{
          job: 'DailyJob',
          schedule: 'DailyCycle',
          datetime: '2026-05-28 00:00:00',
          runtime: Date.now() / 1000 + 3600,
        }] : [],
      }
    }
    if (command === 'disable schedule="DailyCycle"') scheduleEnabled = false
    if (command === 'enable schedule="DailyCycle"') scheduleEnabled = true
    if (command === 'disable job="DailyJob"') jobEnabled = false
    if (command === 'enable job="DailyJob"') jobEnabled = true
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect(page.getByText('Scheduler Jobs', { exact: true })).toBeVisible()
  await expect.poll(() => upcomingPreviewCalls).toBeGreaterThan(0)
  const nextRun = page.locator('.schedules-list-stats').getByText(/Next run:/)
  await expect(nextRun).toBeVisible()

  const scheduleCalls = upcomingPreviewCalls
  const scheduleGroup = page.locator('.sched-group-header').first()
  await scheduleGroup.getByRole('button', { name: 'Expand group', exact: true }).click()
  await scheduleGroup.locator('.q-toggle').click()
  await expect.poll(() => upcomingPreviewCalls).toBeGreaterThan(scheduleCalls)
  await expect(nextRun).toHaveCount(0)

  await scheduleGroup.locator('.q-toggle').click()
  await expect(nextRun).toBeVisible()

  const jobCalls = upcomingPreviewCalls
  const jobRow = page.locator('tbody tr').filter({ hasText: 'DailyJob' })
  await jobRow.locator('.q-toggle').click()
  await expect.poll(() => upcomingPreviewCalls).toBeGreaterThan(jobCalls)
  await expect(nextRun).toHaveCount(0)
})

test('browser regression: schedule job sorting keeps groups intact in both directions', async ({ page }) => {
  await browserFixture(page, command => {
    if (!command.startsWith('status scheduler days=')) return
    return {
      schedules: [
        { name: 'GroupOne', jobs: [{ name: 'ZuluJob' }, { name: 'BravoJob' }] },
        { name: 'GroupTwo', jobs: [{ name: 'AlphaJob' }, { name: 'CharlieJob' }] },
      ],
      preview: [],
    }
  })

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  const table = page.locator('.bareos-panel').filter({ hasText: 'Scheduler Jobs' }).locator('table')
  await expect(table.locator('.sched-group-header')).toHaveCount(2)
  for (const group of await table.locator('.sched-group-header').all()) {
    await group.getByRole('button', { name: 'Expand group', exact: true }).click()
  }
  const rows = table.locator('tbody tr')
  await expect(rows).toHaveCount(6)
  const names = async () => rows.evaluateAll(elements => elements.map(row =>
    row.querySelector('.text-weight-bold, a')?.textContent.trim()
  ))
  expect(await names()).toEqual(['GroupOne', 'ZuluJob', 'BravoJob', 'GroupTwo', 'AlphaJob', 'CharlieJob'])
  const jobHeader = table.getByRole('columnheader', { name: /Job/ })
  await jobHeader.click()
  await expect.poll(names).toEqual(['GroupTwo', 'AlphaJob', 'CharlieJob', 'GroupOne', 'BravoJob', 'ZuluJob'])
  await jobHeader.click()
  await expect.poll(names).toEqual(['GroupOne', 'ZuluJob', 'BravoJob', 'GroupTwo', 'CharlieJob', 'AlphaJob'])
})

test('browser regression: schedule continuation pages retain their group header', async ({ page }) => {
  const jobs = Array.from({ length: 25 }, (_, index) => ({
    name: `LargeJob${String(index + 1).padStart(2, '0')}`,
  }))
  await browserFixture(page, command => {
    if (!command.startsWith('status scheduler days=')) return
    return {
      schedules: [{ name: 'LargeGroup', jobs }, { name: 'SmallGroup', jobs: [{ name: 'SmallJob' }] }],
      preview: [],
    }
  })

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  const panel = page.locator('.bareos-panel').filter({ hasText: 'Scheduler Jobs' })
  const table = panel.locator('table')
  await expect(table.locator('.sched-group-header')).toHaveCount(2)
  await panel.getByRole('combobox', { name: 'Records per page:' }).click()
  await page.getByRole('option', { name: '10', exact: true }).click()
  await table.locator('.sched-group-header').filter({ hasText: 'LargeGroup' })
    .getByRole('button', { name: 'Expand group', exact: true }).click()
  const collectJobs = () => table.locator('tbody a').allTextContents()
  const allJobs = []
  for (let pageNumber = 0; pageNumber < 3; pageNumber++) {
    await expect(table.locator('tbody tr').first()).toContainText('LargeGroup')
    allJobs.push(...await collectJobs())
    if (pageNumber < 2) await panel.getByRole('button', { name: 'Next page' }).click()
  }
  expect(allJobs).toEqual(jobs.map(job => job.name))
  await expect(table.locator('.sched-group-header')).toContainText(['LargeGroup', 'SmallGroup'])
  const jobHeader = table.getByRole('columnheader', { name: /Job/ })
  await jobHeader.click()
  await jobHeader.click()
  const sortedJobs = []
  for (let pageNumber = 0; pageNumber < 3; pageNumber++) {
    await expect(table.locator('.sched-group-header').filter({ hasText: 'LargeGroup' })).toHaveCount(1)
    if (pageNumber > 0) await expect(table.locator('tbody tr').first()).toContainText('LargeGroup')
    sortedJobs.push(...await collectJobs())
    if (pageNumber < 2) await panel.getByRole('button', { name: 'Next page' }).click()
  }
  expect(sortedJobs).toEqual(jobs.map(job => job.name).reverse())
  await table.locator('.sched-group-header').filter({ hasText: 'LargeGroup' })
    .getByRole('button', { name: 'Collapse group', exact: true }).click()
  await expect(table.locator('tbody a')).toHaveCount(0)
  await expect(table.locator('.sched-group-header')).toContainText(['SmallGroup', 'LargeGroup'])
})

test('browser regression: schedule pagination rejects an imported one-row page', async ({ page }) => {
  await page.addInitScript(() => localStorage.setItem('bareos_settings', JSON.stringify({
    tableRowsPerPage: { 'schedules.status': 1 },
  })))
  await browserFixture(page, command => {
    if (!command.startsWith('status scheduler days=')) return
    return { schedules: [{ name: 'Group', jobs: [{ name: 'Job' }] }], preview: [] }
  })
  await openNav(page, 'nav-schedules', /#\/schedules/)
  const panel = page.locator('.bareos-panel').filter({ hasText: 'Scheduler Jobs' })
  await expect(panel.getByRole('combobox', { name: 'Records per page:' })).toHaveValue('10')
})

test('browser regression: schedule All selection survives an empty scope', async ({ page }) => {
  await browserFixture(page, (command, director) => {
    if (!command.startsWith('status scheduler days=')) return
    return {
      schedules: director === 'bareos-dir'
        ? [{ name: 'DailyCycle', enabled: true, jobs: [] }]
        : [],
      preview: [],
    }
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  const scheduleCheckboxes = page.getByRole('checkbox')
  await expect(scheduleCheckboxes).toHaveCount(1)
  await expect(scheduleCheckboxes.first()).toHaveAttribute('aria-checked', 'true')

  const scopeMenu = page.getByTestId('director-scope-menu')
  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(scheduleCheckboxes).toHaveCount(0)

  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(scheduleCheckboxes).toHaveCount(1)
  await expect(scheduleCheckboxes.first()).toHaveAttribute('aria-checked', 'true')
})

test('browser regression: schedule status clears old scope during a failed switch', async ({ page }) => {
  let rejectStatus
  await browserFixture(page, (command, director) => {
    if (!command.startsWith('status scheduler days=')) return
    if (director === 'bareos-dir-2' && command !== 'status scheduler days=0,60') {
      return new Promise((resolve, reject) => {
        rejectStatus = () => reject(new Error('New status unavailable'))
      })
    }
    return {
      schedules: [{ name: 'OldScopeSchedule', jobs: [{ name: 'OldScopeJob', enabled: true }] }],
      preview: [{ schedule: 'OldScopeSchedule', job: 'OldScopeJob', runtime: Date.now() / 1000 + 3600 }],
    }
  })

  const scopeMenu = page.getByTestId('director-scope-menu')
  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect(page.locator('.sched-group-header')).toContainText('OldScopeSchedule')
  await expect(page.getByRole('checkbox', { name: 'OldScopeSchedule' })).toBeVisible()

  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect.poll(() => Boolean(rejectStatus)).toBe(true)
  await expect(page.locator('.sched-group-header')).toHaveCount(0)
  await expect(page.getByRole('checkbox', { name: 'OldScopeSchedule' })).toHaveCount(0)
  rejectStatus()
  await expect(page.getByText('New status unavailable')).toBeVisible()
  await expect(page.locator('.sched-group-header')).toHaveCount(0)
  await expect(page.getByText('OldScopeSchedule', { exact: true })).toHaveCount(0)
})

test('browser regression: schedule status ignores an obsolete aggregate scope', async ({ page }) => {
  let releaseObsolete
  let obsoleteToken
  let firstDirectorCalls = 0
  await browserFixture(page, (command, director, { responseToken }) => {
    if (!command.startsWith('status scheduler days=')) return
    if (command === 'status scheduler days=0,60') return { schedules: [], preview: [] }
    if (director === 'bareos-dir-2') {
      obsoleteToken = responseToken
      return new Promise(resolve => {
        releaseObsolete = () => resolve({
          schedules: [{ name: 'ObsoleteOther', jobs: [] }], preview: [],
        })
      })
    }
    firstDirectorCalls += 1
    return {
      schedules: [{ name: firstDirectorCalls === 2 ? 'ObsoleteAggregate' : 'CurrentScope', jobs: [] }],
      preview: [],
    }
  })

  const scopeMenu = page.getByTestId('director-scope-menu')
  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect(page.locator('.sched-group-header')).toContainText('CurrentScope')
  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await expect.poll(() => Boolean(releaseObsolete)).toBe(true)
  await expect(page.locator('.sched-group-header')).toHaveCount(0)
  await page.getByTestId('director-scope-control').click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(page.locator('.sched-group-header')).toContainText('CurrentScope')
  releaseObsolete()
  await waitForDirectorResponseProcessing(page, obsoleteToken)
  await expect(page.locator('.sched-group-header')).toContainText('CurrentScope')
  await expect(page.getByText('ObsoleteAggregate', { exact: true })).toHaveCount(0)
  await expect(page.getByText('ObsoleteOther', { exact: true })).toHaveCount(0)
})

test('browser regression: upcoming runs ignore an obsolete Director scope', async ({ page }) => {
  let releaseObsoletePreview
  let obsoletePreviewResponseToken
  let firstDirectorPreviewCalls = 0

  await browserFixture(page, (command, director, { responseToken }) => {
    if (command === 'status scheduler days=0,60') {
      if (director === 'bareos-dir') {
        firstDirectorPreviewCalls += 1
        if (firstDirectorPreviewCalls === 1) {
          obsoletePreviewResponseToken = responseToken
          return new Promise(resolve => {
            releaseObsoletePreview = () => resolve({
              schedules: [{ name: 'Old Director', jobs: [{ name: 'Old job', enabled: true }] }],
              preview: [{
                job: 'Old job',
                schedule: 'Old Director',
                datetime: '2026-05-28 01:00:00',
                runtime: Date.now() / 1000 + 3600,
              }],
            })
          })
        }

        return {
          schedules: [{ name: 'Current Director', jobs: [{ name: 'Current job', enabled: true }] }],
          preview: [{
            job: 'Current job',
            schedule: 'Current Director',
            datetime: '2026-05-28 02:00:00',
            runtime: Date.now() / 1000 + 7200,
          }],
        }
      }

      return {
        schedules: [{ name: 'Other Director', jobs: [{ name: 'Other job', enabled: true }] }],
        preview: [{
          job: 'Other job',
          schedule: 'Other Director',
          datetime: '2026-05-28 03:00:00',
          runtime: Date.now() / 1000 + 10800,
        }],
      }
    }

    if (command.startsWith('status scheduler days=')) {
      return { preview: [] }
    }
  })

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect.poll(() => Boolean(releaseObsoletePreview)).toBe(true)

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(page.getByTestId('director-scope-control')).toContainText('bareos-dir')
  await expect.poll(() => firstDirectorPreviewCalls).toBeGreaterThan(1)
  await expect(page.getByText(/Current Director/)).toBeVisible()

  releaseObsoletePreview()
  await waitForDirectorResponseProcessing(page, obsoletePreviewResponseToken)
  await expect(page.getByText(/Current Director/)).toBeVisible()
  await expect(page.getByText(/Old Director/)).toHaveCount(0)
})

test('browser regression: Definitions ignore a forced refresh from the previous scope', async ({ page }) => {
  let toggled = false
  let releaseObsolete
  let obsoleteToken
  const definitions = name => ({ schedules: { [name]: { name } } })
  await browserFixture(page, (command, director, { responseToken }) => {
    if (command === 'disable schedule="FirstSchedule"') {
      toggled = true
      return {}
    }
    if (command !== 'show schedules') return {}
    if (director === 'bareos-dir-2') return definitions('CurrentScopeSchedule')
    if (!toggled) return definitions('FirstSchedule')
    if (!releaseObsolete) {
      obsoleteToken = responseToken
      return new Promise(resolve => {
        releaseObsolete = () => resolve(definitions('ObsoleteScopeSchedule'))
      })
    }
    return definitions('FirstSchedule')
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  await page.getByTestId('schedules-tab-show').click()
  await page.getByRole('row').filter({ hasText: 'FirstSchedule' }).getByRole('switch').click()
  await expect.poll(() => Boolean(releaseObsolete)).toBe(true)
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(page.getByText('FirstSchedule', { exact: true })).toHaveCount(0)
  await expect(page.getByText('CurrentScopeSchedule', { exact: true })).toBeVisible()
  releaseObsolete()
  await waitForDirectorResponseProcessing(page, obsoleteToken)
  await expect(page.getByText('ObsoleteScopeSchedule', { exact: true })).toHaveCount(0)
  await expect(page.getByText('CurrentScopeSchedule', { exact: true })).toBeVisible()
})

test('browser regression: Next run follows eligible jobs after a toggle', async ({ page }) => {
  let dailyEnabled = true
  await browserFixture(page, (command, director) => {
    if (director !== 'bareos-dir') return {}
    if (command === 'disable job="DailyJob"') {
      dailyEnabled = false
      return {}
    }
    if (!command.startsWith('status scheduler days=')) return {}
    return {
      schedules: [
        { name: 'DailyCycle', enabled: true, jobs: [{ name: 'DailyJob', enabled: dailyEnabled }] },
        { name: 'WeeklyCycle', enabled: true, jobs: [{ name: 'WeeklyJob', enabled: true }] },
      ],
      preview: [
        { schedule: 'DailyCycle', runtime: Math.floor(Date.now() / 1000) + 3600 },
        { schedule: 'WeeklyCycle', runtime: Math.floor(Date.now() / 1000) + 7200 },
      ],
    }
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  const nextRun = page.locator('.schedules-list-stats').getByText(/Next run:/)
  await expect(nextRun).toContainText('DailyCycle')
  await page.getByRole('button', { name: 'Expand group', exact: true })
    .filter({ hasText: 'DailyCycle' }).click()
  const dailyRow = page.getByRole('row').filter({ has: page.getByRole('link', { name: 'DailyJob' }) })
  await dailyRow.getByRole('switch').click()
  await expect.poll(() => dailyEnabled).toBe(false)
  await expect(nextRun).toContainText('WeeklyCycle', { timeout: 2000 })
})

test('browser regression: Next run drops the previous scope even when refresh fails', async ({ page }) => {
  let firstDirectorCalls = 0
  let rejectNarrowedRequest
  let narrowedToken
  await browserFixture(page, (command, director, { responseToken }) => {
    if (command !== 'status scheduler days=0,60') return
    const first = director === 'bareos-dir'
    if (first && ++firstDirectorCalls > 1) {
      narrowedToken = responseToken
      return new Promise((_, reject) => {
        rejectNarrowedRequest = () => reject(new Error('Preview unavailable'))
      })
    }
    const name = first ? 'LaterRun' : 'RemovedDirectorRun'
    return {
      schedules: [{ name, jobs: [{ name: 'Backup', enabled: true }] }],
      preview: [{ schedule: name, runtime: Math.floor(Date.now() / 1000) + (first ? 7200 : 3600) }],
    }
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  const nextRun = page.locator('.schedules-list-stats').getByText(/Next run:/)
  await expect(nextRun).toContainText('RemovedDirectorRun')
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect.poll(() => Boolean(rejectNarrowedRequest)).toBe(true)
  await expect(nextRun).toHaveCount(0, { timeout: 2000 })
  rejectNarrowedRequest()
  await waitForDirectorResponseProcessing(page, narrowedToken)
  await expect(nextRun).toHaveCount(0)
})

test.describe('New York schedule calendar', () => {
  test.use({ timezoneId: 'America/New_York' })

  test('keeps Sunday evening when browsing back across fall DST', async ({ page }) => {
    const now = Date.parse('2026-11-02T17:00:00Z')
    const day = 86_400_000
    await page.clock.setFixedTime(new Date(now))
    await browserFixture(page, (command, director) => {
      const offsets = /^status scheduler days=(-?\d+),(-?\d+)$/.exec(command)
      if (!offsets || director !== 'bareos-dir') return {}
      const [, from, to] = offsets.map(Number)
      return {
        schedules: [{ name: 'Evening', enabled: true, jobs: [] }],
        preview: ['2026-11-01T23:00:00Z', '2026-11-02T23:00:00Z']
          .map(Date.parse)
          .filter(instant => instant >= now + from * day && instant < now + to * day)
          .map(instant => ({ schedule: 'Evening', runtime: instant / 1000 })),
      }
    })

    await openNav(page, 'nav-schedules', /#\/schedules/)
    await page.getByRole('button', { name: 'Week', exact: true }).click()
    await page.getByRole('button', { name: 'View schedule details for 2026-11-02' }).click()
    await page.getByRole('button', { name: 'Previous day' }).click()
    await expect(page.getByRole('img', { name: /Evening.*18:00/ })).toBeVisible({ timeout: 3000 })
  })
})

test('browser regression: schedule status ignores an obsolete calendar range', async ({ page }) => {
  let releaseObsoleteStatus
  let obsoleteResponseToken
  let obsoleteSocket
  let settledSocket
  let statusRequests = 0

  await browserFixture(page, (command, _director, { responseToken, socket }) => {
    if (!page.url().includes('/#/schedules')
      || !command.startsWith('status scheduler days=')
      || command === 'status scheduler days=0,60') {
      return
    }

    statusRequests += 1
    if (statusRequests === 1) {
      obsoleteResponseToken = responseToken
      obsoleteSocket = socket
      return new Promise(resolve => {
        releaseObsoleteStatus = () => resolve({
          schedules: [{ name: 'ObsoleteRange', enabled: true, jobs: [] }],
          preview: [],
        })
      })
    }

    if (statusRequests > 2) settledSocket = socket
    return {
      schedules: [{
        name: statusRequests === 2 ? 'CurrentRange' : 'SettledRange',
        enabled: true,
        jobs: [],
      }],
      preview: [],
    }
  })

  await page.getByTestId('director-scope-control').click()
  const scopeMenu = page.getByTestId('director-scope-menu')
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect.poll(() => Boolean(releaseObsoleteStatus)).toBe(true)
  await page.getByRole('button', { name: 'Next period' }).click()
  await expect.poll(() => statusRequests).toBeGreaterThan(1)
  const scheduleGroups = page.locator('.sched-group-header')
  await expect(scheduleGroups).toContainText('CurrentRange')

  // The next response is a same-socket barrier; record transient stale renders
  // so its current result cannot hide an obsolete overwrite.
  await page.evaluate(() => {
    window.obsoleteScheduleSeen = false
    window.obsoleteScheduleObserver = new MutationObserver(() => {
      if ([...document.querySelectorAll('.sched-group-header')]
        .some(header => header.textContent.includes('ObsoleteRange'))) {
        window.obsoleteScheduleSeen = true
      }
    })
    window.obsoleteScheduleObserver.observe(document.body, {
      childList: true, subtree: true, characterData: true,
    })
  })
  releaseObsoleteStatus()
  await directorResponseCompletions.get(page).get(obsoleteResponseToken).sent
  await page.getByRole('button', { name: 'Next period' }).click()
  await expect.poll(() => statusRequests).toBeGreaterThan(2)
  expect(settledSocket).toBe(obsoleteSocket)
  await expect(scheduleGroups).toContainText('SettledRange')
  await expect(scheduleGroups).not.toContainText('ObsoleteRange')
  const obsoleteScheduleSeen = await page.evaluate(() => {
    window.obsoleteScheduleObserver.disconnect()
    return window.obsoleteScheduleSeen
  })
  expect(obsoleteScheduleSeen).toBe(false)
})

test('browser regression: clients list ignores an obsolete refresh', async ({ page }) => {
  let releaseObsoleteClients
  let obsoleteClientsResponseToken
  let obsoleteSocket
  let barrierSocket
  let clientListRequests = 0

  await browserFixture(page, (command, _director, { responseToken, socket }) => {
    if (command === 'llist clients') {
      clientListRequests += 1
      if (clientListRequests === 1) {
        obsoleteClientsResponseToken = responseToken
        obsoleteSocket = socket
        return new Promise(resolve => {
          releaseObsoleteClients = () => resolve({
            clients: [{ name: 'ObsoleteClient' }],
          })
        })
      }
      if (clientListRequests === 2) return { clients: [{ name: 'CurrentClient' }] }
      barrierSocket = socket
      return { clients: [{ name: 'SettledClient' }] }
    }
    if (command === '.clients') return { clients: [] }
    if (command.startsWith('llist jobs reverse limit=1000')) return { jobs: [] }
  })

  await page.getByTestId('director-scope-control').click()
  const scopeMenu = page.getByTestId('director-scope-menu')
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-clients', /#\/clients/)
  await expect.poll(() => Boolean(releaseObsoleteClients)).toBe(true)
  await page.getByRole('button', { name: 'Refresh' }).click()
  await expect(page.getByText('CurrentClient', { exact: true })).toBeVisible()

  await page.evaluate(() => {
    window.obsoleteClientSeen = false
    const observer = new MutationObserver(() => {
      if (document.body.textContent.includes('ObsoleteClient')) {
        window.obsoleteClientSeen = true
      }
    })
    observer.observe(document.body, { childList: true, subtree: true, characterData: true })
  })
  releaseObsoleteClients()
  await waitForDirectorResponseProcessing(page, obsoleteClientsResponseToken)
  await page.getByRole('button', { name: 'Refresh' }).click()
  await expect(page.getByText('SettledClient', { exact: true })).toBeVisible()
  expect(barrierSocket).toBe(obsoleteSocket)
  await expect(page.getByText('ObsoleteClient', { exact: true })).toHaveCount(0)
  expect(await page.evaluate(() => window.obsoleteClientSeen)).toBe(false)
})

test('browser regression: filesets ignore an obsolete forced refresh', async ({ page }) => {
  let releaseOld
  let oldToken
  const requests = new Map()
  await browserFixture(page, (command, director, { responseToken }) => {
    if (command !== 'list filesets') return
    const count = (requests.get(director) ?? 0) + 1
    requests.set(director, count)
    if (director === 'bareos-dir' && count === 2) {
      oldToken = responseToken
      return new Promise(resolve => {
        releaseOld = () => resolve({ filesets: [{ fileset: 'ObsoleteFileset' }] })
      })
    }
    return { filesets: [{ fileset: count >= 3 ? 'NewestFileset' : 'CurrentFileset' }] }
  })
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await page.goto('/#/filesets')
  await expect(page.getByText('CurrentFileset').first()).toBeVisible()
  await page.getByRole('button', { name: 'Refresh' }).click()
  await expect.poll(() => Boolean(releaseOld)).toBe(true)
  await page.getByRole('button', { name: 'Refresh' }).click()
  await expect(page.getByText('NewestFileset').first()).toBeVisible()
  releaseOld()
  await waitForDirectorResponseProcessing(page, oldToken)
  await expect(page.getByText('ObsoleteFileset')).toHaveCount(0)
  await expect(page.getByText('NewestFileset').first()).toBeVisible()
})

test('browser regression: client backup status distinguishes running from no history', async ({ page }) => {
  await browserFixture(page, command => {
    if (command === 'llist clients') {
      return { clients: [{ name: 'RunningClient' }, { name: 'NewClient' }] }
    }
    if (command === '.clients') return { clients: [] }
    if (command.startsWith('llist jobs reverse limit=1000')) {
      return {
        jobs: [{
          jobid: '101',
          name: 'Running backup',
          client: 'RunningClient',
          jobstatus: 'R',
          starttime: '2026-05-28 10:00:00',
        }],
      }
    }
    if (command.startsWith('status scheduler days=')) return { schedules: [], preview: [] }
  })

  await openNav(page, 'nav-clients', /#\/clients/)
  const runningClient = page.getByRole('row').filter({ hasText: 'RunningClient' }).first()
  const newClient = page.getByRole('row').filter({ hasText: 'NewClient' }).first()
  await expect(runningClient).toContainText('Running')
  await expect(runningClient).not.toContainText('No backup history')
  await expect(newClient).toContainText('No backup history')
})

test('browser regression: next run advances with the live clock', async ({ page }) => {
  const now = Date.parse('2026-05-28T12:00:00Z')
  await page.clock.install({ time: new Date(now) })

  await browserFixture(page, command => {
    if (command.startsWith('status scheduler days=')) {
      return {
        schedules: [
          { name: 'SoonSchedule', jobs: [{ name: 'SoonJob', enabled: true }] },
          { name: 'LaterSchedule', jobs: [{ name: 'LaterJob', enabled: true }] },
        ],
        preview: [
          {
            job: 'SoonJob',
            schedule: 'SoonSchedule',
            datetime: '2026-05-28 12:00:30',
            runtime: (now + 30_000) / 1000,
          },
          {
            job: 'LaterJob',
            schedule: 'LaterSchedule',
            datetime: '2026-05-28 12:01:30',
            runtime: (now + 90_000) / 1000,
          },
        ],
      }
    }
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  const stats = page.locator('.schedules-list-stats')
  await expect(stats).toContainText('SoonSchedule')

  await page.clock.fastForward(60_000)
  await expect(stats).toContainText('LaterSchedule')
  await expect(stats).not.toContainText('SoonSchedule')
})

test('browser regression: schedule selection follows scope and preserves None', async ({ page }) => {
  await browserFixture(page, (command, director) => {
    const schedule = director === 'bareos-dir' ? 'DailyCycle' : 'WeeklyCycle'
    if (command.startsWith('status scheduler days=')) {
      return { schedules: [{ name: schedule, enabled: true, jobs: [] }], preview: [] }
    }
  })

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-schedules', /#\/schedules/)
  const scheduleCheckboxes = page.getByRole('checkbox')
  await expect(scheduleCheckboxes).toHaveCount(1)
  await expect(scheduleCheckboxes.first()).toHaveAttribute('aria-checked', 'true')

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await expect(scheduleCheckboxes).toHaveCount(2)
  for (const checkbox of await scheduleCheckboxes.all()) {
    await expect(checkbox).toHaveAttribute('aria-checked', 'true')
  }

  await page.getByRole('button', { name: 'None', exact: true }).click()
  for (const checkbox of await scheduleCheckboxes.all()) {
    await expect(checkbox).toHaveAttribute('aria-checked', 'false')
  }

  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await expect(scheduleCheckboxes).toHaveCount(1)
  await expect(scheduleCheckboxes.first()).toHaveAttribute('aria-checked', 'false')
})

test('browser regression: schedule custom selection survives a narrower scope', async ({ page }) => {
  await browserFixture(page, (command, director) => {
    if (!command.startsWith('status scheduler days=')) return
    const prefix = director === 'bareos-dir' ? 'A' : 'B'
    return {
      schedules: ['1', '2'].map(number => ({ name: `${prefix}${number}`, jobs: [] })),
      preview: [],
    }
  })

  await openNav(page, 'nav-schedules', /#\/schedules/)
  const check = (name) => page.getByRole('checkbox', { name, exact: true })
  await expect(check('bareos-dir / A1')).toBeChecked()
  await expect(check('bareos-dir-2 / B2')).toBeChecked()
  await check('bareos-dir / A2').click()
  await check('bareos-dir-2 / B2').click()

  const scopeMenu = page.getByTestId('director-scope-menu')
  const narrow = async () => {
    await page.getByTestId('director-scope-control').click()
    await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
    await page.keyboard.press('Escape')
    await expect(check('A1')).toBeVisible()
  }
  const broaden = async () => {
    await page.getByTestId('director-scope-control').click()
    await scopeMenu.getByRole('button', { name: 'Select all' }).click()
    await page.keyboard.press('Escape')
    await expect(check('bareos-dir-2 / B2')).toBeVisible()
  }

  await narrow()
  await expect(check('A1')).toBeChecked()
  await expect(check('A2')).not.toBeChecked()
  await check('A2').click()
  await expect(page.getByRole('button', { name: 'All', exact: true })).toBeEnabled()
  await broaden()
  await expect(check('bareos-dir / A1')).toBeChecked()
  await expect(check('bareos-dir / A2')).toBeChecked()
  await expect(check('bareos-dir-2 / B1')).toBeChecked()
  await expect(check('bareos-dir-2 / B2')).not.toBeChecked()

  await narrow()
  await page.getByRole('button', { name: 'All', exact: true }).click()
  await broaden()
  await expect(check('bareos-dir-2 / B2')).toBeChecked()
  await check('bareos-dir / A1').click()
  await check('bareos-dir / A2').click()
  await narrow()
  await expect(check('A1')).not.toBeChecked()
  await expect(check('A2')).not.toBeChecked()
  await expect(page.getByRole('button', { name: 'None', exact: true })).toBeEnabled()
  await page.getByRole('button', { name: 'None', exact: true }).click()
  await broaden()
  await expect(check('bareos-dir-2 / B1')).not.toBeChecked()
  await expect(check('bareos-dir-2 / B2')).not.toBeChecked()
})

test('browser regression: log focus follows reused job routes', async ({ page }) => {
  await browserFixture(page, command => {
    const job = command.match(/^llist jobid=(\d+)$/)
    if (job) return { jobs: [{ jobid: job[1], name: 'Focus fixture', jobstatus: 'T', type: 'B' }] }
    const log = command.match(/^list joblog jobid=(\d+)$/)
    if (log) return { joblog: [
      { logtext: `Warning: job ${log[1]}` },
      { logtext: `Error: job ${log[1]}` },
      { logtext: 'Termination: Backup OK' },
    ] }
  })
  const focused = page.locator('.log-line--focused')
  await page.goto('/#/jobs/101?logFocus=error')
  await expect(focused).toHaveText('Error: job 101')
  await page.goto('/#/jobs/202?logFocus=warning')
  await expect(focused).toHaveText('Warning: job 202')
  await page.goto('/#/jobs/202?logFocus=ok')
  await expect(focused).toHaveText('Termination: Backup OK')
  await page.goto('/#/jobs/101')
  await expect(page.locator('.log-line').first()).toHaveText('Warning: job 101')
  await expect(focused).toHaveCount(0)
  await page.getByRole('button', { name: 'Next issue', exact: true }).click()
  await expect(focused).toHaveText('Warning: job 101')
})

test('browser regression: unknown terminated status filters its jobs', async ({ page }) => {
  await browserFixture(page, command => {
    if (command === 'status director') {
      return { terminated: [{ jobid: '321', name: 'UnknownStatusJob', status: '' }] }
    }
  })
  await openNav(page, 'nav-director', /#\/director/)
  const card = page.locator('.bareos-panel').filter({ hasText: 'Terminated Jobs' })
  await expect(card.getByText('UnknownStatusJob', { exact: true }).first()).toBeVisible()
  await card.getByText(/Unknown: [1-9]/).click()
  await expect(card.getByText('UnknownStatusJob', { exact: true }).first()).toBeVisible()
})

test('browser regression: waiting job opens the log section', async ({ page }) => {
  await browserFixture(page, command => {
    if (command === 'status director') {
      return { running: [{ jobid: '101', status: 'Job is waiting for mount' }] }
    }
    if (command.startsWith('llist job') && !command.startsWith('llist jobs count')) {
      return { jobs: [{ jobid: '101', name: 'Waiting job', jobstatus: 'R', type: 'B' }] }
    }
    if (command.startsWith('list joblog')) return { joblog: [] }
  })
  await page.getByTestId('director-scope-control').click()
  const scopeMenu = page.getByTestId('director-scope-menu')
  await scopeMenu.getByRole('button', { name: 'Select all' }).click()
  await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.setViewportSize({ width: 800, height: 400 })
  await page.getByRole('button', { name: /Job is waiting for mount: Jump to log/ }).first().click()
  await expect(page).toHaveURL(/logFocus=section/)
  await expect(page.getByTestId('job-log-section')).toBeInViewport()
})

test('browser regression: slow job polling can complete', async ({ page }) => {
  await page.clock.install()
  let requests = 0
  let releasePoll
  await browserFixture(page, command => {
    if (command === 'llist jobid=101') {
      requests++
      if (requests === 1) return { jobs: [{ jobid: '101', name: 'Polling job', jobstatus: 'R', type: 'B' }] }
      return new Promise(resolve => { releasePoll = resolve })
    }
  })
  await page.goto('/#/jobs/101?director=bareos-dir')
  await expect(page.getByText('Job #101 — Polling job')).toBeVisible()
  await page.clock.fastForward(5000)
  await expect.poll(() => requests).toBe(2)
  await page.clock.fastForward(5000)
  expect(requests).toBe(2)
  releasePoll({ jobs: [{ jobid: '101', name: 'Polling job', jobstatus: 'T', type: 'B' }] })
  await expect(page.getByRole('status', { name: 'OK', exact: true })).toBeVisible()
})

test('browser regression: job details recover after a failed poll', async ({ page }) => {
  await page.clock.install()
  let requests = 0
  await browserFixture(page, command => {
    if (command === 'llist jobid=101') {
      requests++
      if (requests === 2) throw new Error('Temporary polling failure')
      return { jobs: [{ jobid: '101', name: 'Polling job', jobstatus: 'R', type: 'B' }] }
    }
  })
  await page.goto('/#/jobs/101?director=bareos-dir')
  await expect(page.getByText('Job #101 — Polling job')).toBeVisible()
  await page.clock.fastForward(5000)
  await expect(page.getByText('Temporary polling failure')).toBeVisible()
  await expect(page.getByTestId('job-details-cancel')).toHaveCount(0)
  await page.clock.fastForward(5000)
  await expect.poll(() => requests).toBe(3)
  await expect(page.getByText('Temporary polling failure')).toHaveCount(0)
  await expect(page.getByText('Job #101 — Polling job')).toBeVisible()
  await expect(page.getByTestId('job-details-cancel')).toBeVisible()
  await expect(page.getByTestId('job-log-section')).toBeVisible()
})

test('browser regression: stale successful load cannot clear a newer job error', async ({ page }) => {
  let releaseOldJob
  await browserFixture(page, command => {
    if (command === 'llist jobid=101') {
      return new Promise(resolve => { releaseOldJob = resolve })
    }
    if (command === 'llist jobid=202') throw new Error('Current job failed')
  })
  await page.goto('/#/jobs/101?director=bareos-dir')
  await expect.poll(() => Boolean(releaseOldJob)).toBe(true)
  await page.evaluate(() => { window.location.hash = '#/jobs/202?director=bareos-dir' })
  await expect(page.getByText('Current job failed')).toBeVisible()
  releaseOldJob({ jobs: [{ jobid: '101', name: 'Stale job', jobstatus: 'T', type: 'B' }] })
  await expect.poll(() => page.evaluate(
    () => window.__fixtureMessages.some(message => message.includes('Stale job'))
  )).toBe(true)
  await expect(page.getByText('Current job failed')).toBeVisible()
})

test('browser regression: stale job loads cannot replace the selected job', async ({ page }) => {
  let releaseOldJob
  await browserFixture(page, command => {
    if (command === 'llist jobid=101') {
      return new Promise(resolve => { releaseOldJob = resolve })
    }
    if (command === 'llist jobid=202') {
      return { jobs: [{ jobid: '202', name: 'Current job', jobstatus: 'T', type: 'B' }] }
    }
  })
  await page.goto('/#/jobs/101?director=bareos-dir')
  await expect.poll(() => Boolean(releaseOldJob)).toBe(true)
  await page.evaluate(() => { window.location.hash = '#/jobs/202?director=bareos-dir' })
  await expect(page.getByText('Job #202 — Current job')).toBeVisible()
  releaseOldJob({ jobs: [{ jobid: '101', name: 'Stale job', jobstatus: 'T', type: 'B' }] })
  await expect.poll(() => page.evaluate(
    () => window.__fixtureMessages.some(message => message.includes('Stale job'))
  )).toBe(true)
  await expect(page.getByText('Job #202 — Current job')).toBeVisible()
  await expect(page.getByText('Stale job')).toHaveCount(0)
})

test('browser regression: run defaults follow the selected Director', async ({ page }) => {
  let releaseDefaults
  const runs = []
  await browserFixture(page, (command, director) => {
    if (command === 'show jobs') return { jobs: [{ name: 'Shared job', type: 'Backup', enabled: true }] }
    if (command === '.jobs') return { jobs: [{ name: 'Shared job' }] }
    if (command === '.jobs type=R') return { jobs: [] }
    for (const [query, key] of [['.clients', 'clients'], ['.filesets', 'filesets'], ['.pools', 'pools'], ['.storage', 'storages']]) {
      if (command === query) return { [key]: [{ name: 'Shared resource' }, { name: `${director} resource` }] }
    }
    if (command === '.defaults job="Shared job"') {
      const defaults = {
        client: `${director} resource`, fileset: 'Shared resource',
        pool: `${director} resource`, storage: `${director} resource`,
        level: director === 'bareos-dir' ? 'Full' : 'Differential',
      }
      if (director === 'bareos-dir-2') return new Promise(resolve => {
        releaseDefaults = () => resolve({ defaults })
      })
      return { defaults }
    }
    if (command.startsWith('run ')) {
      runs.push({ command, director })
      return { run: { jobid: '303' } }
    }
  })
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByTestId('jobs-tab-run').click()
  await page.getByRole('button', { name: 'Customize & Start', exact: true }).first().click()
  const dialog = page.locator('.configure-start-dialog')
  const submit = dialog.getByTestId('run-job-submit')
  const selectDirector = async name => {
    await dialog.getByLabel('Director', { exact: true }).fill(name)
    await page.getByRole('option', { name, exact: true }).click()
  }
  await expect(dialog.getByLabel('Client', { exact: true })).toHaveValue('bareos-dir resource')
  await selectDirector('bareos-dir-2')
  await expect(submit).toBeDisabled()
  await expect.poll(() => Boolean(releaseDefaults)).toBe(true)
  const staleDefaults = releaseDefaults
  await selectDirector('bareos-dir')
  await expect(submit).toBeEnabled()
  staleDefaults()
  await selectDirector('bareos-dir-2')
  await expect(submit).toBeDisabled()
  await expect.poll(() => releaseDefaults !== staleDefaults).toBe(true)
  releaseDefaults()
  await expect(dialog.getByLabel('Client', { exact: true })).toHaveValue('bareos-dir-2 resource')
  await expect(dialog.getByLabel('Pool', { exact: true })).toHaveValue('bareos-dir-2 resource')
  await expect(dialog.getByLabel('Level', { exact: true })).toHaveValue('Differential')
  await expect(submit).toBeEnabled()
  await submit.click()
  await expect.poll(() => runs.length).toBe(1)
  expect(runs[0].director).toBe('bareos-dir-2')
  expect(runs[0].command).toContain('client="bareos-dir-2 resource"')
  expect(runs[0].command).toContain('pool="bareos-dir-2 resource"')
  expect(runs[0].command).toContain('level="Differential"')
})

for (const outcome of ['success', 'failure', 'reselection']) {
  test(`browser regression: selected job defaults gate submission on ${outcome}`, async ({ page }) => {
    const requests = []
    const runs = []
    let initialDefaults = true
    const defaults = name => ({
      client: `${name} resource`, fileset: `${name} resource`,
      pool: `${name} resource`, storage: `${name} resource`,
      level: name === 'A' ? 'Full' : 'Differential',
    })
    await browserFixture(page, command => {
      if (command === 'show jobs') return { jobs: [
        { name: 'Job A', type: 'Backup', enabled: true },
        { name: 'Job B', type: 'Backup', enabled: true },
      ] }
      if (command === '.jobs') return { jobs: [{ name: 'Job A' }, { name: 'Job B' }] }
      if (command === '.jobs type=R') return { jobs: [] }
      for (const [query, key] of [['.clients', 'clients'], ['.filesets', 'filesets'], ['.pools', 'pools'], ['.storage', 'storages']]) {
        if (command === query) return { [key]: [{ name: 'A resource' }, { name: 'B resource' }] }
      }
      const match = command.match(/^\.defaults job="Job ([AB])"$/)
      if (match) {
        if (match[1] === 'A' && initialDefaults) {
          initialDefaults = false
          return { defaults: defaults('A') }
        }
        return new Promise((resolve, reject) => {
          requests.push({ resolve, reject })
        })
      }
      if (command.startsWith('run ')) {
        runs.push(command)
        return { run: { jobid: '303' } }
      }
    })
    await openNav(page, 'nav-jobs', /#\/jobs/)
    await page.getByTestId('jobs-tab-run').click()
    await page.getByRole('button', { name: 'Customize & Start', exact: true }).first().click()
    const dialog = page.locator('.configure-start-dialog')
    const submit = dialog.getByTestId('run-job-submit')
    const jobField = dialog.getByLabel('Job *', { exact: true })
    const selectJob = async name => {
      await jobField.fill(name)
      await page.getByRole('option', { name, exact: true }).click()
    }
    await expect(dialog.getByLabel('Client', { exact: true })).toHaveValue('A resource')
    await expect(submit).toBeEnabled()
    await selectJob('Job B')
    await expect.poll(() => requests.length).toBe(1)
    await expect(submit).toBeDisabled()
    await expect(jobField).toBeEnabled()
    expect(runs).toEqual([])

    if (outcome === 'failure') {
      requests[0].reject(new Error('Selected job defaults denied'))
      await expect(page.locator('.q-notification__message')).toContainText('Selected job defaults denied')
      await expect(submit).toBeDisabled()
      await selectJob('Job A')
      await expect.poll(() => requests.length).toBe(2)
      requests[1].resolve({ defaults: defaults('A') })
      await expect(submit).toBeEnabled()
      expect(runs).toEqual([])
      return
    }

    if (outcome === 'reselection') {
      await selectJob('Job A')
      await expect.poll(() => requests.length).toBe(2)
      await selectJob('Job B')
      await expect.poll(() => requests.length).toBe(3)
      requests[0].resolve({ defaults: defaults('B') })
      requests[1].reject(new Error('Stale job defaults denied'))
      await expect.poll(() => page.evaluate(
        () => window.__fixtureMessages.some(message => message.includes('Stale job defaults denied'))
      )).toBe(true)
      await expect(submit).toBeDisabled()
      await expect(dialog.getByLabel('Client', { exact: true })).toHaveValue('A resource')
      await expect(page.getByText(/Stale job defaults denied/)).not.toBeVisible()
      requests[2].resolve({ defaults: defaults('B') })
    } else {
      requests[0].resolve({ defaults: defaults('B') })
    }

    await expect(dialog.getByLabel('Client', { exact: true })).toHaveValue('B resource')
    await expect(submit).toBeEnabled()
    await submit.click()
    await expect.poll(() => runs.length).toBe(1)
    expect(runs[0]).toContain('job="Job B"')
    for (const key of ['client', 'fileset', 'pool', 'storage']) {
      expect(runs[0]).toContain(`${key}="B resource"`)
    }
    expect(runs[0]).toContain('level="Differential"')
  })
}

test('browser regression: run defaults clear an unavailable job', async ({ page }) => {
  const defaultsCalls = []
  await browserFixture(page, (command, director) => {
    if (command === 'show jobs') return { jobs: [{ name: 'Shared job', type: 'Backup' }] }
    if (command === '.jobs') return { jobs: [{ name: director === 'bareos-dir' ? 'Shared job' : 'Other job' }] }
    if (command === '.jobs type=R') return { jobs: [] }
    if (command.startsWith('.defaults ')) {
      defaultsCalls.push(director)
      return { defaults: { level: 'Full' } }
    }
  })
  await page.getByTestId('director-scope-control').click()
  await page.getByTestId('director-scope-menu').getByRole('button', { name: 'Select all' }).click()
  await page.keyboard.press('Escape')
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByTestId('jobs-tab-run').click()
  await page.getByRole('button', { name: 'Customize & Start', exact: true }).first().click()
  const dialog = page.locator('.configure-start-dialog')
  await expect(dialog.getByTestId('run-job-submit')).toBeEnabled()
  await dialog.getByLabel('Director', { exact: true }).fill('bareos-dir-2')
  await page.getByRole('option', { name: 'bareos-dir-2', exact: true }).click()
  await dialog.getByLabel('Job *', { exact: true }).click()
  await expect(page.getByRole('option', { name: 'Other job', exact: true })).toBeVisible()
  await page.keyboard.press('Escape')
  await expect(dialog.getByLabel('Job *', { exact: true })).toHaveValue('')
  await expect(dialog.getByLabel('Level', { exact: true })).toHaveValue('Incremental')
  await expect(dialog.getByTestId('run-job-submit')).toBeDisabled()
  expect(defaultsCalls).toEqual(['bareos-dir'])
})

for (const [jobId, directorName] of [['202', 'bareos-dir'], ['101', 'bareos-dir-2']]) {
  test(`browser regression: comment target survives navigation to ${directorName}/${jobId}`, async ({ page }) => {
    let resolveSave
    const updates = []
    await browserFixture(page, (command, director) => {
      const job = command.match(/^llist jobid=(\d+)$/)
      if (job) return { jobs: [{
        jobid: job[1], name: 'Comment fixture', jobstatus: 'T', type: 'B',
        comment: `Original ${director}/${job[1]}`,
      }] }
      if (command.startsWith('update jobid=')) {
        updates.push({ command, director })
        if (updates.length === 1) return new Promise(resolve => { resolveSave = resolve })
        return {}
      }
    })
    await page.goto('/#/jobs/101?director=bareos-dir')
    await page.getByTestId('job-comment-edit').click()
    const dialog = page.getByTestId('comment-edit-dialog')
    await dialog.getByLabel('Comment', { exact: true }).fill('Old target comment')
    await page.getByTestId('comment-edit-save').click()
    await expect.poll(() => updates.length).toBe(1)
    await page.goto(`/#/jobs/${jobId}?director=${directorName}`)
    await expect(dialog).not.toBeVisible()
    const original = `Original ${directorName}/${jobId}`
    await expect(page.getByText(original, { exact: true })).toBeVisible()
    await page.getByTestId('job-comment-edit').click()
    await expect(dialog.getByLabel('Comment', { exact: true })).toHaveValue(original)
    resolveSave({})
    await dialog.getByLabel('Comment', { exact: true }).fill('New target comment')
    await expect(page.getByText(original, { exact: true })).toBeVisible()
    await page.getByTestId('comment-edit-save').click()
    await expect(dialog).not.toBeVisible()
    await expect(page.getByText('New target comment', { exact: true })).toBeVisible()
    expect(updates).toEqual([
      { command: 'update jobid=101 comment="Old target comment"', director: 'bareos-dir' },
      { command: `update jobid=${jobId} comment="New target comment"`, director: directorName },
    ])
  })
}

test('browser regression: pending comment saves resist dismissal and allow retry', async ({ page }) => {
  let rejectSave
  let updates = 0
  await browserFixture(page, command => {
    if (command.startsWith('update jobid=')) {
      updates += 1
      if (updates === 1) return new Promise((_resolve, reject) => { rejectSave = reject })
      return {}
    }
  })
  await page.goto('/#/jobs/101')
  await page.getByTestId('job-comment-edit').click()
  const dialog = page.getByTestId('comment-edit-dialog')
  await dialog.getByLabel('Comment', { exact: true }).fill('Retried comment')
  await page.getByTestId('comment-edit-save').click()
  await expect.poll(() => updates).toBe(1)
  await page.keyboard.press('Escape')
  await expect(dialog).toBeVisible()
  await page.locator('.q-dialog__backdrop').click({ position: { x: 5, y: 5 }, force: true })
  await expect(dialog).toBeVisible()
  await expect(dialog.getByRole('button', { name: 'Cancel' })).toBeDisabled()
  rejectSave(new Error('Comment update denied'))
  await expect(page.getByTestId('comment-edit-error')).toHaveText('Comment update denied')
  await expect(dialog.getByLabel('Comment', { exact: true })).toHaveValue('Retried comment')
  await page.getByTestId('comment-edit-save').click()
  await expect(dialog).not.toBeVisible()
  await expect(page.getByText('Retried comment', { exact: true })).toBeVisible()
  expect(updates).toBe(2)
})

async function expectConnected(page) {
  await expect(page.locator('[data-testid="director-status-label"]')).toContainText(
    'Connected'
  )

  if (expectedDirectorTransport) {
    await page.locator('[data-testid="director-scope-control"]').click()
    await expect(
      page.locator('[data-testid="director-scope-menu"]')
    ).toContainText(expectedDirectorTransport)
    await page.keyboard.press('Escape')
  }
}

async function login(
  page,
  {
    loginUsername = username,
    loginPassword = password,
    shouldSucceed = true,
  } = {}
) {
  const loginForm = page.locator('[data-testid="login-form"]')
  const loginError = page.locator('[data-testid="login-error"]')

  for (let attempt = 1; attempt <= LOGIN_ATTEMPTS; attempt += 1) {
    await page.goto('/')
    await page.waitForFunction(() => (
      window.location.hash.startsWith('#/dashboard')
      || document.querySelector('[data-testid="login-form"]')
    ), { timeout: LOGIN_RESULT_TIMEOUT_MS })

    if (page.url().includes('/#/dashboard')) {
      if (!shouldSucceed) {
        throw new Error('Invalid credentials unexpectedly restored a session')
      }
      await expectConnected(page)
      return
    }

    await expect(loginForm).toBeVisible()
    await page.getByLabel('Username').fill(loginUsername)
    await page.getByLabel('Password').fill(loginPassword)
    await page.getByRole('button', { name: 'Login' }).click()

    await page.waitForFunction(() => (
      window.location.hash.startsWith('#/dashboard')
      || document.querySelector('[data-testid="login-error"]')?.textContent?.trim()
    ), { timeout: LOGIN_RESULT_TIMEOUT_MS })

    if (!shouldSucceed) {
      await expect(loginError).toBeVisible()
      await expect(page).toHaveURL(/#\/login$/)
      return
    }

    if (page.url().includes('/#/dashboard')) {
      await expectConnected(page)
      return
    }

    const message = (await loginError.textContent())?.trim() || 'Unknown error'
    const transientConnectionError
      = /connection error|websocket connection failed|could not connect to director/i.test(message)
    if (!transientConnectionError || attempt === LOGIN_ATTEMPTS) {
      throw new Error(`Login failed: ${message}`)
    }

    await page.waitForTimeout(1_000)
  }
}

async function openNav(page, testId, urlPattern) {
  await page.locator(`[data-testid="${testId}"]`).click()
  await page.waitForURL(urlPattern)
}

async function openConsole(page) {
  const consoleOutput = page.locator('[data-testid="console-output"]')
  await page.goto('/#/console-popup')
  await expect(consoleOutput).toBeVisible()

  for (let attempt = 0; attempt < 2; attempt += 1) {
    try {
      await expect(consoleOutput).toContainText('Connected to bareos-dir', {
        timeout: 5000,
      })
      return consoleOutput
    } catch {
      await page.getByTitle('Reconnect').click()
    }
  }

  await expect(consoleOutput).toContainText('Connected to bareos-dir')
  return consoleOutput
}

async function selectFirstQOption(
  page,
  testId,
  {
    optionTimeoutMs = 5000,
    selected,
    hasText,
  } = {}
) {
  const field = page.locator(`[data-testid="${testId}"]`)
    .locator('xpath=ancestor::*[contains(@class,"q-field")]')
    .first()
  const combobox = field.getByRole('combobox')
  await expect(field).toBeVisible({ timeout: 20000 })

  const deadline = Date.now() + optionTimeoutMs
  const remaining = () => Math.max(1, deadline - Date.now())
  let lastError
  let optionSelected = false
  while (Date.now() < deadline) {
    try {
      await combobox.click({ timeout: remaining() })
      const listboxId = await combobox.getAttribute('aria-controls', { timeout: remaining() })
      if (!listboxId) {
        throw new Error(`Could not find the option list for ${testId}`)
      }
      let options = page.locator(`[id="${listboxId}"]`).getByRole('option')
      if (hasText) {
        options = options.filter({ hasText })
      }
      const option = options.first()
      await expect(option).toBeVisible({
        timeout: Math.min(1000, remaining()),
      })
      await option.click({ timeout: remaining() })
      optionSelected = true
      break
    } catch (error) {
      lastError = error
      await page.keyboard.press('Escape')
      await page.waitForTimeout(Math.min(300, remaining()))
    }
  }

  if (!optionSelected) {
    throw new Error(`Could not open option menu for ${testId}`, { cause: lastError })
  }
  if (selected) await selected()
}

async function waitForQSelectReady(page, testId, deadline = Date.now() + 20000) {
  const field = page.locator(`[data-testid="${testId}"]`)
    .locator('xpath=ancestor::*[contains(@class,"q-field")]')
    .first()
  const remaining = () => Math.max(1, deadline - Date.now())
  await expect(field).toBeVisible({ timeout: remaining() })
  await expect(field).not.toHaveClass(/q-field--loading/, { timeout: remaining() })
  await expect(field).not.toHaveClass(/q-field--disabled/, { timeout: remaining() })
}

// The test schedules only fire on Sundays at 03:00/03:10 and Mon-Sat at
// 21:00, so the default ±12 h window contains no trigger from Sunday
// 15:10 until Monday 09:00. Zoom out (up to ±3.5 d) until one is shown.
async function expectScheduleTick(page) {
  const timeline = page.getByTestId('combined-job-schedule-timeline').first()
  const tick = timeline.getByTestId('combined-timeline-schedule-tick').first()
  for (let zoomOuts = 0; zoomOuts < 2; zoomOuts += 1) {
    try {
      await tick.waitFor({ state: 'visible', timeout: 5000 })
      return
    } catch {
      await timeline.getByTitle('Zoom out').click()
    }
  }
  await expect(tick).toBeVisible()
}

test('logs in and shows the dashboard', async ({ page }) => {
  await login(page)
  const recentJobsCard = page.locator('.q-card').filter({
    hasText: 'Recent Jobs',
  })

  await expect(page.getByText('Running Jobs', { exact: true })).toBeVisible()
  await expect(page.getByTitle('Combined view of actual job')).toBeVisible()
  await expect(page.getByTestId('combined-job-schedule-timeline')).toBeVisible()
  await expect(page.getByTestId('combined-timeline-job-bar').first()).toBeVisible()
  await expectScheduleTick(page)
  await expect(recentJobsCard).toBeVisible()
  await expect(recentJobsCard).not.toContainText('No data available')
  await expect(recentJobsCard).toContainText('backup-bareos-fd')

  await page.getByRole('tab', { name: 'Job & Schedule Timeline' }).click()
  await expect(page).toHaveURL(/#\/dashboard\/job-schedule-timeline$/)
  await expect(page.getByTestId('combined-job-schedule-timeline')).toBeVisible()
  await expect(page.getByTestId('combined-timeline-job-bar').first()).toBeVisible()
  await expectScheduleTick(page)
  await page.getByTitle('Pan to the past').click()
  await page.waitForTimeout(500)
  await expect(page.getByTestId('combined-job-schedule-timeline')).not.toContainText(
    /Authentication failed/i
  )
})

test('advertises Bareos services unless the director is a subscription build', async ({ page }) => {
  // nothing is known about the build before the first login
  await page.goto('/')
  await expect(page.getByTestId('login-commercial-offering')).toBeVisible()
  await expect(page.getByTestId('login-commercial-offering').getByTestId('commercial-offering-evaluation'))
    .toHaveAttribute('href', 'https://www.bareos.com/try/')
  const sash = page.getByTestId('unsupported-build-sash')
  await expect(sash).toBeVisible()
  await expect(sash).toHaveCSS('background-color', 'rgb(255, 179, 0)')

  await login(page)
  await page.goto('/#/director')
  const binaryInfo = page.getByTestId('director-binary-info').first()
  await expect(binaryInfo).toBeVisible()
  const isSubscription = /subscription/i.test(await binaryInfo.innerText())

  const statusbarOffering = page.getByTestId('statusbar-commercial-offering')
  if (isSubscription) {
    await expect(statusbarOffering).toHaveCount(0)
    await expect(page.getByTestId('unsupported-build-ribbon')).toHaveCount(0)
    return
  }
  await expect(statusbarOffering).toBeVisible()
  await expect(statusbarOffering).toHaveAttribute('href', 'https://www.bareos.com/try/')
  await expect(statusbarOffering).toHaveCSS('color', 'rgb(255, 202, 40)')

  // markers that this is not an official subscription build; the sash is
  // only shown on the login page
  await expect(page.getByTestId('unsupported-build-sash')).toHaveCount(0)
  await expect(page.locator('body')).toHaveClass(/bareos-unsupported-build/)
  await expect(page.locator('header.bareos-toolbar')).toHaveCSS(
    'background-color',
    'rgb(0, 117, 190)',
  )
  await expect(page.locator('footer.bareos-statusbar')).toHaveCSS(
    'background-color',
    'rgb(0, 86, 139)',
  )
  await expect(page).toHaveTitle('Director - Bareos (unsupported build)')
  const ribbon = page.getByTestId('unsupported-build-ribbon')
  await expect(ribbon).toBeVisible()
  await expect(ribbon).toHaveCSS('background-color', 'rgb(255, 179, 0)')
  await expect(ribbon.getByRole('link'))
    .toHaveAttribute('href', 'https://www.bareos.com/try/')
  await expect(ribbon).toContainText(/not for production use/i)
  await page.getByTestId('unsupported-build-ribbon-close').click()
  await expect(ribbon).toHaveCount(0)
  const toast = page.locator('.unsupported-build-toast')
  await expect(toast).toBeVisible()
  await expect(toast.getByRole('button', { name: 'Try for free' })).toBeVisible()
  await expect(toast.getByRole('button', { name: 'Buy a subscription' })).toBeVisible()
  await page.reload()
  await expect(page.getByTestId('director-binary-info').first()).toBeVisible()
  await expect(ribbon).toHaveCount(0)
  await expect(toast).toHaveCount(0)

  await page.getByTestId('account-menu').click()
  await expect(page.getByTestId('menu-offering-evaluation'))
    .toHaveAttribute('href', 'https://www.bareos.com/try/')
  await expect(page.getByTestId('menu-offering-subscription')).toBeVisible()
  await page.keyboard.press('Escape')

  await page.getByRole('tab', { name: 'Subscription' }).click()
  await expect(page.getByTestId('commercial-offering-card')).toBeVisible()
  await expect(page.getByTestId('commercial-offering-evaluation'))
    .toHaveAttribute('href', 'https://www.bareos.com/try/')
  await expect(page.getByTestId('commercial-offering-expert-circle'))
    .toHaveAttribute('href', 'https://www.bareos.com/meet/')
})

test('browser regression: retry updates the remaining director failure', async ({ page }) => {
  let failures = 0
  await page.route('**/api/**', route => {
    const url = route.request().url()
    if (url.endsWith('/directors')) {
      return route.fulfill({ json: { type: 'director_list', directors: ['director-a', 'director-b'] } })
    }
    if (url.endsWith('/session/directors/director-a/login')) {
      return route.fulfill({ json: { authenticatedDirectors: [{ director: 'director-a', username: 'admin' }] } })
    }
    if (url.endsWith('/session/directors/director-b/login')) {
      failures++
      return route.fulfill({
        status: failures === 1 ? 401 : 503,
        json: { message: failures === 1 ? 'Authentication failed' : 'Connection refused' },
      })
    }
    return route.fulfill({ status: 401 })
  })
  await page.routeWebSocket(/\/ws/, socket => {
    socket.onMessage(message => {
      const request = JSON.parse(message)
      if (request.type === 'session') socket.send(JSON.stringify({ type: 'auth_ok' }))
      if (request.type === 'command') socket.send(JSON.stringify({ type: 'response', id: request.id, data: {} }))
    })
  })
  await page.goto('/#/login')
  await expect(page.getByTestId('login-target-directors')).toContainText('director-b')
  await page.getByTestId('login-username').fill('admin')
  await page.getByTestId('login-password').fill('secret')
  await page.getByTestId('login-submit').click()
  const directorB = page.locator('.director-login-status').filter({ hasText: 'director-b' })
  await expect(directorB).toContainText('Authentication failed')
  await expect(page.getByRole('button', { name: 'Skip failed directors' })).toBeVisible()
  await page.getByTestId('login-password').fill('secret')
  await page.getByTestId('login-submit').click()
  await expect(directorB).toContainText('Connection refused')
  await page.getByRole('button', { name: 'Skip failed directors' }).click()
  await expect(page).toHaveURL(/#\/dashboard/)
})

test('loads translations from the Vue JSON catalogs', async ({ page }) => {
  await page.goto('/')
  await page.locator('.login-language-select').click()
  await page.getByRole('option', { name: /German/ }).click()

  await expect(page.getByTestId('login-submit')).toContainText('Anmelden')
  await expect(page.getByTestId('login-username')).toHaveAttribute('aria-label', 'Benutzername')
  await expect(page.getByTestId('login-password')).toHaveAttribute('aria-label', 'Passwort')
})

// Official releases run these tests with the build definitions of the
// release, but PR builds are never subscription builds. Pretend to be one
// by rewriting binary_info in the director replies.
async function reportSubscriptionBuild(page) {
  await page.routeWebSocket(/.*/, (ws) => {
    const server = ws.connectToServer()
    server.onMessage((message) => {
      try {
        const parsed = JSON.parse(message)
        if (parsed?.type === 'response' && parsed.data?.header?.binary_info !== undefined) {
          parsed.data.header.binary_info = 'Bareos subscription'
          ws.send(JSON.stringify(parsed))
          return
        }
      } catch {
        // not JSON: forward unchanged
      }
      ws.send(message)
    })
  })
}

test('hides the service advertisement for subscription builds', async ({ page }) => {
  await reportSubscriptionBuild(page)
  await login(page)
  await page.goto('/#/director')
  await expect(page.getByTestId('director-binary-info').first())
    .toHaveText(/Bareos subscription/)

  await expect(page.getByTestId('statusbar-commercial-offering')).toHaveCount(0)
  await expect(page.getByTestId('unsupported-build-ribbon')).toHaveCount(0)
  await expect(page.getByTestId('unsupported-build-sash')).toHaveCount(0)
  await expect(page.locator('body')).not.toHaveClass(/bareos-unsupported-build/)
  await expect(page).toHaveTitle('Director - Bareos')
  await page.getByTestId('account-menu').click()
  await expect(page.getByTestId('menu-offering-evaluation')).toHaveCount(0)
  await expect(page.getByTestId('menu-offering-support')).toHaveCount(0)
  await page.keyboard.press('Escape')

  await page.getByRole('tab', { name: 'Subscription' }).click()
  await expect(page.getByRole('link', { name: 'Get Official Support' })).toBeVisible()
  await expect(page.getByTestId('commercial-offering-card')).toHaveCount(0)

  // the login page decides from the remembered build kind
  expect(await page.evaluate(() => localStorage.getItem('bareos-webui.buildKind')))
    .toBe('subscription')
  await page.getByTestId('account-menu').click()
  await page.locator('.q-menu').getByText('Logout', { exact: true }).click()
  await expect(page.getByTestId('login-form')).toBeVisible()
  await expect(page.getByTestId('login-commercial-offering')).toHaveCount(0)
  await expect(page.getByTestId('unsupported-build-sash')).toHaveCount(0)
})

test('shows a login error for invalid credentials', async ({ page }) => {
  await login(page, {
    loginPassword: `${password}-invalid`,
    shouldSucceed: false,
  })
  await expect(page.locator('[data-testid="login-error"]')).toContainText(
    /Authentication failed|Connection error|Could not log in to any configured director/i
  )
})

test('shows all configured directors in multi-director login mode', async ({
  page,
}) => {
  test.skip(
    !isMultiDirectorProfile,
    'multi-director login checks only apply to multi-director smoke'
  )

  await page.goto('/')
  await expect(page.locator('[data-testid="login-form"]')).toBeVisible()

  const targetDirectors = page.locator('[data-testid="login-target-directors"]')
  await expect(targetDirectors).toBeVisible()
  await expect(targetDirectors).toContainText('bareos-dir')
  await expect(targetDirectors).toContainText('bareos-dir-2')
  await expect(targetDirectors).toContainText('Not yet logged in')
  await expect(page.locator('[data-testid="login-director"]')).toHaveCount(0)

  await page.getByLabel('Username').fill(username)
  await page.getByLabel('Password').fill(password)
  await page.getByRole('button', { name: 'Login' }).click()
  await page.waitForURL(/#\/dashboard(\/|$)/)
  await expectConnected(page)
})

test('reconnects the console session on demand', async ({ page }) => {
  await login(page)

  const consoleOutput = await openConsole(page)

  await page.getByTitle('Reconnect').click()
  await expect(consoleOutput).toContainText('Console disconnected.')
  await expect(consoleOutput).toContainText('Connected to bareos-dir')
})

test('reconnects the console after typing exit', async ({ page }) => {
  await login(page)

  const consoleOutput = await openConsole(page)

  await consoleOutput.click()
  await page.keyboard.type('exit')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('Console disconnected.')

  await page.getByTitle('Reconnect').click()
  await expect(consoleOutput).toContainText('Connected to bareos-dir')

  await consoleOutput.click()
  await page.keyboard.type('status director')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('status director')
})

test('opens jobs and job details through the real director connection', async ({
  page,
}) => {
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)

  await page.getByLabel('Search in results').fill('backup-bareos-fd')
  // The jobs table is virtual-scrolled, so tbody also contains Quasar's
  // empty padding rows — pick the first row that actually holds a match.
  const firstRow = page
    .locator('tbody tr')
    .filter({ hasText: 'backup-bareos-fd' })
    .first()
  await expect(firstRow).toBeVisible()

  const jobId = (await firstRow.locator('a.text-primary').first().textContent())?.trim()
  await firstRow.locator('a.text-primary').first().click()

  await expect(page).toHaveURL(/#\/jobs\/\d+/)
  await expect(page.getByText(`Job #${jobId}`, { exact: false })).toBeVisible()
  await expect(page.getByText('Job Summary', { exact: true })).toBeVisible()
  await expect(page.getByText('Job Log', { exact: true })).toBeVisible()
})

test('sorts the newest capped jobs independently of display direction', async ({ page }) => {
  test.skip(isMultiDirectorProfile, 'Exercises the single-Director listing path')
  const rows = Array.from({ length: 1001 }, (_, index) => ({
    jobid: String(index + 1),
    name: `SortJob-${index + 1}`,
    jobstatus: 'T',
    jobtype: 'B',
    duration: `00:${String(Math.floor((index + 1) / 60)).padStart(2, '0')}:${String((index + 1) % 60).padStart(2, '0')}`,
  }))
  await page.routeWebSocket(/\/ws/, (socket) => {
    const server = socket.connectToServer()
    socket.onMessage((message) => {
      const request = JSON.parse(message)
      const command = request.command ?? ''
      if (command.startsWith('llist jobs ') && !command.includes(' last ')) {
        const limit = Number(command.match(/\blimit=(\d+)/)?.[1] ?? rows.length)
        const offset = Number(command.match(/\boffset=(\d+)/)?.[1] ?? 0)
        const ordered = command.includes(' reverse ') ? [...rows].reverse() : rows
        socket.send(JSON.stringify({
          type: 'response',
          id: request.id,
          data: {
            jobs: command.startsWith('llist jobs count')
              ? [{ count: '1001' }]
              : ordered.slice(offset, offset + limit),
          },
        }))
      } else {
        server.send(message)
      }
    })
  })
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  const firstId = page.locator('tbody tr a.text-primary').first()
  await expect(firstId).toHaveText('1001')
  const idHeader = page.getByRole('columnheader', { name: /^ID\b/ })
  await idHeader.click()
  await expect(firstId).toHaveText('1')
  await idHeader.click()
  await expect(firstId).toHaveText('1001')

  const durationHeader = page.getByRole('columnheader', { name: /^Duration\b/ })
  await durationHeader.click()
  await expect(firstId).toHaveText('2')
  await expect(page.locator('.q-banner.bg-warning')).toContainText('Fetched 1,000 jobs.')
  await durationHeader.click()
  await expect(firstId).toHaveText('1001')
})

for (const fixture of [
  { name: 'listed restore', listed: 'both', type: 'R', error: 'cannot be rerun' },
  { name: 'unlisted restore', listed: 'none', type: 'R', error: 'cannot be rerun' },
  { name: 'listed backup', listed: 'both', type: 'B' },
  { name: 'unlisted backup', listed: 'none', type: 'B' },
  { name: 'listed duplicate', listed: 'both', multi: true, error: 'exists on multiple selected directors' },
  { name: 'unlisted duplicate', listed: 'none', multi: true, error: 'exists on multiple selected directors' },
  { name: 'partly listed duplicate', listed: 'first', multi: true, error: 'exists on multiple selected directors' },
  { name: 'partly listed with failed lookup', listed: 'first', multi: true, denied: true, error: 'lookup denied' },
]) {
  test(`validates rerun by ID for ${fixture.name}`, async ({ page }) => {
    test.skip(isReadonlyProfile, 'Rerun requires write access')
    test.skip(fixture.multi && !isMultiDirectorProfile, 'Requires multiple Directors')
    const reruns = []
    await page.routeWebSocket(/\/ws/, (socket) => {
      const server = socket.connectToServer()
      let currentDirector
      socket.onMessage((message) => {
        const request = JSON.parse(message)
        if (request.type === 'session') {
          currentDirector = request.director
        }
        const command = request.command ?? ''
        const target = {
          jobid: '424242',
          name: 'Rerun-fixture',
          jobtype: fixture.multi
            ? (currentDirector === 'bareos-dir-2' ? 'R' : 'B')
            : fixture.type,
          jobstatus: 'T',
        }
        let data
        if (command.startsWith('rerun ')) {
          reruns.push({ director: currentDirector, command })
          data = { run: { jobid: '900000' } }
        } else if (command === 'llist jobid=424242') {
          if (fixture.denied && currentDirector === 'bareos-dir-2') {
            socket.send(JSON.stringify({ type: 'error', id: request.id, message: 'lookup denied' }))
            return
          }
          data = { jobs: [target] }
        } else if (command.startsWith('llist jobs ') && !command.includes(' last ')) {
          const targetListed = fixture.listed === 'both'
            || (fixture.listed === 'first' && currentDirector === 'bareos-dir')
          data = {
            jobs: command.startsWith('llist jobs count')
              ? [{ count: '1' }]
              : [targetListed ? target : {
                jobid: '424241', name: 'Fixture-other', jobtype: 'B', jobstatus: 'T',
              }],
          }
        }
        if (data) {
          socket.send(JSON.stringify({ type: 'response', id: request.id, data }))
        } else {
          server.send(message)
        }
      })
    })
    await login(page)
    if (isMultiDirectorProfile) {
      await page.getByTestId('director-scope-control').click()
      const scopeMenu = page.getByTestId('director-scope-menu')
      await scopeMenu.getByRole('button', { name: 'Select all' }).click()
      if (!fixture.multi) {
        await scopeMenu.getByText('bareos-dir-2', { exact: true }).first().click()
      }
      await page.keyboard.press('Escape')
    }
    await openNav(page, 'nav-jobs', /#\/jobs/)
    await expect(page.locator('tbody tr a.text-primary').first()).toBeVisible()
    if (fixture.multi) {
      await expect(page.getByRole('columnheader', { name: /^Director\b/ })).toBeVisible()
      await expect(page.locator('tbody tr').filter({ has: page.locator('a.text-primary') })).toHaveCount(2)
    }
    await page.getByRole('button', { name: /Rerun by Job ID/ }).click()
    const dialog = page.getByRole('dialog')
    await dialog.getByLabel('Job ID *', { exact: true }).fill('424242')
    await dialog.getByRole('button', { name: 'Rerun', exact: true }).click()
    if (fixture.error) {
      await expect(page.locator('.q-notification__message')).toContainText(fixture.error)
      expect(reruns).toEqual([])
      await expect(dialog).toBeVisible()
    } else {
      await expect(page.locator('.q-notification__message')).toContainText('Job restarted as ID 900000.')
      expect(reruns).toEqual([{ director: 'bareos-dir', command: 'rerun jobid=424242 yes' }])
      await expect(dialog).not.toBeVisible()
    }
  })
}

for (const logFocus of ['', 'ok']) {
  for (const [direction, opposite, first, wrapped] of [
    ['Next issue', 'Previous issue', 'Warning: first issue', 'Fatal error: last issue'],
    ['Previous issue', 'Next issue', 'Fatal error: last issue', 'Warning: first issue'],
  ]) {
    test(`navigates ${direction} from ${logFocus || 'initial'} log focus`, async ({ page }) => {
      await page.routeWebSocket(/\/ws/, (socket) => {
        const server = socket.connectToServer()
        socket.onMessage((message) => {
          const request = JSON.parse(message)
          if (request.command?.startsWith('list joblog jobid=')) {
            socket.send(JSON.stringify({
              type: 'response',
              id: request.id,
              data: {
                joblog: [
                  { logtext: 'Warning: first issue' },
                  { logtext: 'Error: middle issue' },
                  { logtext: 'Fatal error: last issue' },
                  { logtext: 'Termination: Backup OK' },
                ],
              },
            }))
          } else {
            server.send(message)
          }
        })
      })

      await login(page)
      await openNav(page, 'nav-jobs', /#\/jobs/)
      const jobLink = page.locator('tbody tr a.text-primary').first()
      await expect(jobLink).toBeVisible()
      const jobId = (await jobLink.textContent())?.trim()
      expect(jobId).toMatch(/^\d+$/)
      await page.goto(`/#/jobs/${jobId}${logFocus ? `?logFocus=${logFocus}` : ''}`)
      await expect(page.locator('.log-line')).toHaveCount(4)
      const focused = page.locator('.log-line--focused')
      if (logFocus) {
        await expect(focused).toHaveText('Termination: Backup OK')
      } else {
        await expect(focused).toHaveCount(0)
      }

      await page.getByRole('button', { name: direction, exact: true }).click()
      await expect(focused).toHaveText(first)
      await page.getByRole('button', { name: opposite, exact: true }).click()
      await expect(focused).toHaveText(wrapped)
      await page.getByRole('button', { name: direction, exact: true }).click()
      await expect(focused).toHaveText(first)
      await page.getByRole('button', { name: direction, exact: true }).click()
      await expect(focused).toHaveText('Error: middle issue')
    })
  }
}

test('keeps Comment non-sortable without changing the active job order', async ({ page }) => {
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  const firstId = page.locator('tbody tr a.text-primary').first()
  await expect(firstId).toBeVisible()
  const originalFirstId = (await firstId.textContent()).trim()
  await page.getByRole('button', { name: 'Choose columns' }).click()
  await page.locator('.q-menu').getByText('Comment', { exact: true }).click()
  await page.keyboard.press('Escape')

  const commentHeader = page.getByRole('columnheader', { name: /^Comment\b/ })
  await expect(commentHeader.locator('.q-table__sort-icon')).toHaveCount(0)
  await commentHeader.click()
  await commentHeader.click()
  await expect(firstId).toHaveText(originalFirstId)
  await expect(page.getByRole('columnheader', { name: /^ID\b/ })).toHaveClass(/sort-desc/)
})

async function checkCommentEditorKeyboard(page) {
  await page.getByRole('button', { name: 'Choose columns' }).click()
  await page.locator('.q-menu').getByText('Comment', { exact: true }).click()
  await page.keyboard.press('Escape')

  const edit = page.locator('[data-testid^="job-comment-edit-"]').first()
  await expect(edit).toHaveCSS('opacity', '0')
  await edit.locator('..').hover()
  await expect(edit).toHaveCSS('opacity', '1')
  await page.mouse.move(0, 0)
  await expect(edit).toHaveCSS('opacity', '0')
  const start = page.getByRole('button', { name: 'Choose columns' })
  await start.focus()
  await expect(start).toBeFocused()
  for (let index = 0; index < 200 && !(await edit.evaluate(el => el === document.activeElement)); index++) {
    await page.keyboard.press('Tab')
  }
  await expect(edit).toBeFocused()
  await expect(edit).toHaveCSS('opacity', '1')
  await page.keyboard.press('Enter')
  await expect(page.getByRole('dialog')).toBeVisible()
}

test('shows comment editing on mouse hover and keyboard focus', async ({ page }) => {
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await checkCommentEditorKeyboard(page)
})

test.describe('touch comment editing', () => {
  test.use({ hasTouch: true })

  test('shows the edit button without hover', async ({ page }) => {
    await login(page)
    await openNav(page, 'nav-jobs', /#\/jobs/)
    expect(await page.evaluate(() => matchMedia('(hover: none)').matches)).toBe(true)
    await page.getByRole('button', { name: 'Choose columns' }).tap()
    await page.locator('.q-menu').getByText('Comment', { exact: true }).tap()
    await page.keyboard.press('Escape')

    const edit = page.locator('[data-testid^="job-comment-edit-"]').first()
    await expect(edit).toHaveCSS('opacity', '1')
    await edit.tap()
    await expect(page.getByRole('dialog')).toBeVisible()
  })
})

test('loads the restore workflow selections', async ({ page }) => {
  test.setTimeout(90000)

  await login(page)
  await openNav(page, 'nav-restore', /#\/restore/)

  await waitForQSelectReady(page, 'restore-source-client')
  await selectFirstQOption(page, 'restore-source-client', {
    hasText: /(^| \/ )bareos-fd$/,
    selected: () => waitForQSelectReady(page, 'restore-backup-job'),
  })
  await expect(page.locator('[data-testid="restore-backup-job"]')).toBeVisible()
  await waitForQSelectReady(page, 'restore-backup-job')
  await selectFirstQOption(page, 'restore-backup-job', { optionTimeoutMs: 8000 })
  await expect(page.locator('[data-testid="restore-target-client"]')).toBeVisible()
  await expect(page.locator('[data-testid="restore-job"]')).toBeVisible()
  await expect(page.getByText('Browse Files', { exact: true })).toBeVisible()
  await expect(page.locator('[data-testid="restore-submit"]')).toBeDisabled()
})

test('restores a real file from a backup', async ({ page }) => {
  test.skip(profile !== 'admin', 'Restore requires the admin profile')
  test.setTimeout(180000)

  const sourceFile = process.env.BAREOS_WEBUI_RESTORE_TEST_FILE
  const destination = process.env.BAREOS_WEBUI_RESTORE_TEST_DESTINATION
  if (!sourceFile || !destination) {
    throw new Error(
      'Restore test fixture is required: set '
      + 'BAREOS_WEBUI_RESTORE_TEST_FILE and '
      + 'BAREOS_WEBUI_RESTORE_TEST_DESTINATION'
    )
  }

  const restoredFile = join(destination, relative('/', sourceFile))
  const expectedContents = 'webui-vue real restore test\n'
  await rm(destination, { recursive: true, force: true })
  await mkdir(destination, { recursive: true })

  await login(page)
  await openNav(page, 'nav-restore', /#\/restore/)

  await waitForQSelectReady(page, 'restore-source-client')
  await selectFirstQOption(page, 'restore-source-client', {
    hasText: /(^| \/ )bareos-fd$/,
    selected: () => waitForQSelectReady(page, 'restore-backup-job'),
  })
  await waitForQSelectReady(page, 'restore-backup-job')
  await selectFirstQOption(page, 'restore-backup-job', {
    hasText: /backup-bareos-fd(?!-)/,
    optionTimeoutMs: 8000,
  })

  const browser = page.getByTestId('restore-browser')
  await expect(browser).toBeVisible({ timeout: 30000 })
  const rootDirectory = browser.getByText('/', { exact: true })
  if (await rootDirectory.count()) {
    await rootDirectory.click()
  }
  for (const directory of relative('/', dirname(sourceFile)).split(sep).filter(Boolean)) {
    const directoryEntry = browser.getByText(`${directory}/`, { exact: true })
    await expect(directoryEntry).toBeVisible()
    await directoryEntry.click()
  }

  const fileName = basename(sourceFile)
  const fileRow = browser.getByText(fileName, { exact: true })
    .locator('xpath=ancestor::tr')
  await expect(fileRow).toBeVisible()
  await fileRow.locator('.q-checkbox').click()

  await page.getByRole('textbox', { name: 'Restore to (Where)' }).fill(destination)
  await page.getByTestId('restore-submit').click()
  await page.getByRole('dialog').getByRole('button', { name: 'OK' }).click()
  await expect(page).toHaveURL(/#\/jobs/)

  await expect.poll(async () => {
    try {
      return await readFile(restoredFile, 'utf8')
    } catch {
      return null
    }
  }, { timeout: 120000 }).toBe(expectedContents)
})

test('navigates through client, pool, and volume detail pages', async ({
  page,
}) => {
  await login(page)

  await openNav(page, 'nav-clients', /#\/clients/)
  const firstClient = page.locator('tbody tr a.text-primary').first()
  const clientName = (await firstClient.textContent())?.trim()
  await firstClient.click()

  await expect(page).toHaveURL(/#\/clients\/.+/)
  await expect(page.locator('.q-page .text-h5').first()).toContainText(
    clientName ?? ''
  )
  await expect(page.getByText('Client Details', { exact: true })).toBeVisible()
  await page.goBack()

  await openNav(page, 'nav-storages', /#\/storages/)
  await expect(page.locator('.q-page').getByText('Storages', { exact: true }).first()).toBeVisible()
  await expect(page.getByTestId('storages-detail-hint')).toBeVisible()

  const firstStorageRow = page.locator('tbody tr').first()
  const storageName = (
    await firstStorageRow.getByTestId('storages-row-name').textContent()
  )?.trim()
  await firstStorageRow.click()

  await expect(page).toHaveURL(/#\/storages\?.*storage=/)
  await expect(page.getByTestId('storages-detail')).toBeVisible()
  await expect(page.getByTestId('storages-detail-name')).toContainText(storageName ?? '')

  // The selection lives in the URL, so a reload restores the detail panel.
  await page.reload()
  await expect(page.getByTestId('storages-detail-name')).toContainText(storageName ?? '')

  await page.getByTestId('storages-detail-close').click()
  await expect(page.getByTestId('storages-detail-hint')).toBeVisible()

  await openNav(page, 'nav-pools', /#\/pools/)
  const firstPool = page.locator('tbody tr a.text-primary').first()
  const poolName = (await firstPool.textContent())?.trim()
  await firstPool.click()

  await expect(page).toHaveURL(/#\/pools\/.+/)
  await expect(page.locator('.q-page .text-h5').first()).toContainText(
    poolName ?? ''
  )
  await expect(page.getByText('Volumes', { exact: true })).toBeVisible()

  await page.goBack()
  await page.getByRole('tab', { name: 'Volumes' }).click()
  const firstVolume = page.locator('tbody tr a.text-primary').first()
  await firstVolume.click()
  await expect(page).toHaveURL(/#\/volumes\/.+/)
  await expect(page.getByText('Volume Properties', { exact: true })).toBeVisible()
})

test('covers schedules and director tabs through the director connection', async ({
  page,
}) => {
  await login(page)

  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect(page.getByText('Scheduler Jobs', { exact: true })).toBeVisible()
  await expect(page.getByText('Scheduler Preview', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Definitions' }).click()
  await expect(
    page.locator('.q-tab-panel:visible').getByText('Schedules', { exact: true })
  ).toBeVisible()

  await openNav(page, 'nav-director', /#\/director/)
  await expect(page.getByText('Director Info', { exact: true })).toBeVisible()
  await expect(page.getByText('Scheduled Jobs', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Messages' }).click()
  await expect(page.getByText('Director Messages', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Catalog' }).click()
  await expect(page.getByText('Jobs With No Data', { exact: true })).toBeVisible()
  await expect(page.getByText('Prune Expired Records', { exact: true })).toBeVisible()
})

test('shows readonly ACL restrictions for the restricted profile', async ({
  page,
}) => {
  test.skip(!isReadonlyProfile, 'restricted ACL checks only apply to readonly smoke')

  await login(page)
  await page.goto('/#/acls')
  await expect(page.getByText('User Command ACL', { exact: true })).toBeVisible()

  const deniedChip = page.locator('.q-chip').filter({ hasText: /denied/i }).first()
  await expect(deniedChip).toBeVisible()
  await expect(deniedChip).toContainText(/[1-9]\d*\s+denied/i)
})

test('keeps the console session when navigating away and back', async ({
  page,
}) => {
  await login(page)

  const consoleOutput = await openConsole(page)

  await page.getByText('status director', { exact: true }).click()
  await expect(consoleOutput).toContainText('Terminated Jobs:')

  await page.goto('/#/dashboard')
  await page.goto('/#/console-popup')
  // xterm.js only keeps the visible viewport rows in the DOM, so the
  // echoed command line has scrolled out by the time the (much longer)
  // replayed response is redrawn — assert on the replayed output itself,
  // which is what proves the session survived the navigation.
  await expect(consoleOutput).toContainText('Terminated Jobs:')
})

test('restores the proxy-backed login after a page reload', async ({ page }) => {
  await login(page)

  // Reload clears the SPA state; /api/session must restore the proxy-backed
  // login from the session cookie so users stay signed in.
  await page.reload()
  await page.waitForURL(/#\/dashboard(\/|$)/)
  await expectConnected(page)
})

test('opens the console and runs a raw command through the proxied director connection', async ({
  page,
}) => {
  await login(page)

  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('status director')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('Terminated Jobs:')
})

test('redraws a completed console command before it is sent', async ({ page }) => {
  await login(page)

  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('list jobs client=bareos-')
  await page.keyboard.press('Tab')
  await expect(consoleOutput).toContainText('list jobs client=bareos-fd')
})

test('redraws the pending console command after a terminal resize', async ({ page }) => {
  await login(page)

  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  const command = `list jobs client=${'x'.repeat(60)}`
  await page.keyboard.type(command)
  await expect(consoleOutput).toContainText(command)

  await page.setViewportSize({ width: 500, height: 800 })
  await expect(consoleOutput).toContainText(command)
})

test('displays completion candidates without executing terminal controls', async ({ page }) => {
  const submitted = []
  await page.routeWebSocket(/\/ws/, socket => {
    const server = socket.connectToServer()
    socket.onMessage(message => {
      const request = JSON.parse(message)
      if (request.type === 'command' && request.command === '.client') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'bareos-\x1b[2Jname', prompt: 'main',
        }))
      } else if (request.type === 'command' && request.command.startsWith('list jobs client=')) {
        submitted.push(request.command)
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'command received', prompt: 'main',
        }))
      } else {
        server.send(message)
      }
    })
  })

  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('list jobs client=bareos-')
  await page.keyboard.press('Tab')
  await expect(consoleOutput).toContainText('list jobs client=bareos-\\x1b[2Jname')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('command received')
  await expect.poll(() => submitted).toEqual(['list jobs client=bareos-\x1b[2Jname'])
  await expect(consoleOutput).toContainText('list jobs client=bareos-\\x1b[2Jname')
})

test('preserves console output split across unterminated chunks', async ({ page }) => {
  let releaseOutput
  const outputReady = new Promise(resolve => { releaseOutput = resolve })
  await page.routeWebSocket(/\/ws/, socket => {
    const server = socket.connectToServer()
    socket.onMessage(async message => {
      const request = JSON.parse(message)
      if (request.type === 'command' && request.command === 'webui-output-test') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id,
          text: `${'scroll-line\n'.repeat(50)}${'wide'.repeat(60)}first-\x1B[31m`,
          prompt: 'more',
        }))
        await outputReady
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'second-\x1B[0m', prompt: 'more',
        }))
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'final', prompt: 'main',
        }))
      } else if (request.type === 'command' && request.command === 'webui-empty-test') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'unfinished', prompt: 'more',
        }))
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: '', prompt: 'main',
        }))
      } else if (request.type === 'command' && request.command === 'webui-single-test') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'single-result', prompt: 'main',
        }))
      } else {
        server.send(message)
      }
    })
  })

  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('webui-output-test')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('first-')
  await page.keyboard.type('next-command')
  await expect(consoleOutput).toContainText('next-command')
  await page.keyboard.press('ArrowLeft')
  await page.keyboard.press('ArrowLeft')
  releaseOutput()
  await expect(consoleOutput).toContainText('first-second-final')
  await expect(consoleOutput).toContainText('next-command')
  await page.keyboard.type('Z')
  await expect(consoleOutput).toContainText('next-commaZnd')
  await page.keyboard.press('End')
  await page.keyboard.press('Control+u')
  await page.keyboard.type('webui-empty-test')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('unfinished')
  await page.keyboard.type('webui-single-test')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('single-result')

  await page.goto('/#/dashboard')
  await openConsole(page)
  await expect(consoleOutput).toContainText('first-second-final')
  await expect(consoleOutput).toContainText('unfinished')
  await expect(consoleOutput).toContainText('single-result')
})

test('positions the console cursor at wide and combining characters', async ({ page }) => {
  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  const textarea = page.locator('.xterm-helper-textarea')
  const cursorX = () => textarea.evaluate(element => element.getBoundingClientRect().x)
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'AB')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('AB')
  const asciiEnd = await cursorX()
  await page.keyboard.press('ArrowLeft')
  await expect.poll(cursorX).toBeLessThan(asciiEnd)
  const columnWidth = asciiEnd - await cursorX()

  await page.keyboard.press('End')
  await page.keyboard.press('Control+u')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'A界')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('A界')
  const wideEnd = await cursorX()
  await page.keyboard.press('ArrowLeft')
  await expect.poll(async () => wideEnd - await cursorX()).toBeCloseTo(2 * columnWidth, 0)

  await page.keyboard.press('End')
  await page.keyboard.press('Control+u')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'Ae\u0301B')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('Ae\u0301B')
  const combiningEnd = await cursorX()
  await page.keyboard.press('ArrowLeft')
  await page.keyboard.press('ArrowLeft')
  await expect.poll(async () => combiningEnd - await cursorX()).toBeCloseTo(columnWidth, 0)
})

test('keeps the console cursor on the input after scrollback is trimmed', async ({ page }) => {
  await page.routeWebSocket(/\/ws/, socket => {
    const server = socket.connectToServer()
    socket.onMessage(message => {
      const request = JSON.parse(message)
      if (request.type === 'command' && request.command === 'webui-fill-scrollback') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id,
          text: `${'scroll-line\n'.repeat(5100)}scrollback-filled\n`, prompt: 'main',
        }))
      } else {
        server.send(message)
      }
    })
  })

  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('webui-fill-scrollback')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('scrollback-filled')
  const textarea = page.locator('.xterm-helper-textarea')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', `editstart${'x'.repeat(300)}`)
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await page.keyboard.press('Home')
  const inputRow = page.locator('.xterm-rows > div').filter({ hasText: 'editstart' }).last()
  await expect(inputRow).toBeVisible()
  await expect.poll(async () => {
    const cursor = await textarea.boundingBox()
    const row = await inputRow.boundingBox()
    return Math.abs(cursor.y - row.y)
  }).toBeLessThan(3)
})

test('redraws wrapped console input without leaving stale rows', async ({ page }) => {
  let releaseOutput
  const outputReady = new Promise(resolve => { releaseOutput = resolve })
  await page.routeWebSocket(/\/ws/, socket => {
    const server = socket.connectToServer()
    socket.onMessage(async message => {
      const request = JSON.parse(message)
      if (request.type === 'command' && request.command === 'webui-wrapped-output') {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'stream-start', prompt: 'more',
        }))
        await outputReady
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: '-stream-end', prompt: 'main',
        }))
      } else {
        server.send(message)
      }
    })
  })

  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('webui-wrapped-output')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('stream-start')
  const textarea = page.locator('.xterm-helper-textarea')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData(
      'text/plain',
      `longinput${'x'.repeat(100)}middleinput${'y'.repeat(100)}endinput`
    )
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await page.keyboard.press('Home')
  await page.keyboard.press('End')
  const inputRows = page.locator('.xterm-rows > div')
  const expectInputMarkersOnce = async () => {
    for (const marker of ['longinput', 'middleinput', 'endinput']) {
      await expect(inputRows.filter({ hasText: marker })).toHaveCount(1)
    }
  }
  await expectInputMarkersOnce()
  releaseOutput()
  await expect(consoleOutput).toContainText('stream-start-stream-end')
  await expectInputMarkersOnce()
})

test('passes Meta shortcuts through while keeping console editing keys', async ({ page }) => {
  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('abc')
  const textarea = page.locator('.xterm-helper-textarea')
  const shortcutAllowed = await textarea.evaluate(element => element.dispatchEvent(
    new KeyboardEvent('keydown', { key: 'v', metaKey: true, bubbles: true, cancelable: true })
  ))
  expect(shortcutAllowed).toBe(true)
  await expect(consoleOutput).toContainText('abc')
  await expect(consoleOutput).not.toContainText('abcv')
  await page.keyboard.press('Meta+ArrowLeft')
  await page.keyboard.press('Meta+Backspace')
  await expect(consoleOutput).toContainText('ac')
})

test('pastes into the console editor and interactive selection', async ({ page }) => {
  const selectionInputs = []
  await page.routeWebSocket(/\/ws/, socket => {
    const server = socket.connectToServer()
    let selecting = false
    socket.onMessage(message => {
      const request = JSON.parse(message)
      if (request.type === 'command' && request.command === 'webui-paste-selection') {
        selecting = true
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'Enter search text:', prompt: 'select',
        }))
      } else if (selecting && request.type === 'command' && request.command.startsWith('resize:')) {
        socket.send(JSON.stringify({
          type: 'raw_response', id: request.id, text: 'Enter search text:', prompt: 'select',
        }))
      } else if (request.type === 'command' && (
        request.command.startsWith('key:text:') || request.command === 'key:space'
      )) {
        selectionInputs.push(request.command)
      } else {
        server.send(message)
      }
    })
  })

  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('list js')
  await page.keyboard.press('ArrowLeft')
  const textarea = page.locator('.xterm-helper-textarea')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'ob')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('list jobs')
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'unexpected\ncommand')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('Pasted text cannot contain control characters.')
  await expect(consoleOutput).not.toContainText('unexpected')
  await expect(consoleOutput).toContainText('list jobs')

  await page.keyboard.press('End')
  await page.keyboard.press('Control+u')
  await page.keyboard.type('webui-paste-selection')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('Enter search text:')
  await expect(page.getByRole('button', { name: 'Clear' })).toBeDisabled()
  await textarea.evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'example path  ')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect.poll(() => selectionInputs).toEqual(['key:text:example path', 'key:space', 'key:space'])
})

test('pastes text into the real destination browser search', async ({ page }) => {
  test.skip(profile !== 'admin', 'Requires administrative restore access')
  await login(page)
  const consoleOutput = await openConsole(page)
  await consoleOutput.click()
  await page.keyboard.type('run job=RestoreFiles')
  await page.keyboard.press('Enter')
  await expect(consoleOutput).toContainText('Restore Options')

  await page.keyboard.press('ArrowDown')
  await expect(consoleOutput).toContainText('> Restore Client')
  await page.keyboard.press('ArrowDown')
  await expect(consoleOutput).toContainText('> Where [b: browse]')
  await page.keyboard.type('b')
  await expect(consoleOutput).toContainText('Browse destination client')
  await page.keyboard.type('/')
  await expect(consoleOutput).toContainText('Type substring')
  await page.locator('.xterm-helper-textarea').evaluate(element => {
    const clipboardData = new DataTransfer()
    clipboardData.setData('text/plain', 'needle.dir  ')
    element.dispatchEvent(new ClipboardEvent('paste', { clipboardData, bubbles: true }))
  })
  await expect(consoleOutput).toContainText('Search: needle.dir')
  await page.keyboard.type('x')
  await expect.poll(() => consoleOutput.textContent()).toContain('Search: needle.dir  x')
})

test('sends the current console size when switching directors', async ({ page }) => {
  test.skip(!isMultiDirectorProfile, 'Requires multiple Directors')

  const sizes = new Map()
  page.on('websocket', socket => {
    let director
    socket.on('framesent', ({ payload }) => {
      const request = JSON.parse(payload)
      if (request.type === 'session' && request.mode === 'raw') {
        director = request.director
      } else if (director && request.command?.startsWith('.terminalsize ')) {
        sizes.set(director, request.command)
      }
    })
  })

  await login(page)
  await page.goto('/#/console-popup?director=bareos-dir')
  await expect(page.getByTestId('console-output')).toContainText('Connected to bareos-dir')
  await expect.poll(() => sizes.get('bareos-dir')).toMatch(/^\.terminalsize \d+ \d+ color$/)

  await page.getByRole('tab', { name: 'bareos-dir-2', exact: true }).click()
  await expect.poll(() => sizes.get('bareos-dir-2')).toBe(sizes.get('bareos-dir'))

  await page.setViewportSize({ width: 900, height: 720 })
  await expect.poll(() => sizes.get('bareos-dir-2')).not.toBe(sizes.get('bareos-dir'))
  await page.getByRole('tab', { name: 'bareos-dir', exact: true }).click()
  await expect.poll(() => sizes.get('bareos-dir')).toBe(sizes.get('bareos-dir-2'))
})

async function assertDashboardResizePersists(page) {
  await page.getByTitle('Edit layout').click()
  await expect(page.getByRole('button', { name: 'Done' })).toBeVisible()

  const firstItem = page.locator('.vgl-item').first()
  const resizer = firstItem.locator('.vgl-item__resizer').first()
  await expect(firstItem).toBeVisible()
  await expect(resizer).toBeVisible()

  const before = await firstItem.boundingBox()
  if (!before) throw new Error('Could not get initial widget bounds')

  const handle = await resizer.boundingBox()
  if (!handle) throw new Error('Could not get resize handle bounds')

  const startX = handle.x + handle.width / 2
  const startY = handle.y + handle.height / 2
  await page.mouse.move(startX, startY)
  await page.mouse.down()
  await page.mouse.move(startX - 260, startY - 120, { steps: 16 })
  await page.mouse.up()

  let after
  let previous
  let stableSamples = 0
  await expect.poll(async () => {
    const bounds = await firstItem.boundingBox()
    if (!bounds) return false
    const changed = Math.abs(bounds.width - before.width) > 20
      || Math.abs(bounds.height - before.height) > 10
    stableSamples = changed && previous
      && Math.abs(bounds.width - previous.width) < 1
      && Math.abs(bounds.height - previous.height) < 1
      ? stableSamples + 1 : 0
    previous = bounds
    if (stableSamples >= 2) after = bounds
    return stableSamples >= 2
  }).toBe(true)

  await page.getByRole('button', { name: 'Done' }).click()
  await page.reload()
  await expect(firstItem).toBeVisible()
  await expect.poll(async () => {
    const restored = await firstItem.boundingBox()
    if (!restored) return false
    return Math.abs(restored.width - after.width) < 2
      && Math.abs(restored.height - after.height) < 2
  }).toBe(true)
}

test('dashboard first resize attempt must persist', async ({ page }) => {
  await login(page)
  await page.goto('/#/dashboard')
  await assertDashboardResizePersists(page)
})

test('browser regression: dashboard first resize attempt must persist', async ({ page }) => {
  await browserFixture(page)
  await assertDashboardResizePersists(page)
})
