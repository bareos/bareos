
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
#ifndef STORED_LABEL_H_
#define STORED_LABEL_H_

int read_dev_volume_label(DeviceControlRecord *dcr);
void create_volume_label(Device *dev, const char *VolName, const char *PoolName);

#define ANSI_VOL_LABEL 0
#define ANSI_EOF_LABEL 1
#define ANSI_EOV_LABEL 2

bool write_ansi_ibm_labels(DeviceControlRecord *dcr, int type, const char *VolName);
int read_ansi_ibm_label(DeviceControlRecord *dcr);
bool write_session_label(DeviceControlRecord *dcr, int label);
void dump_volume_label(Device *dev);
void dump_label_record(Device *dev, DeviceRecord *rec, bool verbose);
bool unser_volume_label(Device *dev, DeviceRecord *rec);
bool unser_session_label(SESSION_LABEL *label, DeviceRecord *rec);
bool write_new_volume_label_to_dev(DeviceControlRecord *dcr, const char *VolName,
                                   const char *PoolName, bool relabel);

#endif // STORED_LABEL_H_
