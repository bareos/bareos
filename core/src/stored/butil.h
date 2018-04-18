
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef STORED_BUTIL_H_
#define STORED_BUTIL_H_

void print_ls_output(const char *fname, const char *link, int type, struct stat *statp);
JobControlRecord *setup_jcr(const char *name, char *dev_name,
               BootStrapRecord *bsr, DirectorResource *director, DeviceControlRecord* dcr,
               const char *VolumeName, bool readonly);
void display_tape_error_status(JobControlRecord *jcr, Device *dev);

#endif // STORED_BUTIL_H_
