/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
/**
 * @file
 * OS detection: reads /etc/os-release and probes the package manager.
 */
#ifndef BAREOS_BAREOS_SETUP_OS_DETECTOR_H_
#define BAREOS_BAREOS_SETUP_OS_DETECTOR_H_

#include <string>

struct OsInfo {
  std::string distro;      // ID from os-release, e.g. "ubuntu"
  std::string version;     // VERSION_ID, e.g. "24.04"
  std::string codename;    // VERSION_CODENAME, e.g. "noble"
  std::string pretty_name; // PRETTY_NAME
  std::string arch;        // uname machine, e.g. "x86_64"
  std::string pkg_mgr;     // "apt" | "dnf" | "yum" | "zypper" | "unknown"
};

/** Detect the host OS.  Returns populated OsInfo; throws on fatal errors. */
OsInfo DetectOs();

#endif  // BAREOS_BAREOS_SETUP_OS_DETECTOR_H_
