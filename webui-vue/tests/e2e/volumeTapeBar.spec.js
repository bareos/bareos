/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program. If not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

import { readFile } from 'node:fs/promises'
import { test, expect } from '@playwright/test'

test('a narrow tape bar contains all of its segments', async ({ page }) => {
  const component = await readFile(
    new URL('../../src/pages/VolumeDetailsPage.vue', import.meta.url), 'utf8'
  )
  const css = component.match(/<style scoped>([\s\S]*?)<\/style>/)?.[1]
  expect(css).toBeTruthy()
  await page.setContent(`
    <style>${css}</style>
    <div class="volume-tape-bar" style="width: 300px">
      ${'<div class="volume-tape-segment" style="width: 0.5%"></div>'.repeat(200)}
    </div>
  `)
  const fits = await page.locator('.volume-tape-bar').evaluate(bar => {
    const last = bar.lastElementChild.getBoundingClientRect()
    const bounds = bar.getBoundingClientRect()
    return last.width > 0 && last.right <= bounds.right + 0.1
  })
  expect(fits).toBe(true)
})
