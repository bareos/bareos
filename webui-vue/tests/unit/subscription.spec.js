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
import { subscriptionRemainingSummary } from '../../src/utils/subscription.js'

describe('subscription helpers', () => {
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
