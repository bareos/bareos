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

// Decides which step of the Restore wizard (Backup source /
// Files to restore / Restore target / Review & run) should be shown right
// after the page's initial load completes.
//
// Normal interactive use must never auto-advance the stepper: the user
// always has to click "Continue" to move forward, even once a step's
// prerequisites happen to be satisfied (e.g. selecting a source job also
// auto-defaults the destination client/job, but that alone must not jump
// the user past "Restore target").
//
// The one exception is a genuine deep link (the page opened with a
// `jobid` query parameter, e.g. via a "Restore" link from a job's
// details page): in that case it's fine to land past whichever steps are
// already satisfied, since the user already made an explicit choice
// elsewhere. This function is intended to be called exactly once, right
// after the initial route-driven form population has finished.
export function resolveInitialRestoreStep({
  hasDeepLinkJobid,
  sourceStepDone,
  filesStepDone,
  destinationStepDone,
}) {
  if (!hasDeepLinkJobid) {
    return 1
  }
  if (sourceStepDone && filesStepDone && destinationStepDone) {
    return 4
  }
  if (sourceStepDone && filesStepDone) {
    return 3
  }
  if (sourceStepDone) {
    return 2
  }
  return 1
}
