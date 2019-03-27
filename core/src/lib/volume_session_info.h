/*
 * BAREOS® - Backup Archiving REcovery Open Sourced
 *
 * Copyright (C) 2019-2019 Bareos GmbH & Co. KG
 *
 * This program is Free Software; you can redistribute it and/or modify
 * it under the terms of version three of the GNU Affero General Public License
 * as published by the Free Software Foundation and included in the file
 * LICENSE.
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details. You should have received a copy of the GNU Affero General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 */

/*
 * provides a struct to wrap a pair of VolumeSessionId and VolumeSessionTime
 */

#ifndef BAREOS_LIB_VOLUME_SESSION_INFO_H_
#define BAREOS_LIB_VOLUME_SESSION_INFO_H_ 1

struct VolumeSessionInfo {
  uint32_t id;
  uint32_t time;

  /* explicit constructor disables default construction */
  VolumeSessionInfo(uint32_t t_id, uint32_t t_time) : id(t_id), time(t_time) {}
};

#endif /**  BAREOS_LIB_VOLUME_SESSION_INFO_H_ */
