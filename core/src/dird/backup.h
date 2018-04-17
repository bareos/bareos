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

#ifndef DIRD_BACKUP_H_
#define DIRD_BACKUP_H_

int wait_for_job_termination(JobControlRecord *jcr, int timeout = 0);
bool do_native_backup_init(JobControlRecord *jcr);
bool do_native_backup(JobControlRecord *jcr);
void native_backup_cleanup(JobControlRecord *jcr, int TermCode);
void update_bootstrap_file(JobControlRecord *jcr);
bool send_accurate_current_files(JobControlRecord *jcr);
void generate_backup_summary(JobControlRecord *jcr, ClientDbRecord *cr, int msg_type,
                             const char *term_msg);

char* storage_address_to_contact(ClientResource *client, StoreResource *store);
char* client_address_to_contact(ClientResource *client, StoreResource *store);
char* storage_address_to_contact(StoreResource *rstore, StoreResource *wstore);

#endif // DIRD_BACKUP_H_
