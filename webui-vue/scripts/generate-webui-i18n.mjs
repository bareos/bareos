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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
 */

import {
  existsSync,
  readFileSync,
  readdirSync,
  statSync,
  writeFileSync,
} from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDir = path.dirname(fileURLToPath(import.meta.url))
export const projectRoot = path.resolve(scriptDir, '..')
export const sourceDir = path.join(projectRoot, 'src')
export const localesDir = path.join(sourceDir, 'i18n', 'locales')
export const localeManifestPath = path.join(localesDir, 'locales.json')

function readUtf8(filePath) {
  return readFileSync(filePath, 'utf8')
}

function readJson(filePath) {
  try {
    return JSON.parse(readUtf8(filePath))
  } catch (error) {
    throw new Error(`Invalid JSON in ${path.relative(projectRoot, filePath)}: ${error.message}`)
  }
}

function walkFiles(root, predicate, result = []) {
  for (const entry of readdirSync(root)) {
    const entryPath = path.join(root, entry)
    const stats = statSync(entryPath)
    if (stats.isDirectory()) {
      if (entryPath !== localesDir && !entryPath.includes(`${path.sep}generated${path.sep}`)) {
        walkFiles(entryPath, predicate, result)
      }
      continue
    }
    if (predicate(entryPath)) result.push(entryPath)
  }
  return result
}

function parseJavaScriptStringLiteral(literal) {
  return Function(`"use strict"; return (${literal});`)()
}

function collectLiteralMatches(source, pattern, messages) {
  for (const match of source.matchAll(pattern)) {
    const value = parseJavaScriptStringLiteral(`${match[1]}${match[2]}${match[1]}`)
    if (value) messages.add(value)
  }
}

function compareMessageIds(left, right) {
  if (left < right) return -1
  if (left > right) return 1
  return 0
}

