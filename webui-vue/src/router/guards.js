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

export function resolveSetupNavigation(auth, setup, to) {
  if (setup.needsSetup) {
    if (auth.isLoggedIn) {
      auth.logout()
    }
    if (to.name !== 'setup') {
      return { name: 'setup' }
    }
    return undefined
  }

  if (!auth.isLoggedIn && setup.isReady && to.name === 'setup') {
    return { name: 'login' }
  }

  if (to.meta.requiresAuth && !auth.isLoggedIn) {
    return { name: 'login' }
  }
  if ((to.name === 'login' || to.name === 'setup') && auth.isLoggedIn) {
    return { name: 'dashboard' }
  }

  return undefined
}
