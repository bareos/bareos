/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
#include <QDebug>
#include <QThread>

#include "monitoritem.h"
#include "authenticate.h"
#include "monitoritemthread.h"

MonitorItem::MonitorItem(QObject* parent)
   : QObject(parent)
   , d(new MonitorItemPrivate)
{
   /* Run this class only in the context of
      MonitorItemThread because of the networking */
   Q_ASSERT(QThread::currentThreadId() == MonitorItemThread::instance()->getThreadId());
}

MonitorItem::~MonitorItem()
{
   delete d;
}

char* MonitorItem::get_name() const
{
   return static_cast<URES*>(d->resource)->hdr.name;
}

void MonitorItem::writecmd(const char* command)
{
   if (d->DSock) {
      d->DSock->msglen = pm_strcpy(&d->DSock->msg, command);
      bnet_send(d->DSock);
   }
}

bool MonitorItem::get_job_defaults(struct JobDefaults &job_defs)
{
   int stat;
   char *def;
   BSOCK *dircomm;
   bool rtn = false;
   QString scmd = QString(".defaults job=\"%1\"").arg(job_defs.job_name);

   if (job_defs.job_name == "") {
      return rtn;
   }

   if (!doconnect()) {
      return rtn;
   }
   dircomm = d->DSock;
   dircomm->fsend("%s", scmd.toUtf8().data());

   while ((stat = dircomm->recv()) > 0) {
      def = strchr(dircomm->msg, '=');
      if (!def) {
         continue;
      }
      /* Pointer to default value */
      *def++ = 0;
      strip_trailing_newline(def);

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

bool MonitorItem::doconnect()
{
   if (d->DSock) {
   //already connected
       return true;
   }

  JCR jcr;
  memset(&jcr, 0, sizeof(jcr));

  DIRRES *dird;
  CLIENTRES *filed;
  STORERES *stored;
  QString message;

  switch (d->type) {
  case R_DIRECTOR:
     dird = static_cast<DIRRES*>(d->resource);
     message = QString("Connecting to Director %1:%2").arg(dird->address).arg(dird->DIRport);
     emit showStatusbarMessage(message);
     d->DSock =  New(BSOCK_TCP);
     if (!d->DSock->connect(NULL, d->connectTimeout, 0, 0, "Director daemon",
                            dird->address, NULL, dird->DIRport, false)) {
        delete d->DSock;
        d->DSock = NULL;
     } else {
        jcr.dir_bsock = d->DSock;
     }
     break;
  case R_CLIENT:
     filed = static_cast<CLIENTRES*>(d->resource);
     message = QString("Connecting to Client %1:%2").arg(filed->address).arg(filed->FDport);
     emit showStatusbarMessage(message);
     d->DSock =  New(BSOCK_TCP);
     if (!d->DSock->connect(NULL, d->connectTimeout, 0, 0, "File daemon",
                            filed->address, NULL, filed->FDport, false)) {
        delete d->DSock;
        d->DSock = NULL;
     } else {
        jcr.file_bsock = d->DSock;
     }
     break;
  case R_STORAGE:
     stored = static_cast<STORERES*>(d->resource);
     message = QString("Connecting to Storage %1:%2").arg(stored->address).arg(stored->SDport);
     emit showStatusbarMessage(message);
     d->DSock =  New(BSOCK_TCP);
     if (!d->DSock->connect(NULL, d->connectTimeout, 0, 0, "Storage daemon",
                            stored->address, NULL, stored->SDport, false)) {
        delete d->DSock;
        d->DSock = NULL;
     } else {
        jcr.store_bsock = d->DSock;
     }
     break;
  default:
     printf("Error, currentitem is not a Client, a Storage or a Director..\n");
     return false;
  }

  if (d->DSock == NULL) {
     emit showStatusbarMessage("Cannot connect to daemon.");
     emit clearText(get_name());
     emit appendText(get_name(), QString("Cannot connect to daemon."));
     d->state = MonitorItem::Error;
     emit statusChanged(get_name(), d->state);
     return false;
  }

  if (!authenticate_daemon(this, &jcr)) {
     d->state = MonitorItem::Error;
     emit statusChanged(get_name(), d->state);
     message = QString("Authentication error : %1").arg(d->DSock->msg);
     emit showStatusbarMessage(message);
     emit clearText(get_name());
     emit appendText(get_name(), QString("Authentication error : %1").arg(d->DSock->msg));
     d->DSock->signal(BNET_TERMINATE); /* send EOF */
     d->DSock->close();
     delete d->DSock;
     d->DSock = NULL;
     return false;
  }

  switch (d->type) {
  case R_DIRECTOR:
     emit showStatusbarMessage("Opened connection with Director daemon.");
     break;
  case R_CLIENT:
     emit showStatusbarMessage("Opened connection with File daemon.");
     break;
  case R_STORAGE:
     emit showStatusbarMessage("Opened connection with Storage daemon.");
     break;
  default:
     emit showStatusbarMessage("Error, currentitem is not a Client, a Storage or a Director..\n");
     d->state = Error;
     emit statusChanged(get_name(), d->state);
     return false;
  }

  if (d->type == R_DIRECTOR) { /* Read connection messages... */
     docmd(""); /* Usually invalid, but no matter */
  }

  d->state = Running;
  emit statusChanged(get_name(), d->state);

  return true;
}

void MonitorItem::disconnect()
{
   if (d->DSock) {
      writecmd("quit");
      d->DSock->signal(BNET_TERMINATE); /* send EOF */
      d->DSock->close();
      delete d->DSock;
      d->DSock = NULL;
   }
}

bool MonitorItem::docmd(const char* command)
{
   if (!doconnect()) {
      return false;
   }

   if (command[0] != 0) {
      writecmd(command);
   }

   emit clearText(get_name());
   bool jobRunning = false;

   while (1) {
      int stat;
      if ((stat = bnet_recv(d->DSock)) >= 0) {
         strip_trailing_newline(d->DSock->msg);
         QString msg = QString::fromUtf8(d->DSock->msg);
         emit appendText(QString::fromUtf8(get_name()), msg);
         if (d->type == R_CLIENT) {
             if (msg.contains("Job started:"))
                jobRunning = true;
         }
      } else if (stat == BNET_SIGNAL) {
         if (d->DSock->msglen == BNET_EOD) {
            // qDebug() << "<< EOD >>";
             if (d->type == R_CLIENT)
                emit jobIsRunning (jobRunning);
            return true;
         }
         else if (d->DSock->msglen == BNET_SUB_PROMPT) {
            // qDebug() << "<< PROMPT >>";
            return false;
         }
         else if (d->DSock->msglen == BNET_HEARTBEAT) {
            bnet_sig(d->DSock, BNET_HB_RESPONSE);
         }
         else {
            qDebug() << bnet_sig_to_ascii(d->DSock);
         }
      } else { /* BNET_HARDEOF || BNET_ERROR */
         d->DSock = NULL;
         d->state = MonitorItem::Error;
         emit statusChanged(get_name(), d->state);
         emit showStatusbarMessage("Error : BNET_HARDEOF or BNET_ERROR");
         //fprintf(stderr, "<< ERROR >>\n"));
         return false;
      } /* if ((stat = bnet_recv(d->DSock)) >= 0) */

      if (is_bnet_stop(d->DSock)) {
         d->DSock = NULL;
         d->state = MonitorItem::Error;
         emit statusChanged(get_name(), d->state);
         emit showStatusbarMessage("Error : Connection closed.");
         //fprintf(stderr, "<< STOP >>\n");
         return false;            /* error or term */
      } /* if (is_bnet_stop(d->DSock) */

   } /* while (1) */
}

void MonitorItem::get_list(const char *cmd, QStringList &lst)
{
   doconnect();
   writecmd(cmd);
   while (bnet_recv(d->DSock) >= 0) {
      strip_trailing_newline(d->DSock->msg);
      if (*(d->DSock->msg)) {
         lst << QString(d->DSock->msg);
      }
   }
}

void MonitorItem::get_status()
{
    switch (d->type) {
        case R_DIRECTOR:
           docmd("status dir");
           break;
        case R_CLIENT:
           if (!docmd("status"))
              emit jobIsRunning(false);
           break;
        case R_STORAGE:
           docmd("status");
           break;
        default:
           break;
    }
}

void MonitorItem::connectToMainWindow(QObject* mainWindow)
{
   connect(this, SIGNAL(showStatusbarMessage(QString)),
           mainWindow, SLOT(onShowStatusbarMessage(QString)));
   connect(this, SIGNAL(appendText(QString,QString)),
           mainWindow, SLOT(onAppendText(QString,QString)));
   connect(this, SIGNAL(clearText(QString)),
           mainWindow, SLOT(onClearText(QString)));
   connect(this, SIGNAL(statusChanged(QString,int)),
           mainWindow, SLOT(onStatusChanged(QString,int)));
   if (d->type == R_CLIENT) {
     connect(this, SIGNAL(jobIsRunning(bool)),
           mainWindow, SLOT(onFdJobIsRunning(bool)));
   }
}

Rescode MonitorItem::type() const { return d->type; }
void* MonitorItem::resource() const { return d->resource; }
BSOCK* MonitorItem::DSock() const { return d->DSock; }
MonitorItem::StateEnum MonitorItem::state() const { return d->state; }
int MonitorItem::connectTimeout() const { return d->connectTimeout; }

void MonitorItem::setType(Rescode type) { d->type = type; }
void MonitorItem::setResource(void* resource) { d->resource = resource; }
void MonitorItem::setDSock(BSOCK* DSock) { d->DSock = DSock; }
void MonitorItem::setState(MonitorItem::StateEnum state) { d->state = state; }
void MonitorItem::setConnectTimeout(int timeout) { d->connectTimeout = timeout; }
