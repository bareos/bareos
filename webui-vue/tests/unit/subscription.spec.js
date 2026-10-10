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
import { subscriptionRemainingSummary, subscriptionSnapshotAge } from '../../src/utils/subscription.js'

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

  describe('accounting snapshot age', () => {
    const snapshot = { calculated_at: '2026-10-04 08:00:00', age_seconds: 3600 }
    it('uses catalog duration even when Director timestamps have a different timezone', () => {
      expect(subscriptionSnapshotAge(snapshot, '2026-10-04 13:00:00'))
        .toEqual({ milliseconds: 3600000, stale: false })
    })
    it('is stale strictly above 24 hours, including elapsed viewing time', () => {
      expect(subscriptionSnapshotAge({ age_seconds: 86400 }, '', 0).stale).toBe(false)
      expect(subscriptionSnapshotAge({ age_seconds: 86400 }, '', 1000).stale).toBe(true)
    })
    it('does not invent an age for missing, invalid or future snapshots', () => {
      expect(subscriptionSnapshotAge(null, '2026-10-05 08:00:00')).toBeNull()
      expect(subscriptionSnapshotAge({ calculated_at: 'invalid' }, '2026-10-05 08:00:00')).toBeNull()
      expect(subscriptionSnapshotAge({ calculated_at: '2026-02-30 08:00:00' }, '2026-10-05 08:00:00')).toBeNull()
      expect(subscriptionSnapshotAge({ age_seconds: -1 }, '')).toBeNull()
      expect(subscriptionSnapshotAge({ age_seconds: '3600' }, '')).toBeNull()
      expect(subscriptionSnapshotAge({ age_seconds: NaN }, '')).toBeNull()
    })
  })
})
