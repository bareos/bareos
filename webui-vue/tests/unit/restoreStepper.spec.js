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
import { resolveInitialRestoreStep } from '../../src/utils/restoreStepper.js'

describe('resolveInitialRestoreStep', () => {
  it('never auto-advances without a deep-link jobid, even if steps are already done', () => {
    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: false,
      sourceStepDone: false,
      filesStepDone: false,
      destinationStepDone: false,
    })).toBe(1)

    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: false,
      sourceStepDone: true,
      filesStepDone: true,
      destinationStepDone: true,
    })).toBe(1)
  })

  it('stays on step 1 for a deep link whose prerequisites are not yet satisfied', () => {
    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: true,
      sourceStepDone: false,
      filesStepDone: false,
      destinationStepDone: false,
    })).toBe(1)
  })

  it('advances to Files to restore for a deep link with only the source step done', () => {
    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: true,
      sourceStepDone: true,
      filesStepDone: false,
      destinationStepDone: false,
    })).toBe(2)
  })

  it('advances to Restore target for a deep link with source and files done', () => {
    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: true,
      sourceStepDone: true,
      filesStepDone: true,
      destinationStepDone: false,
    })).toBe(3)
  })

  it('advances to Review & run for a deep link with all steps done', () => {
    expect(resolveInitialRestoreStep({
      hasDeepLinkJobid: true,
      sourceStepDone: true,
      filesStepDone: true,
      destinationStepDone: true,
    })).toBe(4)
  })
})
