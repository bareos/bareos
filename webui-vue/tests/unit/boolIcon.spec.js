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

import { createApp } from 'vue'
import { createI18n } from 'vue-i18n'
import { Quasar } from 'quasar/dist/quasar.client.js'
import { describe, expect, it } from 'vitest'
import BoolIcon from '../../src/components/BoolIcon.vue'

describe('boolean icon accessibility', () => {
  it.each([[true, 'Yes'], [false, 'No']])('exposes the %s label', (value, label) => {
    const app = createApp(BoolIcon, { value })
    app.use(Quasar)
    app.use(createI18n({ legacy: false, locale: 'en', messages: { en: {} }, missingWarn: false, fallbackWarn: false }))
    const host = document.createElement('div')
    app.mount(host)
    try {
      const icon = host.querySelector('[role="img"]')
      expect(icon.getAttribute('aria-hidden')).toBe('false')
      expect(icon.getAttribute('aria-label')).toBe(label)
    } finally {
      app.unmount()
    }
  })
})
