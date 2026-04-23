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

import {
  DEFAULT_WEBUI_LOCALE,
  PHP_WEBUI_LOCALES,
} from '../generated/php-locales.js'

const AVAILABLE_LOCALES = new Set(PHP_WEBUI_LOCALES.map(({ value }) => value))

const PHP_TO_INTL_LOCALE = Object.freeze({
  cn_CN: 'zh-CN',
  cs_CZ: 'cs-CZ',
  nl_BE: 'nl-BE',
  en_EN: 'en',
  fr_FR: 'fr-FR',
  de_DE: 'de-DE',
  hu_HU: 'hu-HU',
  it_IT: 'it-IT',
  pl_PL: 'pl-PL',
  pt_BR: 'pt-BR',
  ru_RU: 'ru-RU',
  sk_SK: 'sk-SK',
  es_ES: 'es-ES',
  tr_TR: 'tr-TR',
  uk_UA: 'uk-UA',
})

const BROWSER_TO_PHP_LOCALE = Object.freeze({
  cn: 'cn_CN',
  'cn-cn': 'cn_CN',
  zh: 'cn_CN',
  'zh-cn': 'cn_CN',
  cs: 'cs_CZ',
  'cs-cz': 'cs_CZ',
  nl: 'nl_BE',
  'nl-be': 'nl_BE',
  en: 'en_EN',
  'en-en': 'en_EN',
  'en-gb': 'en_EN',
  'en-us': 'en_EN',
  fr: 'fr_FR',
  'fr-fr': 'fr_FR',
  de: 'de_DE',
  'de-de': 'de_DE',
  hu: 'hu_HU',
  'hu-hu': 'hu_HU',
  it: 'it_IT',
  'it-it': 'it_IT',
  pl: 'pl_PL',
  'pl-pl': 'pl_PL',
  pt: 'pt_BR',
  'pt-br': 'pt_BR',
  ru: 'ru_RU',
  'ru-ru': 'ru_RU',
  sk: 'sk_SK',
  'sk-sk': 'sk_SK',
  es: 'es_ES',
  'es-es': 'es_ES',
  tr: 'tr_TR',
  'tr-tr': 'tr_TR',
  uk: 'uk_UA',
  'uk-ua': 'uk_UA',
})

const DIRECTOR_MONTHS = Object.freeze({
  Jan: 0,
  Feb: 1,
  Mar: 2,
  Apr: 3,
  May: 4,
  Jun: 5,
  Jul: 6,
  Aug: 7,
  Sep: 8,
  Oct: 9,
  Nov: 10,
  Dec: 11,
})

export const localeOptions = PHP_WEBUI_LOCALES

export function normalizeWebUiLocale(locale) {
  if (typeof locale !== 'string' || locale.length === 0) {
    return DEFAULT_WEBUI_LOCALE
  }

  if (AVAILABLE_LOCALES.has(locale)) {
    return locale
  }

  const normalized = locale.replace('-', '_')
  if (AVAILABLE_LOCALES.has(normalized)) {
    return normalized
  }

  const browserKey = locale.toLowerCase().replace('_', '-')
  return BROWSER_TO_PHP_LOCALE[browserKey]
    ?? BROWSER_TO_PHP_LOCALE[browserKey.split('-')[0]]
    ?? DEFAULT_WEBUI_LOCALE
}

export function localeToIntl(locale = DEFAULT_WEBUI_LOCALE) {
  const normalized = normalizeWebUiLocale(locale)
  return PHP_TO_INTL_LOCALE[normalized] ?? normalized.replace('_', '-')
}

export function detectPreferredLocale(languages) {
  const preferredLanguages = Array.isArray(languages) && languages.length > 0
    ? languages
    : (typeof navigator !== 'undefined'
        ? navigator.languages?.filter(Boolean) ?? [navigator.language].filter(Boolean)
        : [])

  for (const language of preferredLanguages) {
    if (typeof language !== 'string' || language.length === 0) {
      continue
    }

    if (AVAILABLE_LOCALES.has(language)) return language

    const underscoreLocale = language.replace('-', '_')
    if (AVAILABLE_LOCALES.has(underscoreLocale)) return underscoreLocale

    const browserKey = language.toLowerCase().replace('_', '-')
    const mappedLocale = BROWSER_TO_PHP_LOCALE[browserKey]
      ?? BROWSER_TO_PHP_LOCALE[browserKey.split('-')[0]]

    if (mappedLocale && AVAILABLE_LOCALES.has(mappedLocale)) {
      return mappedLocale
    }
  }

  return DEFAULT_WEBUI_LOCALE
}

export function applyDocumentLocale(locale) {
  if (typeof document === 'undefined') return
  document.documentElement.setAttribute('lang', localeToIntl(locale))
}

export function formatNumber(value, locale, options) {
  const number = typeof value === 'number' ? value : Number(value)
  if (!Number.isFinite(number)) return String(value ?? '')
  return new Intl.NumberFormat(localeToIntl(locale), options).format(number)
}

export function formatLocalDateTime(value, locale, options = {}) {
  const date = value instanceof Date ? value : new Date(value)
  if (Number.isNaN(date.getTime())) return String(value ?? '')

  return new Intl.DateTimeFormat(localeToIntl(locale), {
    dateStyle: 'medium',
    timeStyle: 'medium',
    ...options,
  }).format(date)
}

function formatRelativeDiff(diffMs, locale) {
  const absDiffMs = Math.abs(diffMs)
  const formatter = new Intl.RelativeTimeFormat(localeToIntl(locale), {
    numeric: 'auto',
  })
  const units = [
    ['year',   365 * 24 * 60 * 60 * 1000],
    ['month',   30 * 24 * 60 * 60 * 1000],
    ['day',           24 * 60 * 60 * 1000],
    ['hour',               60 * 60 * 1000],
    ['minute',                  60 * 1000],
    ['second',                       1000],
  ]

  for (const [unit, unitMs] of units) {
    if (absDiffMs >= unitMs || unit === 'second') {
      const delta = diffMs / unitMs
      const rounded = diffMs < 0 ? Math.ceil(delta) : Math.floor(delta)
      return formatter.format(rounded, unit)
    }
  }

  return formatter.format(0, 'second')
}

export function formatRelativeDate(date, locale) {
  if (!(date instanceof Date) || Number.isNaN(date.getTime())) {
    return String(date ?? '')
  }

  return formatRelativeDiff(date.getTime() - Date.now(), locale)
}

export function formatSqlRelativeTime(value, locale) {
  if (!value) return '—'

  const date = new Date(String(value).replace(' ', 'T'))
  if (Number.isNaN(date.getTime())) return value

  return formatRelativeDate(date, locale)
}

export function parseDirectorDate(value) {
  if (!value) return null

  const match = String(value).match(
    /^(\d{1,2})-([A-Za-z]{3})-(\d{2})\s+(\d{2}):(\d{2})/,
  )
  if (!match) return null

  const month = DIRECTOR_MONTHS[match[2]]
  if (month === undefined) return null

  return new Date(
    2000 + parseInt(match[3], 10),
    month,
    parseInt(match[1], 10),
    parseInt(match[4], 10),
    parseInt(match[5], 10),
  )
}

export function formatDirectorRelativeTime(value, locale) {
  if (!value) return '—'

  const date = parseDirectorDate(value)
  if (!date) return value

  return formatRelativeDate(date, locale)
}
