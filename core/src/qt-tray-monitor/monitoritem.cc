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
#include "lib/bsock_tcp.h"
#include "lib/bnet.h"

MonitorItem::MonitorItem(QObject* parent)
    : QObject(parent), d(new MonitorItemPrivate)
{
  /* Run this class only in the context of
     MonitorItemThread because of the networking */
  Q_ASSERT(QThread::currentThreadId() ==
           MonitorItemThread::instance()->getThreadId());
}

MonitorItem::~MonitorItem() { delete d; }

char* MonitorItem::get_name() const { return d->resource->resource_name_; }

void MonitorItem::writecmd(const char* command)
{
  if (d->DSock) {
    d->DSock->message_length = PmStrcpy(d->DSock->msg, command);
    BnetSend(d->DSock);
  }
}

bool MonitorItem::GetJobDefaults(struct JobDefaults& job_defs)
{
  int stat;
  char* def;
  BareosSocket* dircomm;
  QString scmd = QString(".defaults job=\"%1\"").arg(job_defs.job_name);

  if (job_defs.job_name == "" || !doconnect()) { return false; }

  dircomm = d->DSock;
  dircomm->fsend("%s", scmd.toUtf8().data());

  while ((stat = dircomm->recv()) > 0) {
    def = strchr(dircomm->msg, '=');

    if (def) {
      /* Pointer to default value */
      *def++ = 0;
      StripTrailingNewline(def);

      if (strcmp(dircomm->msg, "job") == 0) {
        if (strcmp(def, job_defs.job_name.toUtf8().data()) != 0) return false;
      } else if (strcmp(dircomm->msg, "pool") == 0) {
        job_defs.pool_name = def;
      } else if (strcmp(dircomm->msg, "messages") == 0) {
        job_defs.messages_name = def;
      } else if (strcmp(dircomm->msg, "client") == 0) {
        job_defs.client_name = def;
      } else if (strcmp(dircomm->msg, "storage") == 0) {
        job_defs.StoreName = def;
      } else if (strcmp(dircomm->msg, "where") == 0) {
        job_defs.where = def;
      } else if (strcmp(dircomm->msg, "level") == 0) {
        job_defs.level = def;
      } else if (strcmp(dircomm->msg, "type") == 0) {
        job_defs.type = def;
      } else if (strcmp(dircomm->msg, "fileset") == 0) {
        job_defs.fileset_name = def;
      } else if (strcmp(dircomm->msg, "catalog") == 0) {
        job_defs.catalog_name = def;
        job_defs.enabled = *def == '1' ? true : false;
      }
    }
  }
  return true;
}

