/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
 *  DirComm, Director communications,class
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"
#include "console.h"
#include "restore.h"
#include "select.h"
#include "textinput.h"
#include "run/run.h"

static int tls_pem_callback(char *buf, int size, const void *userdata);

DirComm::DirComm(Console *parent, int conn):  m_notifier(NULL),  m_api_set(false)
{
   m_console = parent;
   m_sock = NULL;
   m_at_prompt = false;
   m_at_main_prompt = false;
   m_sent_blank = false;
   m_conn = conn;
   m_in_command = 0;
   m_in_select = false;
   m_notify = false;
}

DirComm::~DirComm()
{
}

/* Terminate any open socket */
void DirComm::terminate()
{
   if (m_sock) {
      if (m_notifier) {
         m_notifier->setEnabled(false);
         delete m_notifier;
         m_notifier = NULL;
         m_notify = false;
      }
      if (mainWin->m_connDebug)
         Pmsg2(000, "DirComm %i terminating connections %s\n", m_conn, m_console->m_dir->name());
      m_sock->close();
      m_sock = NULL;
   }
}

/*
 * Connect to Director. 
 */
bool DirComm::connect_dir()
{
   JCR *jcr = new JCR;
   utime_t heart_beat;
   char buf[1024];
   CONRES *cons;
      
   buf[0] = 0;

   if (m_sock) {
      mainWin->set_status( tr("Already connected."));
      m_console->display_textf(_("Already connected\"%s\".\n"),
            m_console->m_dir->name());
      if (mainWin->m_connDebug) {
         Pmsg2(000, "DirComm %i BAILING already connected %s\n", m_conn, m_console->m_dir->name());
      }
      goto bail_out;
   }

   if (mainWin->m_connDebug)Pmsg2(000, "DirComm %i connecting %s\n", m_conn, m_console->m_dir->name());
   memset(jcr, 0, sizeof(JCR));

   mainWin->set_statusf(_("Connecting to Director %s:%d"), m_console->m_dir->address, m_console->m_dir->DIRport);
   if (m_conn == 0) {
      m_console->display_textf(_("Connecting to Director %s:%d\n\n"), m_console->m_dir->address, m_console->m_dir->DIRport);
   }

   /* Give GUI a chance */
   app->processEvents();
   
   LockRes();
   /* If cons==NULL, default console will be used */
   cons = (CONRES *)GetNextRes(R_CONSOLE, NULL);
   UnlockRes();

   /* Initialize Console TLS context once */
   if (cons && !cons->tls_ctx && (cons->tls_enable || cons->tls_require)) {
      /* Generate passphrase prompt */
      bsnprintf(buf, sizeof(buf), "Passphrase for Console \"%s\" TLS private key: ", 
                cons->name());

      /* Initialize TLS context:
       * Args: CA certfile, CA certdir, Certfile, Keyfile,
       * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer   
       */
      cons->tls_ctx = new_tls_context(cons->tls_ca_certfile,
         cons->tls_ca_certdir, cons->tls_certfile,
         cons->tls_keyfile, tls_pem_callback, &buf, NULL, true);

      if (!cons->tls_ctx) {
         m_console->display_textf(_("Failed to initialize TLS context for Console \"%s\".\n"),
            m_console->m_dir->name());
         if (mainWin->m_connDebug) {
            Pmsg2(000, "DirComm %i BAILING Failed to initialize TLS context for Console %s\n", m_conn, m_console->m_dir->name());
         }
         goto bail_out;
      }
   }

   /* Initialize Director TLS context once */
   if (!m_console->m_dir->tls_ctx && (m_console->m_dir->tls_enable || m_console->m_dir->tls_require)) {
      /* Generate passphrase prompt */
      bsnprintf(buf, sizeof(buf), "Passphrase for Director \"%s\" TLS private key: ", 
                m_console->m_dir->name());

      /* Initialize TLS context:
       * Args: CA certfile, CA certdir, Certfile, Keyfile,
       * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
      m_console->m_dir->tls_ctx = new_tls_context(m_console->m_dir->tls_ca_certfile,
                          m_console->m_dir->tls_ca_certdir, m_console->m_dir->tls_certfile,
                          m_console->m_dir->tls_keyfile, tls_pem_callback, &buf, NULL, true);

      if (!m_console->m_dir->tls_ctx) {
         m_console->display_textf(_("Failed to initialize TLS context for Director \"%s\".\n"),
            m_console->m_dir->name());
         mainWin->set_status("Connection failed");
         if (mainWin->m_connDebug) {
            Pmsg2(000, "DirComm %i BAILING Failed to initialize TLS context for Director %s\n", m_conn, m_console->m_dir->name());
         }
         goto bail_out;
      }
   }

   if (m_console->m_dir->heartbeat_interval) {
      heart_beat = m_console->m_dir->heartbeat_interval;
   } else if (cons) {
      heart_beat = cons->heartbeat_interval;
   } else {
      heart_beat = 0;
   }        

   m_sock = bnet_connect(NULL, 5, 15, heart_beat,
                          _("Director daemon"), m_console->m_dir->address,
                          NULL, m_console->m_dir->DIRport, 0);
   if (m_sock == NULL) {
      mainWin->set_status("Connection failed");
      if (mainWin->m_connDebug) {
         Pmsg2(000, "DirComm %i BAILING Connection failed %s\n", m_conn, m_console->m_dir->name());
      }
      goto bail_out;
   } else {
      /* Update page selector to green to indicate that Console is connected */
      mainWin->actionConnect->setIcon(QIcon(":images/connected.png"));
      QBrush greenBrush(Qt::green);
      QTreeWidgetItem *item = mainWin->getFromHash(m_console);
      if (item) {
         item->setForeground(0, greenBrush);
      }
   }

   jcr->dir_bsock = m_sock;

   if (!authenticate_director(jcr, m_console->m_dir, cons, buf, sizeof(buf))) {
      m_console->display_text(buf);
      if (mainWin->m_connDebug) {
         Pmsg2(000, "DirComm %i BAILING Connection failed %s\n", m_conn, m_console->m_dir->name());
      }
      goto bail_out;
   }

   if (buf[0]) {
      m_console->display_text(buf);
   }

   /* Give GUI a chance */
   app->processEvents();

   mainWin->set_status(_("Initializing ..."));

   /* 
    * Set up input notifier
    */
   m_notifier = new QSocketNotifier(m_sock->m_fd, QSocketNotifier::Read, 0);
   QObject::connect(m_notifier, SIGNAL(activated(int)), this, SLOT(notify_read_dir(int)));
   m_notifier->setEnabled(true);
   m_notify = true;

   write(".api 1");
   m_api_set = true;
   m_console->displayToPrompt(m_conn);

   m_console->beginNewCommand(m_conn);

   mainWin->set_status(_("Connected"));

   if (mainWin->m_connDebug) {
      Pmsg2(000, "Returning TRUE from DirComm->connect_dir : %i %s\n", m_conn, m_console->m_dir->name());
   }
   return true;

bail_out:
   if (mainWin->m_connDebug) {
      Pmsg2(000, "Returning FALSE from DirComm->connect_dir : %i %s\n", m_conn, m_console->m_dir->name());
   }
   delete jcr;
   return false;
}

