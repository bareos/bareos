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

const profile = process.env.BAREOS_WEBUI_PROFILE ?? 'admin'
const username = process.env.BAREOS_WEBUI_USERNAME ?? 'admin'
const password = process.env.BAREOS_WEBUI_PASSWORD ?? 'admin'
const expectedDirectorTransport
  = process.env.BAREOS_WEBUI_EXPECTED_DIRECTOR_TRANSPORT
const isReadonlyProfile = profile === 'readonly'

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
  const loginError = page.locator('[data-testid="login-error"]')

  await page.goto('/')
  await expect(page.locator('[data-testid="login-form"]')).toBeVisible()
  await page.getByLabel('Username').fill(loginUsername)
  await page.getByLabel('Password').fill(loginPassword)
  await page.getByRole('button', { name: 'Login' }).click()

  if (!shouldSucceed) {
    await expect(page.locator('[data-testid="login-error"]')).toBeVisible()
    await expect(page).toHaveURL(/#\/login$/)
    return
  }

  await Promise.race([
    page.waitForURL(/#\/dashboard$/),
    loginError.waitFor({ state: 'visible' }).then(async () => {
      const message = (await loginError.textContent())?.trim()
      throw new Error(`Login failed: ${message || 'Unknown error'}`)
    }),
  ])
  await expectConnected(page)
}

async function openNav(page, testId, urlPattern) {
  await page.locator(`[data-testid="${testId}"]`).click()
  await page.waitForURL(urlPattern)
}

async function selectFirstQOption(page, testId) {
  const field = page.locator(`[data-testid="${testId}"]`)
    .locator('xpath=ancestor::*[contains(@class,"q-field")]')
    .first()
  await field.click()
  const option = page.locator(
    '.q-menu:visible .q-item, .q-menu:visible [role="option"], .q-menu:visible .q-virtual-scroll__content .q-item'
  ).first()
  await expect(option).toBeVisible()
  await option.click()
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
    /Authentication failed|Connection error/i
  )
})

test('reconnects the console session on demand', async ({ page }) => {
  await login(page)

  await page.goto('/#/console-popup')
  const consoleOutput = page.locator('[data-testid="console-output"]')
  await expect(consoleOutput).toContainText('Connected to bareos-dir')

  await page.getByTitle('Reconnect').click()
  await expect(consoleOutput).toContainText('Console disconnected.')
  await expect(consoleOutput).toContainText('Connected to bareos-dir')
})

test('opens jobs and job details through the real director connection', async ({
  page,
}) => {
  await login(page)
  await openNav(page, 'nav-jobs', /#\/jobs/)

  await page.getByLabel('Search in results').fill('backup-bareos-fd')
  const firstRow = page.locator('tbody tr').first()
  await expect(firstRow).toContainText('backup-bareos-fd')

  const jobId = (await firstRow.locator('a.text-primary').first().textContent())?.trim()
  await firstRow.locator('a.text-primary').first().click()

  await expect(page).toHaveURL(/#\/jobs\/\d+/)
  await expect(page.getByText(`Job #${jobId}`, { exact: false })).toBeVisible()
  await expect(page.getByText('Job Summary', { exact: true })).toBeVisible()
  await expect(page.getByText('Job Log', { exact: true })).toBeVisible()
})

test('loads the restore workflow selections', async ({ page }) => {
  await login(page)
  await openNav(page, 'nav-restore', /#\/restore/)

  await page.waitForTimeout(4000)
  await selectFirstQOption(page, 'restore-source-client')
  await expect(page.locator('[data-testid="restore-backup-job"]')).toBeVisible()
  await page.waitForTimeout(2000)
  await selectFirstQOption(page, 'restore-backup-job')
  await expect(page.locator('[data-testid="restore-target-client"]')).toBeVisible()
  await expect(page.locator('[data-testid="restore-job"]')).toBeVisible()
  await expect(page.getByText('Browse Files', { exact: true })).toBeVisible()
  await expect(page.locator('[data-testid="restore-submit"]')).toBeDisabled()
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
  await expect(page.locator('.q-page .text-h6').first()).toContainText(
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

  await page.goto('/#/console-popup')
  const consoleOutput = page.locator('[data-testid="console-output"]')
  await expect(consoleOutput).toContainText('Connected to bareos-dir')

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

  await page.goto('/#/console-popup')
  await expect(page.locator('[data-testid="console-output"]')).toContainText(
    'Connected to bareos-dir'
  )
  await page.getByText('status director', { exact: true }).click()
  await expect(page.locator('[data-testid="console-output"]')).toContainText(
    'Terminated Jobs:'
  )
})
