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

#ifndef BAREOS_DIRD_MSGCHAN_H_
#define BAREOS_DIRD_MSGCHAN_H_

bool StartStorageDaemonJob(JobControlRecord *jcr, alist *rstore, alist *wstore,
                              bool send_bsr = false);
bool StartStorageDaemonMessageThread(JobControlRecord *jcr);
int BgetDirmsg(BareosSocket *bs, bool allow_any_msg = false);
void WaitForStorageDaemonTermination(JobControlRecord *jcr);

#endif // BAREOS_DIRD_MSGCHAN_H_