/* 
 * This should be moved into a bSocket class 
 */
char *DirComm::msg()
{
   if (m_sock) {
      return m_sock->msg;
   }
   return NULL;
}

int DirComm::write(const QString msg)
{
   return write(msg.toUtf8().data());
}

int DirComm::write(const char *msg)
{
   if (!m_sock) {
      return -1;
   }
   m_sock->msglen = pm_strcpy(m_sock->msg, msg);
   m_at_prompt = false;
   m_at_main_prompt = false;
   if (mainWin->m_commDebug) Pmsg2(000, "conn %i send: %s\n", m_conn, msg);
   /*
    * Ensure we send only one blank line.  Multiple blank lines are
    *  simply discarded, it keeps the console output looking nicer.
    */
   if (m_sock->msglen == 0 || (m_sock->msglen == 1 && *m_sock->msg == '\n')) {
      if (!m_sent_blank) {
         m_sent_blank = true;
         return m_sock->send();
      } else {
         return -1;             /* discard multiple blanks */
      }
   }
   m_sent_blank = false;        /* clear flag */
   return m_sock->send();
}

int DirComm::sock_read()
{
   int stat;
#ifdef HAVE_WIN32
   bool wasEnabled = notify(false);
   stat = m_sock->recv();
   notify(wasEnabled);
#else
   stat = m_sock->recv();
#endif
   return stat;
}

/* 
 * Blocking read from director
 */
