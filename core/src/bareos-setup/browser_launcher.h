/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#ifndef BAREOS_BAREOS_SETUP_BROWSER_LAUNCHER_H_
#define BAREOS_BAREOS_SETUP_BROWSER_LAUNCHER_H_

#include <string>

#include "command_runner.h"

/** Try the available system browser launchers for a setup URL. */
bool TryOpenBrowser(SetupContext& context, const std::string& url);

#endif  // BAREOS_BAREOS_SETUP_BROWSER_LAUNCHER_H_