export function collectVueMessageIds(root = sourceDir) {
  const files = walkFiles(root, entryPath => /\.(js|vue)$/.test(entryPath)).sort()
  const messages = new Set()

  for (const filePath of files) {
    const source = readUtf8(filePath)
    collectLiteralMatches(
      source,
      /(?:^|[^\w$.])(?:t|translate|messageId)\(\s*(["'])((?:\\.|(?!\1)[\s\S])*)\1/g,
      messages,
    )

    // Existing data structures predate messageId(). Keep their user-facing
    // metadata discoverable while they are migrated to explicit markers.
    collectLiteralMatches(
      source,
      /\b(?:label|labelKey|description|defaultTitle|category|headerTitle)\s*:\s*(["'])((?:\\.|(?!\1).)*)\1/g,
      messages,
    )
  }

  messages.delete('/')
  return [...messages].sort(compareMessageIds)
}

export function readLocaleManifest(filePath = localeManifestPath) {
  const manifest = readJson(filePath)
  if (
    typeof manifest?.default !== 'string'
    || !Array.isArray(manifest.locales)
    || manifest.locales.some(locale => (
      typeof locale?.value !== 'string' || typeof locale?.label !== 'string'
    ))
  ) {
    throw new Error('src/i18n/locales/locales.json has an invalid structure')
  }

  const localeNames = manifest.locales.map(locale => locale.value)
  if (!localeNames.includes(manifest.default)) {
    throw new Error(`Default locale "${manifest.default}" is not listed`)
  }
  if (new Set(localeNames).size !== localeNames.length) {
    throw new Error('src/i18n/locales/locales.json contains duplicate locales')
  }
  return manifest
}

export function catalogPath(locale) {
  return path.join(localesDir, `${locale}.json`)
}

function sortedCatalog(messageIds, existing, sourceLocale) {
  return Object.fromEntries(messageIds.map(messageId => [
    messageId,
    sourceLocale ? messageId : (existing[messageId] ?? ''),
  ]))
}

function catalogSource(catalog) {
  return `${JSON.stringify(catalog, null, 2)}\n`
}

function placeholders(message) {
  return [...String(message).matchAll(/\{([A-Za-z_][A-Za-z0-9_]*)\}/g)]
    .map(match => match[1])
    .sort()
}

function validateTranslation(locale, messageId, translation) {
  if (typeof translation !== 'string') {
    return `${locale}: "${messageId}" must have a string value`
  }
  if (!translation) return null

  const expected = placeholders(messageId)
  const actual = placeholders(translation)
  if (JSON.stringify(expected) !== JSON.stringify(actual)) {
    return `${locale}: "${messageId}" has placeholders {${actual.join(', ')}}; expected {${expected.join(', ')}}`
  }
  return null
}

export function buildExpectedCatalogs() {
  const manifest = readLocaleManifest()
  const messageIds = collectVueMessageIds()
  const catalogs = {}

  for (const { value: locale } of manifest.locales) {
    const filePath = catalogPath(locale)
    const existing = existsSync(filePath) ? readJson(filePath) : {}
    catalogs[locale] = sortedCatalog(messageIds, existing, locale === manifest.default)
  }

  return { manifest, messageIds, catalogs }
}

export function validateCatalogs({ manifest, messageIds, catalogs }) {
  const errors = []
  const expectedKeys = JSON.stringify(messageIds)
  const expectedFiles = new Set([
    'locales.json',
    'catalogs.js',
    ...manifest.locales.map(locale => `${locale.value}.json`),
  ])

  for (const entry of readdirSync(localesDir)) {
    if (entry.endsWith('.json') && !expectedFiles.has(entry)) {
      errors.push(`Unexpected locale file: src/i18n/locales/${entry}`)
    }
  }

  for (const { value: locale } of manifest.locales) {
    const catalog = catalogs[locale]
    if (JSON.stringify(Object.keys(catalog)) !== expectedKeys) {
      errors.push(`${locale}.json keys do not match the extracted source messages`)
    }
    for (const messageId of messageIds) {
      const error = validateTranslation(locale, messageId, catalog[messageId])
      if (error) errors.push(error)
      if (locale === manifest.default && catalog[messageId] !== messageId) {
        errors.push(`${locale}: source value for "${messageId}" must equal its key`)
      }
    }
  }
  return errors
}

export function updateJsonCatalogs() {
  const expected = buildExpectedCatalogs()
  for (const { value: locale } of expected.manifest.locales) {
    writeFileSync(catalogPath(locale), catalogSource(expected.catalogs[locale]))
  }
  return expected
}

export function checkJsonCatalogs() {
  const expected = buildExpectedCatalogs()
  const errors = validateCatalogs(expected)

  for (const { value: locale } of expected.manifest.locales) {
    const filePath = catalogPath(locale)
    if (!existsSync(filePath)) {
      errors.push(`Missing locale file: src/i18n/locales/${locale}.json`)
      continue
    }
    if (readUtf8(filePath) !== catalogSource(expected.catalogs[locale])) {
      errors.push(`src/i18n/locales/${locale}.json is not synchronized; run npm run update:i18n`)
    }
  }

  if (errors.length) throw new Error(errors.join('\n'))
  return expected
}

// Compatibility for callers of the previous generator API.
export function generateWebUiI18n() {
  return buildExpectedCatalogs()
}

const command = process.argv[2] ?? '--write'
if (import.meta.url === `file://${process.argv[1]}`) {
  if (command === '--check') {
    const { messageIds, manifest } = checkJsonCatalogs()
    console.log(`Checked ${messageIds.length} messages in ${manifest.locales.length} locales.`)
  } else if (command === '--write') {
    const updated = updateJsonCatalogs()
    const errors = validateCatalogs(updated)
    if (errors.length) throw new Error(errors.join('\n'))
    console.log(`Updated ${updated.messageIds.length} messages in ${updated.manifest.locales.length} locales.`)
  } else {
    throw new Error(`Unknown argument: ${command}`)
  }
}