bool MonitorItem::doconnect()
{
  if (d->DSock) {
    // already connected
    return true;
  }

  JobControlRecord jcr;
  memset(&jcr, 0, sizeof(jcr));

  DirectorResource* dird;
  ClientResource* filed;
  StorageResource* stored;
  QString message;

  switch (d->type) {
    case R_DIRECTOR:
      dird = static_cast<DirectorResource*>(d->resource);
      message = QString("Connecting to Director %1:%2")
                    .arg(dird->address)
                    .arg(dird->DIRport);
      emit showStatusbarMessage(message);
      d->DSock = new BareosSocketTCP;
      if (!d->DSock->connect(NULL, d->connectTimeout, 0, 0, "Director daemon",
                             dird->address, NULL, dird->DIRport, false)) {
        delete d->DSock;
        d->DSock = NULL;
      } else {
        jcr.dir_bsock = d->DSock;
      }
      break;

    case R_CLIENT:
      filed = static_cast<ClientResource*>(d->resource);
      message = QString("Connecting to Client %1:%2")
                    .arg(filed->address)
                    .arg(filed->FDport);
      emit showStatusbarMessage(message);
      d->DSock = new BareosSocketTCP;
      if (!d->DSock->connect(NULL, d->connectTimeout, 0, 0, "File daemon",
                             filed->address, NULL, filed->FDport, false)) {
        delete d->DSock;
        d->DSock = NULL;
      } else {
        jcr.file_bsock = d->DSock;
      }
      break;

    case R_STORAGE:
      stored = static_cast<StorageResource*>(d->resource);
      message = QString("Connecting to Storage %1:%2")
                    .arg(stored->address)
                    .arg(stored->SDport);
      emit showStatusbarMessage(message);
      d->DSock = new BareosSocketTCP;
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

  char* name = get_name();

  if (d->DSock == NULL) {
    emit showStatusbarMessage("Cannot connect to daemon.");
    emit clearText(name);
    emit appendText(name, QString("Cannot connect to daemon."));
    d->state = MonitorItem::Error;
    emit statusChanged(name, d->state);
    return false;
  }

  AuthenticationResult auth_result = AuthenticateWithDaemon(this, &jcr);

  bool authentication_ok =
      ((auth_result == AuthenticationResult::kAlreadyAuthenticated) ||
       (auth_result == AuthenticationResult::kNoError));

  if (!authentication_ok) {
    d->state = MonitorItem::Error;
    emit statusChanged(name, d->state);
    std::string err_buffer;
    if (GetAuthenticationResultString(auth_result, err_buffer)) {
      message = QString("Authentication error : %1").arg(err_buffer.c_str());
    } else {
      message = QString("Authentication error : %1").arg("Unknown");
    }
    emit showStatusbarMessage(message);
    emit clearText(name);
    emit appendText(name, message);
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
      emit showStatusbarMessage(
          "Error, currentitem is not a Client, a Storage or a Director..\n");
      d->state = Error;
      emit statusChanged(name, d->state);
      return false;
  }

  d->state = Running;
  emit statusChanged(name, d->state);

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
  if (!doconnect()) { return false; }

  if (command[0] != 0) { writecmd(command); }

  emit clearText(get_name());
  bool jobRunning = false;

  while (1) {
    int stat;
    if ((stat = BnetRecv(d->DSock)) >= 0) {
      StripTrailingNewline(d->DSock->msg);
      QString msg = QString::fromUtf8(d->DSock->msg);
      emit appendText(QString::fromUtf8(get_name()), msg);
      if (d->type == R_CLIENT) {
        if (msg.contains("Job started:")) jobRunning = true;
      }
    } else if (stat == BNET_SIGNAL) {
      if (d->DSock->message_length == BNET_EOD) {
        // qDebug() << "<< EOD >>";
        if (d->type == R_CLIENT) emit jobIsRunning(jobRunning);
        return true;
      } else if (d->DSock->message_length == BNET_SUB_PROMPT) {
        // qDebug() << "<< PROMPT >>";
        return false;
      } else if (d->DSock->message_length == BNET_HEARTBEAT) {
        BnetSig(d->DSock, BNET_HB_RESPONSE);
      } else {
        qDebug() << BnetSigToAscii(d->DSock);
      }
    } else { /* BNET_HARDEOF || BNET_ERROR */
      d->DSock = NULL;
      d->state = MonitorItem::Error;
      emit statusChanged(get_name(), d->state);
      emit showStatusbarMessage("Error : BNET_HARDEOF or BNET_ERROR");
      // fprintf(stderr, "<< ERROR >>\n"));
      return false;
    } /* if ((stat = BnetRecv(d->DSock)) >= 0) */

    if (IsBnetStop(d->DSock)) {
      d->DSock = NULL;
      d->state = MonitorItem::Error;
      emit statusChanged(get_name(), d->state);
      emit showStatusbarMessage("Error : Connection closed.");
      // fprintf(stderr, "<< STOP >>\n");
      return false; /* error or term */
    }               /* if (IsBnetStop(d->DSock) */

  } /* while (1) */
}

void MonitorItem::get_list(const char* cmd, QStringList& lst)
{
  doconnect();
  writecmd(cmd);
  while (BnetRecv(d->DSock) >= 0) {
    StripTrailingNewline(d->DSock->msg);
    if (*(d->DSock->msg)) { lst << QString(d->DSock->msg); }
  }
}

void MonitorItem::GetStatus()
{
  switch (d->type) {
    case R_DIRECTOR:
      docmd("status dir");
      break;
    case R_CLIENT:
      if (!docmd("status")) emit jobIsRunning(false);
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
  connect(this, SIGNAL(showStatusbarMessage(QString)), mainWindow,
          SLOT(onShowStatusbarMessage(QString)));
  connect(this, SIGNAL(appendText(QString, QString)), mainWindow,
          SLOT(onAppendText(QString, QString)));
  connect(this, SIGNAL(clearText(QString)), mainWindow,
          SLOT(onClearText(QString)));
  connect(this, SIGNAL(statusChanged(QString, int)), mainWindow,
          SLOT(onStatusChanged(QString, int)));
  if (d->type == R_CLIENT) {
    connect(this, SIGNAL(jobIsRunning(bool)), mainWindow,
            SLOT(onFdJobIsRunning(bool)));
  }
}

Rescode MonitorItem::type() const { return d->type; }
void* MonitorItem::resource() const { return d->resource; }
BareosSocket* MonitorItem::DSock() const { return d->DSock; }
MonitorItem::StateEnum MonitorItem::state() const { return d->state; }
int MonitorItem::connectTimeout() const { return d->connectTimeout; }

void MonitorItem::setType(Rescode type) { d->type = type; }
void MonitorItem::setResource(BareosResource* resource)
{
  d->resource = resource;
}
void MonitorItem::setDSock(BareosSocket* DSock) { d->DSock = DSock; }
void MonitorItem::setState(MonitorItem::StateEnum state) { d->state = state; }
void MonitorItem::setConnectTimeout(int timeout)
{
  d->connectTimeout = timeout;
}
