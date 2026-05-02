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

async function login(page) {
  await page.goto('/')
  await expect(page.locator('[data-testid="login-form"]')).toBeVisible()
  await page.getByLabel('Username').fill(
    process.env.BAREOS_WEBUI_USERNAME ?? 'admin'
  )
  await page.getByLabel('Password').fill(
    process.env.BAREOS_WEBUI_PASSWORD ?? 'admin'
  )
  await page.getByRole('button', { name: 'Login' }).click()
  await page.waitForURL(/#\/dashboard$/)
  await expect(page.locator('[data-testid="director-status-label"]')).toContainText(
    'Connected'
  )
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

test('opens the console and runs a raw command through the real proxy', async ({ page }) => {
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
