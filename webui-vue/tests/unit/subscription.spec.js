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

import { describe, expect, it } from 'vitest'
import { serializeSubscriptionTable, subscriptionRemainingSummary } from '../../src/utils/subscription.js'

describe('subscription helpers', () => {
  it.each(['csv', 'tsv'])('exports every available table in labeled %s sections', format => {
    const data = {
      'unit-detail': [{ client: 'demo', plugin: 'none', count: 1, size_gb: 2 }],
      'unit-clients': [],
      'unit-plugins': [],
      accounting: [{ client: 'demo', fileset: 'files', files: 1, bytes: 2048 }],
      'unit-summary': { accounting_mode: 'volume-based', used: 1, configured: 2, remaining: 1 },
      summary: { total_files: 1, total_bytes: 2048, total_logical_bytes: 1024 },
    }
    const output = serializeSubscriptionTable(data, 'all', format)
    for (const table of Object.keys(data)) {
      expect(output).toContain(`${table}\r\n${serializeSubscriptionTable(data, table, format)}`)
    }
    expect(output).toContain('\r\n\r\naccounting\r\n')
    expect(() => serializeSubscriptionTable({}, 'all', format)).toThrow('unavailable')
  })

  it('exports CSV with escaped text, raw numbers and spreadsheet-safe names', () => {
    expect(serializeSubscriptionTable({
      'unit-detail': [
        { client: '=SUM(A1)', plugin: 'a,"b"\nc', count: 2, size_gb: 1234.5 },
      ],
    }, 'unit-detail', 'csv')).toBe(
      'client,plugin,count,size_gb\r\n\'=SUM(A1),"a,""b""\nc",2,1234.5\r\n',
    )
  })

  it('exports TSV, preserving Unicode and escaping tabs and unsafe prefixes', () => {
    expect(serializeSubscriptionTable({
      'unit-clients': [{ client: '\tclient\tä', count: 1, size_gb: 0 }],
    }, 'unit-clients', 'tsv')).toBe(
      'client\tcount\tsize_gb\r\n"\'\tclient\tä"\t1\t0\r\n',
    )
  })

  it('exports the summary as a single row and retains negative numeric values', () => {
    expect(serializeSubscriptionTable({
      'unit-summary': { accounting_mode: 'volume-based', used: 2, configured: 1, remaining: -1 },
    }, 'unit-summary', 'csv')).toContain('volume-based,2,1,-1\r\n')
  })

  it.each(['csv', 'tsv'])('preserves negative SQL numeric strings in %s without precision loss', format => {
    const delimiter = format === 'csv' ? ',' : '\t'
    expect(serializeSubscriptionTable({
      'unit-summary': {
        accounting_mode: 'volume-based', used: '10', configured: '5',
        remaining: '-9007199254740993',
      },
    }, 'unit-summary', format)).toContain(
      ['volume-based', '10', '5', '-9007199254740993'].join(delimiter) + '\r\n',
    )
  })

  it.each(['csv', 'tsv'])('protects names and numeric-column formulas in %s', format => {
    const delimiter = format === 'csv' ? ',' : '\t'
    expect(serializeSubscriptionTable({
      'unit-detail': [{
        client: '-5', plugin: '@SUM(A1)', count: '-1+2', size_gb: '-0.5',
      }],
    }, 'unit-detail', format)).toContain(
      ["'-5", "'@SUM(A1)", "'-1+2", '-0.5'].join(delimiter) + '\r\n',
    )
  })

  it('exports accounting values without unit conversion and keeps missing values blank', () => {
    expect(serializeSubscriptionTable({
      accounting: [{ client: 'client', fileset: 'files', files: 2, bytes: 1024,
        logical_bytes: 513, rule_code: 'A', jobs_in_chain: 1, excluded: false }],
    }, 'accounting', 'csv')).toContain('client,files,2,1024,513,A,,1,false,\r\n')
  })

  it('exports an empty table with headers and rejects unavailable tables', () => {
    expect(serializeSubscriptionTable({ 'unit-plugins': [] }, 'unit-plugins', 'csv'))
      .toBe('plugin,count,size_gb\r\n')
    expect(() => serializeSubscriptionTable({}, 'accounting', 'csv')).toThrow('unavailable')
    expect(() => serializeSubscriptionTable({}, 'unknown', 'csv')).toThrow('Unsupported')
    expect(() => serializeSubscriptionTable({}, 'accounting', 'xml')).toThrow('Unsupported')
  })

  it('keeps non-negative remaining values as remaining', () => {
    expect(subscriptionRemainingSummary({ remaining: '0' })).toEqual({
      value: '0',
      labelKey: 'Remaining',
      isOverLimit: false,
    })
    expect(subscriptionRemainingSummary({ remaining: '4' })).toEqual({
      value: '4',
      labelKey: 'Remaining',
      isOverLimit: false,
    })
  })

  it('renders negative remaining values as over limit', () => {
    expect(subscriptionRemainingSummary({ remaining: '-1' })).toEqual({
      value: '1',
      labelKey: 'Over Limit',
      isOverLimit: true,
    })
  })
})
