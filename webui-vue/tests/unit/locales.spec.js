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

import { describe, expect, it, vi } from 'vitest'
import {
  DEFAULT_WEBUI_LOCALE,
  WEBUI_LOCALES,
} from '../../src/generated/webui-locales.js'
import {
  detectPreferredLocale,
  formatDirectorRelativeTime,
  formatRelativeDate,
  formatSqlRelativeTime,
  localeFlagEmoji,
  localeOptions,
  localeToIntl,
  normalizeWebUiLocale,
} from '../../src/utils/locales.js'

describe('webui locales', () => {
  it('matches the PHP WebUI locale catalog', () => {
    expect(WEBUI_LOCALES.map(({ value }) => value)).toEqual([
      'cn_CN',
      'cs_CZ',
      'nl_BE',
      'en_EN',
      'fr_FR',
      'de_DE',
      'hu_HU',
      'it_IT',
      'pl_PL',
      'pt_BR',
      'ru_RU',
      'sk_SK',
      'es_ES',
      'tr_TR',
      'uk_UA',
    ])
  })

  it('maps browser locales onto the PHP locale keys', () => {
    expect(normalizeWebUiLocale('de-DE')).toBe('de_DE')
    expect(normalizeWebUiLocale('en-US')).toBe('en_EN')
    expect(normalizeWebUiLocale('zh-CN')).toBe('cn_CN')
    expect(localeToIntl('cn_CN')).toBe('zh-CN')
  })

  it('detects the first supported browser language', () => {
    expect(detectPreferredLocale(['zz-ZZ', 'tr-TR', 'de-DE'])).toBe('tr_TR')
    expect(detectPreferredLocale(['zz-ZZ'])).toBe(DEFAULT_WEBUI_LOCALE)
  })

  it('decorates locale options with flag icons', () => {
    expect(localeFlagEmoji('de-DE')).toBe('🇩🇪')
    expect(localeOptions.find(({ value }) => value === 'pt_BR')).toMatchObject({
      value: 'pt_BR',
      label: 'Portuguese (BR)',
      flag: '🇧🇷',
    })
  })

  it('formats SQL and director timestamps with relative locale-aware text', () => {
    vi.useFakeTimers()
    vi.setSystemTime(new Date('2026-04-23T12:00:00'))

    expect(formatSqlRelativeTime('2026-04-23 11:58:00', 'en_EN')).toBe('2 minutes ago')
    expect(formatDirectorRelativeTime('23-Apr-26 11:58', 'en_EN')).toBe('2 minutes ago')

    vi.useRealTimers()
  })

  it('uses calendar-day boundaries, not raw 24h chunks, for day-level relative labels', () => {
    vi.useFakeTimers()
    // Friday 21:12 local time.
    vi.setSystemTime(new Date(2026, 8, 11, 21, 12, 0))

    // Less than 24 raw hours away but crosses two calendar-day boundaries
    // (Saturday, then Sunday) -> should read "in 2 days", not "tomorrow".
    const dayAfterTomorrow = new Date(2026, 8, 13, 3, 0, 0)
    expect(formatRelativeDate(dayAfterTomorrow, 'en_EN')).toBe('in 2 days')

    // A run tomorrow (Saturday) should still read "tomorrow".
    const tomorrow = new Date(2026, 8, 12, 21, 0, 0)
    expect(formatRelativeDate(tomorrow, 'en_EN')).toBe('tomorrow')

    vi.useRealTimers()
  })
})
