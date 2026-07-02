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

import { readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import path from 'node:path'
import { fileURLToPath } from 'node:url'

const scriptDir = path.dirname(fileURLToPath(import.meta.url))
export const projectRoot = path.resolve(scriptDir, '..')
export const repositoryRoot = path.resolve(projectRoot, '..')
export const loginFormPath = path.join(
  repositoryRoot,
  'webui',
  'module',
  'Auth',
  'src',
  'Auth',
  'Form',
  'LoginForm.php'
)
export const legacyLanguageDir = path.join(
  repositoryRoot,
  'webui',
  'module',
  'Application',
  'language'
)
export const sourceDir = path.join(projectRoot, 'src')
export const generatedLocalesPath = path.join(
  sourceDir,
  'generated',
  'webui-locales.js'
)
export const generatedMessagesPath = path.join(
  sourceDir,
  'generated',
  'webui-messages.js'
)

const AGPL_HEADER = `/*
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
`

function readUtf8(filePath) {
  return readFileSync(filePath, 'utf8')
}

function walkFiles(root, predicate, result = []) {
  for (const entry of readdirSync(root)) {
    const entryPath = path.join(root, entry)
    const stats = statSync(entryPath)
    if (stats.isDirectory()) {
      walkFiles(entryPath, predicate, result)
      continue
    }
    if (predicate(entryPath)) {
      result.push(entryPath)
    }
  }
  return result
}

function escapeJavaScriptSingleQuotedString(value) {
  return value.replaceAll('\\', '\\\\').replaceAll("'", "\\'")
}

function parsePhpDoubleQuotedString(value) {
  return JSON.parse(value.replace(/\\"/g, '\\"'))
}

function parsePoQuotedString(line) {
  return JSON.parse(line.trim())
}

function parseJavaScriptStringLiteral(literal) {
  return Function(`"use strict"; return (${literal});`)()
}

export function readLegacyLocales(filePath = loginFormPath) {
  const source = readUtf8(filePath)
  const locales = []
  const matches = source.matchAll(
    /\$locales\['([^']+)'\]\s*=\s*("(?:\\.|[^"])*");/g
  )

  for (const match of matches) {
    locales.push({
      value: match[1],
      label: parsePhpDoubleQuotedString(match[2]),
    })
  }

  return locales
}

function appendPoValue(entry, target, line) {
  entry[target] += parsePoQuotedString(line)
}

export function parsePoCatalog(filePath) {
  const lines = readUtf8(filePath).split(/\r?\n/)
  const entries = new Map()
  let current = null
  let currentField = null

  function finishEntry() {
    if (!current || current.msgid === '') {
      current = null
      currentField = null
      return
    }

    entries.set(current.msgid, current.msgstr || current.msgid)
    current = null
    currentField = null
  }

  for (const line of lines) {
    if (line.startsWith('msgid ')) {
      finishEntry()
      current = { msgid: parsePoQuotedString(line.slice(6)), msgstr: '' }
      currentField = 'msgid'
      continue
    }

    if (line.startsWith('msgstr ')) {
      current ??= { msgid: '', msgstr: '' }
      current.msgstr = parsePoQuotedString(line.slice(7))
      currentField = 'msgstr'
      continue
    }

    if (line.startsWith('"') && current && currentField) {
      appendPoValue(current, currentField, line)
      continue
    }

    if (line.trim() === '') {
      finishEntry()
    }
  }

  finishEntry()
  return entries
}

export function collectVueMessageIds(root = sourceDir) {
  const files = walkFiles(
    root,
    (entryPath) =>
      /\.(js|vue)$/.test(entryPath)
      && !entryPath.includes(`${path.sep}generated${path.sep}`)
  ).sort()

  const messageIds = new Set()
  const literalPattern = /(?:^|[^\w$.])(?:t|translate)\(\s*(["'])((?:\\.|(?!\1)[\s\S])*)\1/g

  for (const filePath of files) {
    const source = readUtf8(filePath)
    for (const match of source.matchAll(literalPattern)) {
      const quote = match[1]
      const value = match[2]
      messageIds.add(parseJavaScriptStringLiteral(`${quote}${value}${quote}`))
    }
  }

  return [...messageIds].sort((left, right) => left.localeCompare(right))
}

export function buildWebUiMessages(locales, vueMessageIds, languageDir = legacyLanguageDir) {
  const messages = {}

  for (const { value: locale } of locales) {
    const catalogPath = path.join(languageDir, `${locale}.po`)
    const catalogEntries = parsePoCatalog(catalogPath)
    const catalog = Object.fromEntries(catalogEntries)

    for (const msgid of vueMessageIds) {
      if (!(msgid in catalog)) {
        catalog[msgid] = msgid
      }
    }

    messages[locale] = catalog
  }

  return messages
}

export function buildWebUiLocalesSource(locales) {
  const localeItems = locales
    .map(
      ({ value, label }) =>
        `  { value: '${escapeJavaScriptSingleQuotedString(value)}', label: '${escapeJavaScriptSingleQuotedString(label)}' },`
    )
    .join('\n')

  return `${AGPL_HEADER}
// Generated from the legacy PHP WebUI locale list.

export const DEFAULT_WEBUI_LOCALE = 'en_EN'

export const WEBUI_LOCALES = [
${localeItems}
]
`
}

export function buildWebUiMessagesSource(messages) {
  return `${AGPL_HEADER}
// Generated from the legacy PHP WebUI catalogs and Vue literal msgids.

export const WEBUI_MESSAGES = ${JSON.stringify(messages, null, 2)}
`
}

export function generateWebUiI18n() {
  const locales = readLegacyLocales()
  const vueMessageIds = collectVueMessageIds()
  const messages = buildWebUiMessages(locales, vueMessageIds)

  return {
    locales,
    vueMessageIds,
    messages,
    localesSource: buildWebUiLocalesSource(locales),
    messagesSource: buildWebUiMessagesSource(messages),
  }
}

export function writeGeneratedWebUiI18n() {
  const generated = generateWebUiI18n()

  writeFileSync(generatedLocalesPath, generated.localesSource)
  writeFileSync(generatedMessagesPath, generated.messagesSource)

  return generated
}

if (import.meta.url === `file://${process.argv[1]}`) {
  writeGeneratedWebUiI18n()
}
