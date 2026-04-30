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

import { createI18n } from 'vue-i18n'
import { DEFAULT_WEBUI_LOCALE } from '../generated/webui-locales.js'
import { WEBUI_MESSAGES } from '../generated/webui-messages.js'
import { normalizeWebUiLocale } from '../utils/locales.js'

function interpolate(message, values = {}) {
  return Object.entries(values).reduce(
    (text, [key, value]) => text.replaceAll(`{${key}}`, String(value)),
    message
  )
}

export const i18n = createI18n({
  legacy: false,
  locale: DEFAULT_WEBUI_LOCALE,
  fallbackLocale: DEFAULT_WEBUI_LOCALE,
  messages: WEBUI_MESSAGES,
  missingWarn: false,
  fallbackWarn: false,
})

export function setI18nLocale(locale) {
  i18n.global.locale.value = normalizeWebUiLocale(locale)
}

export function translate(locale, msgid, values) {
  const effectiveLocale = normalizeWebUiLocale(locale)
  const catalog = WEBUI_MESSAGES[effectiveLocale]
    ?? WEBUI_MESSAGES[DEFAULT_WEBUI_LOCALE]
    ?? {}
  const fallbackCatalog = WEBUI_MESSAGES[DEFAULT_WEBUI_LOCALE] ?? {}
  const message = catalog[msgid] ?? fallbackCatalog[msgid] ?? msgid

  return interpolate(message, values)
}
