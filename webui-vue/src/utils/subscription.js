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

export function subscriptionRemainingSummary(unitSummary) {
  const rawRemaining = unitSummary?.remaining
  const parsedRemaining = Number(rawRemaining)

  if (!Number.isFinite(parsedRemaining)) {
    return {
      value: String(rawRemaining ?? ''),
      labelKey: 'Remaining',
      isOverLimit: false,
    }

  }

  if (parsedRemaining < 0) {
    return {
      value: String(Math.abs(parsedRemaining)),
      labelKey: 'Over Limit',
      isOverLimit: true,
    }
  }

  return {
    value: String(parsedRemaining),
    labelKey: 'Remaining',
    isOverLimit: false,
  }
}

const subscriptionExportTables = {
  'unit-detail': ['client', 'plugin', 'count', 'size_gb'],
  'unit-clients': ['client', 'count', 'size_gb'],
  'unit-plugins': ['plugin', 'count', 'size_gb'],
  accounting: [
    'client', 'fileset', 'files', 'bytes', 'logical_bytes', 'rule_code',
    'rule_explanation', 'jobs_in_chain', 'excluded', 'accounting_issue',
  ],
  'unit-summary': ['accounting_mode', 'used', 'configured', 'remaining'],
  summary: ['total_files', 'total_bytes', 'total_logical_bytes'],
}

export function serializeSubscriptionTable(data, table, format) {
  if (table === 'all') {
    const sections = Object.keys(subscriptionExportTables)
      .filter(key => data[key] != null)
      .map(key => `${key}\r\n${serializeSubscriptionTable(data, key, format)}`)
    if (!sections.length) {
      throw new Error('The selected subscription table is unavailable')
    }
    return sections.join('\r\n')
  }
  const columns = subscriptionExportTables[table]
  if (!columns || !['csv', 'tsv'].includes(format)) {
    throw new Error('Unsupported subscription export table or format')
  }
  const dataset = data[table]
  const isSummary = table === 'unit-summary' || table === 'summary'
  if (dataset == null || (!isSummary && !Array.isArray(dataset))) {
    throw new Error('The selected subscription table is unavailable')
  }
  const rows = isSummary ? [dataset] : dataset
  const delimiter = format === 'csv' ? ',' : '\t'
  const escape = value => {
    let text = String(value ?? '')
    // Spreadsheet applications interpret these text prefixes as formulas.
    if (typeof value === 'string' && /^\s*[=+\-@]|^[\t\r\n]/.test(text)) {
      text = `'${text}`
    }
    return /["\r\n\t,]/.test(text)
      ? `"${text.replaceAll('"', '""')}"`
      : text
  }
  return [
    columns.join(delimiter),
    ...rows.map(row => columns.map(column => escape(row[column])).join(delimiter)),
  ].join('\r\n') + '\r\n'
}
