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

import {
  buildDirectorStatusCards,
  configStatusChipColor,
  configStatusChipIcon,
  configStatusChipLabel,
  configStatusChipTooltip,
  resolveDirectorOsChip,
} from '../../src/utils/directorStatus.js'

describe('director status helpers', () => {
  const t = (value) => value

  it('builds status cards with reported director, OS icon, and config access', () => {
    const cards = buildDirectorStatusCards(
      [{
        director: 'prod-a',
        header: {
          director: 'prod-a-dir',
          os: 'Windows Server 2022',
          jobs_run: 42,
        },
      }],
      {
        'prod-a': {
          available: true,
          message: '',
        },
      }
    )

    expect(cards).toEqual([expect.objectContaining({
      scopeDirector: 'prod-a',
      reportedDirector: 'prod-a-dir',
      jobs_run: 42,
      configStatusAvailable: true,
      configStatusUnavailableReason: '',
      osIcon: {
        icon: 'mdi-microsoft-windows',
        color: 'blue-7',
      },
    })])
  })

  it('chooses platform icons from the raw director OS string', () => {
    expect(resolveDirectorOsChip('Darwin 24.0')).toEqual({
      icon: 'mdi-apple',
      color: 'grey-8',
    })
    expect(resolveDirectorOsChip('Ubuntu 24.04')).toEqual({
      icon: 'mdi-ubuntu',
      color: 'deep-orange-7',
    })
  })

  it('derives config chip states and tooltips from access and warnings', () => {
    expect(configStatusChipColor({
      configStatusAvailable: true,
      config_warnings: 2,
    })).toBe('negative')
    expect(configStatusChipIcon({
      configStatusAvailable: true,
      config_warnings: 0,
    })).toBe('check_circle')
    expect(configStatusChipLabel({
      configStatusAvailable: false,
    }, t)).toBe('Config Unavailable')
    expect(configStatusChipTooltip({
      configStatusAvailable: false,
      configStatusUnavailableReason: 'ACL denied',
    }, t)).toBe('Configuration status unavailable because the required ACL is missing.')
    expect(configStatusChipTooltip({
      configStatusAvailable: false,
      configStatusUnavailableReason: 'probe failed',
    }, t)).toBe('Configuration status unavailable: probe failed')
  })
})
