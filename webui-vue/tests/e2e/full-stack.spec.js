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

async function waitForQSelectReady(page, testId) {
  const field = page.locator(`[data-testid="${testId}"]`)
    .locator('xpath=ancestor::*[contains(@class,"q-field")]')
    .first()
  await expect(field).toBeVisible({ timeout: 20000 })
  await expect(field).not.toHaveClass(/q-field--loading/, { timeout: 20000 })
  await expect(field).not.toHaveClass(/q-field--disabled/, { timeout: 20000 })
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

test('dashboard first resize attempt must persist', async ({ page }) => {
  await login(page)
  await page.goto('/#/dashboard')

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

  await page.waitForTimeout(350)
  const after = await firstItem.boundingBox()
  if (!after) throw new Error('Could not get resized widget bounds')

  const widthChanged = Math.abs(after.width - before.width) > 20
  const heightChanged = Math.abs(after.height - before.height) > 10
  expect(widthChanged || heightChanged).toBe(true)
})