int DirComm::read()
{
   int stat = -1;

   if (!m_sock) {
      return -1;
   }
   while (m_sock) {
      for (;;) {
         if (!m_sock) break;
         stat = m_sock->wait_data_intr(0, 50000);
         if (stat > 0) {
            break;
         } 
         app->processEvents();
         if (m_api_set && m_console->is_messagesPending() && is_notify_enabled() && m_console->hasFocus()) {
            if (mainWin->m_commDebug) Pmsg1(000, "conn %i process_events\n", m_conn);
            m_console->messagesPending(false);
            m_console->write_dir(m_conn, ".messages", false);
         }
      }
      if (!m_sock) {
         return -1;
      }
      m_sock->msg[0] = 0;
      stat = sock_read();
      if (stat >= 0) {
         if (mainWin->m_commDebug) Pmsg2(000, "conn %i got: %s\n", m_conn, m_sock->msg);
         if (m_at_prompt) {
            m_console->display_text("\n");
            m_at_prompt = false;
            m_at_main_prompt = false;
         }
      }
      switch (m_sock->msglen) {
      case BNET_MSGS_PENDING :
         if (is_notify_enabled() && m_console->hasFocus()) {
            m_console->messagesPending(false);
            if (mainWin->m_commDebug) Pmsg1(000, "conn %i MSGS PENDING\n", m_conn);
            m_console->write_dir(m_conn, ".messages", false);
            m_console->displayToPrompt(m_conn);
            continue;
         }
         m_console->messagesPending(true);
         continue;
      case BNET_CMD_OK:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i CMD OK\n", m_conn);
         m_at_prompt = false;
         m_at_main_prompt = false;
         if (--m_in_command < 0) {
            m_in_command = 0;
         }
         mainWin->set_status(_("Command completed ..."));
         continue;
      case BNET_CMD_BEGIN:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i CMD BEGIN\n", m_conn);
         m_at_prompt = false;
         m_at_main_prompt = false;
         m_in_command++;
         mainWin->set_status(_("Processing command ..."));
         continue;
      case BNET_MAIN_PROMPT:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i MAIN PROMPT\n", m_conn);
         if (!m_at_prompt && ! m_at_main_prompt) {
            m_at_prompt = true;
            m_at_main_prompt = true;
            mainWin->set_status(_("At main prompt waiting for input ..."));
         }
         break;
      case BNET_SUB_PROMPT:
         if (mainWin->m_commDebug) Pmsg2(000, "conn %i SUB_PROMPT m_in_select=%d\n", m_conn, m_in_select);
         m_at_prompt = true;
         m_at_main_prompt = false;
         mainWin->set_status(_("At prompt waiting for input ..."));
         break;
      case BNET_TEXT_INPUT:
         if (mainWin->m_commDebug) Pmsg4(000, "conn %i TEXT_INPUT at_prompt=%d  m_in_select=%d notify=%d\n", 
               m_conn, m_at_prompt, m_in_select, is_notify_enabled());
         //if (!m_in_select && is_notify_enabled()) {
         if (!m_in_select) {
            mainWin->waitExit();
            new textInputDialog(m_console, m_conn);
         } else {
            if (mainWin->m_commDebug) Pmsg0(000, "!m_in_select && is_notify_enabled\n");
            m_at_prompt = true;
            m_at_main_prompt = false;
            mainWin->set_status(_("At prompt waiting for input ..."));
         }
         break;
      case BNET_CMD_FAILED:
         if (mainWin->m_commDebug) Pmsg1(000, "CMD FAILED\n", m_conn);
         if (--m_in_command < 0) {
            m_in_command = 0;
         }
         mainWin->set_status(_("Command failed."));
         break;
      /* We should not get this one */
      case BNET_EOD:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i EOD\n", m_conn);
         mainWin->set_status_ready();
         if (!m_api_set) {
            break;
         }
         continue;
      case BNET_START_SELECT:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i START SELECT\n", m_conn);
         m_in_select = true;
         new selectDialog(m_console, m_conn);
         m_in_select = false;
         break;
      case BNET_YESNO:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i YESNO\n", m_conn);
         new yesnoPopUp(m_console, m_conn);
         break;
      case BNET_RUN_CMD:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i RUN CMD\n", m_conn);
         new runCmdPage(m_conn);
         break;
      case BNET_START_RTREE:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i START RTREE CMD\n", m_conn);
         new restorePage(m_conn);
         break;
      case BNET_END_RTREE:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i END RTREE CMD\n", m_conn);
         break;
      case BNET_ERROR_MSG:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i ERROR MSG\n", m_conn);
         stat = sock_read();          /* get the message */
         m_console->display_text(msg());
         QMessageBox::critical(m_console, "Error", msg(), QMessageBox::Ok);
         m_console->beginNewCommand(m_conn);
         mainWin->waitExit();
         break;
      case BNET_WARNING_MSG:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i WARNING MSG\n", m_conn);
         stat = sock_read();          /* get the message */
         if (!m_console->m_warningPrevent) {
            QMessageBox::critical(m_console, "Warning", msg(), QMessageBox::Ok);
         }
         break;
      case BNET_INFO_MSG:
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i INFO MSG\n", m_conn);
         stat = sock_read();          /* get the message */
         m_console->display_text(msg());
         mainWin->set_status(msg());
         break;
      }

      if (!m_sock) {
         stat = BNET_HARDEOF;
         return stat;
      }
      if (is_bnet_stop(m_sock)) {         /* error or term request */
         if (mainWin->m_commDebug) Pmsg1(000, "conn %i BNET STOP\n", m_conn);
         m_console->stopTimer();
         m_sock->close();
         m_sock = NULL;
         mainWin->actionConnect->setIcon(QIcon(":images/disconnected.png"));
         QBrush redBrush(Qt::red);
         QTreeWidgetItem *item = mainWin->getFromHash(m_console);
         item->setForeground(0, redBrush);
         if (m_notifier) {
            m_notifier->setEnabled(false);
            delete m_notifier;
            m_notifier = NULL;
            m_notify = false;
         }
         mainWin->set_status(_("Director disconnected."));
         stat = BNET_HARDEOF;
      }
      break;
   } 
   return stat;
}

