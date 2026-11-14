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

// Exercise the public application with an isolated, entirely intercepted
// transport. In particular, no mutation can reach a real Director.
async function browserFixture(page, commandHandler = () => undefined) {
  const directors = ['bareos-dir', 'bareos-dir-2']
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
      try {
        const data = await commandHandler(request.command, director)
        const job = {
          jobid: '101', name: 'Browser-fixture', jobtype: 'B', jobstatus: 'T',
          comment: 'Original comment',
        }
        socket.send(JSON.stringify({
          type: 'response', id: request.id,
          data: data ?? (
            request.command.startsWith('llist jobs count') ? { jobs: [{ count: '1' }] }
              : request.command.startsWith('llist job') ? { jobs: [job] }
                : request.command.startsWith('list joblog') ? { joblog: [{ logtext: 'Termination: Backup OK' }] }
                  : {}
          ),
        }))
      } catch (error) {
        socket.send(JSON.stringify({ type: 'error', id: request.id, message: error.message }))
      }
    })
  })
  await page.goto('/#/dashboard')
  await expectConnected(page)
}

test('browser regression: column checkboxes have accessible names', async ({ page }) => {
  await browserFixture(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByRole('button', { name: 'Choose columns', exact: true }).click()
  await expect(page.getByRole('checkbox', { name: 'Comment', exact: true })).toBeVisible()
  for (const checkbox of await page.getByRole('checkbox').all()) {
    await expect(checkbox).toHaveAccessibleName(/\S/)
  }
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
      window.location.hash === '#/dashboard'
      || document.querySelector('[data-testid="login-form"]')
    ), { timeout: LOGIN_RESULT_TIMEOUT_MS })

    if (page.url().endsWith('/#/dashboard')) {
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
      window.location.hash === '#/dashboard'
      || document.querySelector('[data-testid="login-error"]')?.textContent?.trim()
    ), { timeout: LOGIN_RESULT_TIMEOUT_MS })

    if (!shouldSucceed) {
      await expect(loginError).toBeVisible()
      await expect(page).toHaveURL(/#\/login$/)
      return
    }

    if (page.url().endsWith('/#/dashboard')) {
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
  let lastError
  while (Date.now() < deadline) {
    try {
      await combobox.click()
      const listboxId = await combobox.getAttribute('aria-controls')
      if (!listboxId) {
        throw new Error(`Could not find the option list for ${testId}`)
      }
      let options = page.locator(`[id="${listboxId}"]`).getByRole('option')
      if (hasText) {
        options = options.filter({ hasText })
      }
      const option = options.first()
      await expect(option).toBeVisible({
        timeout: Math.min(1000, Math.max(1, deadline - Date.now())),
      })
      await option.click()
      if (selected) {
        await selected()
      }
      return
    } catch (error) {
      lastError = error
      await page.keyboard.press('Escape')
      await page.waitForTimeout(300)
    }
  }

  throw new Error(`Could not open option menu for ${testId}`, { cause: lastError })
}

async function waitForQSelectReady(page, testId) {
  const field = page.locator(`[data-testid="${testId}"]`)
    .locator('xpath=ancestor::*[contains(@class,"q-field")]')
    .first()
  await expect(field).toBeVisible({ timeout: 20000 })
  await expect(field).not.toHaveClass(/q-field--loading/, { timeout: 20000 })
  await expect(field).not.toHaveClass(/q-field--disabled/, { timeout: 20000 })
}

test('logs in and shows the dashboard', async ({ page }) => {
  await login(page)
  const recentJobsCard = page.locator('.q-card').filter({
    hasText: 'Most recent job status per job name',
  })
  const totalsCard = page.locator('.q-card').filter({
    hasText: 'Job Totals',
  })

  await expect(page.getByText('Running Jobs', { exact: true })).toBeVisible()
  await expect(recentJobsCard).toBeVisible()
  await expect(recentJobsCard).not.toContainText('No data available')
  await expect(recentJobsCard).toContainText('backup-bareos-fd')
  await expect(totalsCard).not.toContainText('Total Jobs0')
  await expect(totalsCard).not.toContainText('Total Bytes0 B')
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
  await page.waitForURL(/#\/dashboard$/)
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
  await expect(page.locator('.q-banner.bg-warning')).toContainText('1,001')
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

test('shows comment editing on mouse hover and keyboard focus', async ({ page }) => {
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)
  await page.getByRole('button', { name: 'Choose columns' }).click()
  await page.locator('.q-menu').getByText('Comment', { exact: true }).click()
  await page.keyboard.press('Escape')

  const edit = page.locator('[data-testid^="job-comment-edit-"]').first()
  await expect(edit).toHaveCSS('opacity', '0')
  await edit.locator('..').hover()
  await expect(edit).toHaveCSS('opacity', '1')
  await page.mouse.move(0, 0)
  await expect(edit).toHaveCSS('opacity', '0')
  await page.keyboard.press('Tab')
  await edit.focus()
  await expect(edit).toHaveCSS('opacity', '1')
  await edit.click()
  await expect(page.getByRole('dialog')).toBeVisible()
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
  await page.getByRole('tab', { name: 'Pools' }).click()
  const firstPool = page.locator('tbody tr a.text-primary').first()
  const poolName = (await firstPool.textContent())?.trim()
  await firstPool.click()

  await expect(page).toHaveURL(/#\/storages\/pools\/.+/)
  await expect(page.locator('.q-page .text-h5').first()).toContainText(
    poolName ?? ''
  )
  await expect(page.getByText('Volumes', { exact: true })).toBeVisible()

  await page.goBack()
  await page.getByRole('tab', { name: 'Volumes' }).click()
  const firstVolume = page.locator('tbody tr a.text-primary').first()
  await firstVolume.click()
  await expect(page).toHaveURL(/#\/storages\/volumes\/.+/)
  await expect(page.getByText('Volume Properties', { exact: true })).toBeVisible()
})

test('covers schedules and director tabs through the director connection', async ({
  page,
}) => {
  await login(page)

  await openNav(page, 'nav-schedules', /#\/schedules/)
  await expect(page.getByText('Scheduler Jobs', { exact: true })).toBeVisible()
  await expect(page.getByText('Scheduler Preview', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Show' }).click()
  await expect(
    page.locator('.q-tab-panel:visible').getByText('Schedules', { exact: true })
  ).toBeVisible()

  await openNav(page, 'nav-director', /#\/director/)
  await expect(page.getByText('Director Info', { exact: true })).toBeVisible()
  await expect(page.getByText('Scheduled Jobs', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Messages' }).click()
  await expect(page.getByText('Director Messages', { exact: true })).toBeVisible()

  await page.getByRole('tab', { name: 'Catalog Maintenance' }).click()
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
  await expect(consoleOutput).toContainText('status director')
  await expect(consoleOutput).toContainText('Terminated Jobs:')
})

test('restores the proxy-backed login after a page reload', async ({ page }) => {
  await login(page)

  // Reload clears the SPA state; /api/session must restore the proxy-backed
  // login from the session cookie so users stay signed in.
  await page.reload()
  await page.waitForURL(/#\/dashboard$/)
  await expectConnected(page)
})

test('opens the console and runs a raw command through the proxied director connection', async ({
  page,
}) => {
  await login(page)

  const consoleOutput = await openConsole(page)
  await page.getByText('status director', { exact: true }).click()
  await expect(consoleOutput).toContainText('Terminated Jobs:')
})
