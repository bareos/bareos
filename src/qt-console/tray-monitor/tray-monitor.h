/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Includes specific to the tray monitor
 *
 *     Nicolas Boichat, August MMIV
 *
 */

#ifndef TRAY_MONITOR_H
#define TRAY_MONITOR_H

#ifdef HAVE_WIN32
# ifndef _STAT_DEFINED
#  define _STAT_DEFINED 1 /* don't pull in MinGW struct stat from wchar.h */
# endif
#endif

#include <QString>
#include <QStringList>

#include "bacula.h"
#include "tray_conf.h"
#include "jcr.h"


struct job_defaults {
   QString job_name;
   QString pool_name;
   QString messages_name;
   QString client_name;
   QString store_name;
   QString where;
   QString level;
   QString type;
   QString fileset_name;
   QString catalog_name;
   bool enabled;
};

struct resources {
   QStringList job_list;
   QStringList pool_list;
   QStringList client_list;
   QStringList storage_list;
   QStringList levels;
   QStringList fileset_list;
   QStringList messages_list;
};

enum stateenum {
   idle = 0,
   running = 1,
   warn = 2,
   error = 3
};

class monitoritem;
int doconnect(monitoritem* item);
void get_list(monitoritem* item, const char *cmd, QStringList &lst);

class  monitoritem {
public:
   rescode type; /* R_DIRECTOR, R_CLIENT or R_STORAGE */
   void* resource; /* DIRRES*, CLIENT* or STORE* */
   BSOCK *D_sock;
   stateenum state;
   stateenum oldstate;

   char *get_name() {
      return ((URES*)resource)->hdr.name;
   }

   void writecmd(const char* command) {
      if (this->D_sock) {
         this->D_sock->msglen = pm_strcpy(&this->D_sock->msg, command);
         bnet_send(this->D_sock);
      }
   }

   bool get_job_defaults(struct job_defaults &job_defs)
   {
      int stat;
      char *def;
      BSOCK *dircomm;
      bool rtn = false;
      QString scmd = QString(".defaults job=\"%1\"").arg(job_defs.job_name);

      if (job_defs.job_name == "") {
         return rtn;
      }

      if (!doconnect(this)) {
         return rtn;
      }
      dircomm = this->D_sock;
      dircomm->fsend("%s", scmd.toUtf8().data());

      while ((stat = dircomm->recv()) > 0) {
         def = strchr(dircomm->msg, '=');
         if (!def) {
            continue;
         }
         /* Pointer to default value */
         *def++ = 0;
         strip_trailing_junk(def);
         
         if (strcmp(dircomm->msg, "job") == 0) {
            if (strcmp(def, job_defs.job_name.toUtf8().data()) != 0) {
               goto bail_out;
            }
            continue;
         }
         if (strcmp(dircomm->msg, "pool") == 0) {
            job_defs.pool_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "messages") == 0) {
            job_defs.messages_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "client") == 0) {
            job_defs.client_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "storage") == 0) {
            job_defs.store_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "where") == 0) {
            job_defs.where = def;
            continue;
         }
         if (strcmp(dircomm->msg, "level") == 0) {
            job_defs.level = def;
            continue;
         }
         if (strcmp(dircomm->msg, "type") == 0) {
            job_defs.type = def;
            continue;
         }
         if (strcmp(dircomm->msg, "fileset") == 0) {
            job_defs.fileset_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "catalog") == 0) {
            job_defs.catalog_name = def;
            continue;
         }
         if (strcmp(dircomm->msg, "enabled") == 0) {
            job_defs.enabled = *def == '1' ? true : false;
            continue;
         }
      }
      rtn = true;
      /* Fall through wanted */
   bail_out:
      return rtn;
   }
};

#endif  /* TRAY_MONITOR_H */
