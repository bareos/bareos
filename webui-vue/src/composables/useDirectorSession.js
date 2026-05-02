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

import { useAuthStore } from '../stores/auth.js'
import { useDirectorStore } from '../stores/director.js'
import { useSettingsStore } from '../stores/settings.js'

export async function switchActiveDirector(targetDirector) {
  const auth = useAuthStore()
  const director = useDirectorStore()
  const settings = useSettingsStore()

  if (!targetDirector) {
    throw new Error('No director selected.')
  }

  const previousCredentials = auth.getCredentials()
  const previousDirector = previousCredentials?.director

  if (!previousCredentials?.password) {
    throw new Error('Not logged in.')
  }

  if (previousDirector === targetDirector && director.isConnected) {
    return false
  }

  director.disconnect()
  auth.setDirector(targetDirector)
  settings.directorName = targetDirector

  try {
    await director.connectAndWait(auth.getCredentials())
    return true
  } catch (error) {
    if (previousDirector && previousDirector !== targetDirector) {
      auth.setDirector(previousDirector)
      settings.directorName = previousDirector
      try {
        await director.connectAndWait(previousCredentials)
      } catch {
        // Preserve the original switch error.
      }
    }
    throw error
  }
}
