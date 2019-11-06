/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_INCLUDE_JOB_LEVEL_H_
#define BAREOS_SRC_INCLUDE_JOB_LEVEL_H_

enum JobLevelCode : char
{
  L_FULL = 'F',         /**< Full backup */
  L_INCREMENTAL = 'I',  /**< Since last backup */
  L_DIFFERENTIAL = 'D', /**< Since last full backup */
  L_SINCE = 'S',
  L_VERIFY_CATALOG = 'C',           /**< Verify from catalog */
  L_VERIFY_INIT = 'V',              /**< Verify save (init DB) */
  L_VERIFY_VOLUME_TO_CATALOG = 'O', /**< Verify Volume to catalog entries \
                                     */
  L_VERIFY_DISK_TO_CATALOG = 'd',   /**< Verify Disk attributes to catalog */
  L_VERIFY_DATA = 'A',              /**< Verify data on volume */
  L_BASE = 'B',                     /**< Base level job */
  L_NONE = ' ',                     /**< None, for Restore and Admin */
  L_VIRTUAL_FULL = 'f'              /**< Virtual full backup */
};

#endif  // BAREOS_SRC_INCLUDE_JOB_LEVEL_H_
