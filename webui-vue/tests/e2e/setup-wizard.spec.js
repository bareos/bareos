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

const setupRepositoryPath = process.env.BAREOS_SETUP_REPOSITORY_PATH
const setupRuntimeRoot = process.env.BAREOS_SETUP_RUNTIME_ROOT
const setupDirectorPort = process.env.BAREOS_SETUP_DIRECTOR_PORT
const setupClientPort = process.env.BAREOS_SETUP_CLIENT_PORT
const setupStoragePort = process.env.BAREOS_SETUP_STORAGE_PORT
const setupConsolePassword = process.env.BAREOS_SETUP_CONSOLE_PASSWORD ?? 'wizard-secret'

test('creates the initial deployment and reaches login', async ({ page }) => {
  test.setTimeout(180_000)

  await page.goto('/')
  await expect(page.getByText('Initial setup wizard', { exact: true })).toBeVisible()

  await page.getByRole('button', { name: 'Start wizard' }).click()
  await page.getByRole('button', { name: /Advanced deployment settings/ }).click()
  await page.getByLabel('Repository path').fill(setupRepositoryPath)
  await page.getByLabel('Runtime root').fill(setupRuntimeRoot)
  await page.getByRole('button', { name: 'Continue' }).click()

  await page.getByRole('button', { name: /Advanced service settings/ }).click()
  await page.getByLabel('Director port').fill(setupDirectorPort)
  await page.getByLabel('File daemon port').fill(setupClientPort)
  await page.getByLabel('Storage daemon port').fill(setupStoragePort)
  await page.getByLabel('Admin console password').fill(setupConsolePassword)
  await page.getByRole('button', { name: 'Continue' }).click()

  await page.locator('[data-testid="setup-submit"]').click()
  await expect(page.locator('[data-testid="setup-progress"]')).toContainText(
    'Creating catalog database',
    { timeout: 120_000 }
  )
  await expect(page.locator('[data-testid="setup-progress"]')).toContainText(
    'Starting generated Bareos daemons',
    { timeout: 120_000 }
  )
  await expect(page.locator('[data-testid="setup-progress"]')).toContainText(
    'Setup finished successfully',
    { timeout: 120_000 }
  )

  await page.locator('[data-testid="setup-continue-login"]').click()
  await page.waitForURL(/#\/login$/)
  await expect(page.locator('[data-testid="login-form"]')).toBeVisible()
  await expect(page.getByLabel('Username')).toHaveValue('admin')
  await expect(page.getByLabel('Password')).toHaveValue(setupConsolePassword)

  await page.getByRole('button', { name: 'Login' }).click()
  await page.waitForURL(/#\/dashboard$/)
  await expect(page.locator('[data-testid="director-status-label"]')).toContainText(
    'Connected'
  )
})
