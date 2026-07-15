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
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_QT_TRAY_MONITOR_RESOURCES_H_
#define BAREOS_QT_TRAY_MONITOR_RESOURCES_H_

constexpr inline const char* kRes_LogoIcon = ":/images/bareos-logo.svg";
constexpr inline const char* kRes_LogoBGIcon
    = ":/images/bareos-logo-background.svg";
constexpr inline const char* kRes_RunningSpriteSheet
    = ":/images/tray-running-sprite.png";
constexpr inline int kRes_RunningFrameCount = 12;
constexpr inline int kRes_RunningFrameWidth = 128;
constexpr inline int kRes_RunningFrameHeight = 128;
constexpr inline const char* kRes_ErrorIcon = ":/images/warning.png";
constexpr inline const char* kRes_FailureIcon = ":/images/failure.png";

#endif  // BAREOS_QT_TRAY_MONITOR_RESOURCES_H_