/* Called by signal when the Director has output for us */
void DirComm::notify_read_dir(int /* fd */)
{
   int stat;
   if (!mainWin->m_notify) {
      return;
   }
   if (mainWin->m_commDebug) Pmsg1(000, "enter read_dir conn %i read_dir\n", m_conn);
   stat = m_sock->wait_data(0, 5000);
   if (stat > 0) {
      if (mainWin->m_commDebug) Pmsg2(000, "read_dir conn %i stat=%d\n", m_conn, stat);
      while (read() >= 0) {
         m_console->display_text(msg());
      }
   }
   if (mainWin->m_commDebug) Pmsg2(000, "exit read_dir conn %i stat=%d\n", m_conn, stat);
}

/*
 * When the notifier is enabled, read_dir() will automatically be
 * called by the Qt event loop when ever there is any output 
 * from the Director, and read_dir() will then display it on
 * the console.
 *
 * When we are in a bat dialog, we want to control *all* output
 * from the Directory, so we set notify to off.
 *    m_console->notify(false);
 */
bool DirComm::notify(bool enable) 
{ 
   bool prev_enabled = false;
   /* Set global flag */
   mainWin->m_notify = enable;
   if (m_notifier) {
      prev_enabled = m_notifier->isEnabled();   
      m_notifier->setEnabled(enable);
      m_notify = enable;
      if (mainWin->m_connDebug) Pmsg3(000, "conn=%i set_notify=%d prev=%d\n", m_conn, enable, prev_enabled);
   } else if (mainWin->m_connDebug) {
      Pmsg2(000, "m_notifier does not exist: %i %s\n", m_conn, m_console->m_dir->name());
   }
   return prev_enabled;
}

bool DirComm::is_notify_enabled() const
{
   return m_notify;
}

/*
 * Call-back for reading a passphrase for an encrypted PEM file
 * This function uses getpass(), 
 *  which uses a static buffer and is NOT thread-safe.
 */
static int tls_pem_callback(char *buf, int size, const void *userdata)
{
   (void)size;
   (void)userdata;
#ifdef HAVE_TLS
# if defined(HAVE_WIN32)
   //sendit(prompt);
   if (win32_cgets(buf, size) == NULL) {
      buf[0] = 0;
      return 0;
   } else {
      return strlen(buf);
   }
# else
   const char *prompt = (const char *)userdata;
   char *passwd;

   passwd = getpass(prompt);
   bstrncpy(buf, passwd, size);
   return strlen(buf);
# endif
#else
   buf[0] = 0;
   return 0;
#endif
}
