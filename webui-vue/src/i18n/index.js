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

const MESSAGE_OVERRIDES = {
  de_DE: {
    'Log in to Bareos': 'Bei Bareos anmelden',
    Login: 'Anmelden',
    Username: 'Benutzername',
    Password: 'Passwort',
    Directors: 'Direktoren',
    'Retry remaining directors': 'Verbleibende Direktoren erneut versuchen',
    'Skip failed directors': 'Fehlgeschlagene Direktoren überspringen',
    'Reuse current credentials': 'Aktuelle Zugangsdaten wiederverwenden',
    'Retry the remaining directors or skip them.':
      'Verbleibende Direktoren erneut versuchen oder überspringen.',
    'Retry the remaining directors.':
      'Verbleibende Direktoren erneut versuchen.',
    'The entered credentials will be tried on all configured directors.':
      'Die eingegebenen Zugangsdaten werden bei allen konfigurierten Direktoren ausprobiert.',
    'Login failure': 'Anmeldung fehlgeschlagen',
    'Successfully logged in': 'Erfolgreich angemeldet',
    'Not yet logged in': 'Noch nicht angemeldet',
    'Could not load the configured directors.':
      'Die konfigurierten Direktoren konnten nicht geladen werden.',
    'Could not log in to any configured director. Retry the remaining directors.':
      'An keinem konfigurierten Direktor konnte eine Anmeldung erfolgen. Verbleibende Direktoren erneut versuchen.',
    'Authentication failed': 'Authentifizierung fehlgeschlagen',
    'Could not connect to director. Is the proxy running?':
      'Verbindung zum Direktor fehlgeschlagen. Läuft der Proxy?',
    'WebSocket connection failed': 'WebSocket-Verbindung fehlgeschlagen',
    'WebSocket connection failed. Check proxy configuration or firewall.':
      'WebSocket-Verbindung fehlgeschlagen. Proxy-Konfiguration oder Firewall prüfen.',
    'Director list request failed: {message}':
      'Anfrage der Direktorenliste fehlgeschlagen: {message}',
  },
}

const EFFECTIVE_MESSAGES = Object.fromEntries(
  Object.entries(WEBUI_MESSAGES).map(([locale, messages]) => [
    locale,
    {
      ...messages,
      ...(MESSAGE_OVERRIDES[locale] ?? {}),
    },
  ])
)

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
  messages: EFFECTIVE_MESSAGES,
  missingWarn: false,
  fallbackWarn: false,
})

export function setI18nLocale(locale) {
  i18n.global.locale.value = normalizeWebUiLocale(locale)
}

export function translate(locale, msgid, values) {
  const effectiveLocale = normalizeWebUiLocale(locale)
  const catalog = EFFECTIVE_MESSAGES[effectiveLocale]
    ?? EFFECTIVE_MESSAGES[DEFAULT_WEBUI_LOCALE]
    ?? {}
  const fallbackCatalog = EFFECTIVE_MESSAGES[DEFAULT_WEBUI_LOCALE] ?? {}
  const message = catalog[msgid] ?? fallbackCatalog[msgid] ?? msgid

  return interpolate(message, values)
}
