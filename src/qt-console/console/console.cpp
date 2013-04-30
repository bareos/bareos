/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

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
 *  Console Class
 *
 *   Kern Sibbald, January MMVII
 *
 */ 

#include "bat.h"
#include "console.h"
#include "restore.h"
#include "select.h"
#include "run/run.h"

Console::Console(QTabWidget *parent) : Pages()
{
   QFont font;
   m_name = tr("Console");
   m_messages_pending = false;
   m_parent = parent;
   m_closeable = false;
   m_console = this;
   m_warningPrevent = false;
   m_dircommCounter = 0;

   /* 
    * Create a connection to the Director and put it in a hash table
    */
   m_dircommHash.insert(m_dircommCounter, new DirComm(this, m_dircommCounter));

   setupUi(this);
   m_textEdit = textEdit;   /* our console screen */
   m_cursor = new QTextCursor(m_textEdit->document());
   mainWin->actionConnect->setIcon(QIcon(":images/disconnected.png"));

   m_timer = NULL;
   m_contextActions.append(actionStatusDir);
   m_contextActions.append(actionConsoleHelp);
   m_contextActions.append(actionRequestMessages);
   m_contextActions.append(actionConsoleReload);
   connect(actionStatusDir, SIGNAL(triggered()), this, SLOT(status_dir()));
   connect(actionConsoleHelp, SIGNAL(triggered()), this, SLOT(consoleHelp()));
   connect(actionConsoleReload, SIGNAL(triggered()), this, SLOT(consoleReload()));
   connect(actionRequestMessages, SIGNAL(triggered()), this, SLOT(messages()));
}

Console::~Console()
{
}

void Console::startTimer()
{
   m_timer = new QTimer(this);
   QWidget::connect(m_timer, SIGNAL(timeout()), this, SLOT(poll_messages()));
   m_timer->start(mainWin->m_checkMessagesInterval*30000);
}

void Console::stopTimer()
{
   if (m_timer) {
      QWidget::disconnect(m_timer, SIGNAL(timeout()), this, SLOT(poll_messages()));
      m_timer->stop();
      delete m_timer;
      m_timer = NULL;
   }
}

/* slot connected to the timer
 * requires preferences of check messages and operates at interval */
void Console::poll_messages()
{
   int conn;

   /* Do not poll if notifier off */
   if (!mainWin->m_notify) {
      return;
   }

   /*
    * Note if we call getDirComm here, we continuously consume
    *  file descriptors.
    */
   if (!findDirComm(conn)) {    /* find a free DirComm */
      return;                   /* try later */
   }

   DirComm *dircomm = m_dircommHash.value(conn);
   if (mainWin->m_checkMessages && dircomm->m_at_main_prompt && hasFocus() && !mainWin->getWaitState()){
      dircomm->write(".messages");
      displayToPrompt(conn);
      messagesPending(false);
   }
}

/*
 * Connect to Director.  This does not connect to the director, dircomm does.
 * This creates the first and possibly 2nd dircomm instance
 */
void Console::connect_dir()
{
   DirComm *dircomm = m_dircommHash.value(0);

   if (!m_console->m_dir) {
      mainWin->set_status( tr("No Director found."));
      return;
   }

   m_textEdit = textEdit;   /* our console screen */

   if (dircomm->connect_dir()) {
      if (mainWin->m_connDebug) {
         Pmsg1(000, "DirComm 0 Seems to have Connected %s\n", m_dir->name());
      }
      beginNewCommand(0);
   }
   mainWin->set_status(_("Connected"));
   
   startTimer();                      /* start message timer */
}

/*
 * A function created to separate out the population of the lists
 * from the Console::connect_dir function
 */
void Console::populateLists(bool /*forcenew*/)
{
   int conn;
   if (!getDirComm(conn)) {
      if (mainWin->m_connDebug) Pmsg0(000, "call newDirComm\n");
      if (!newDirComm(conn)) {
         Emsg1(M_ABORT, 0, "Failed to connect to %s for populateLists.\n", m_dir->name());
         return;
      }
   }
   populateLists(conn);
   notify(conn, true);
}

void Console::populateLists(int conn)
{
   job_list.clear();
   restore_list.clear();
   client_list.clear();
   fileset_list.clear();
   messages_list.clear();
   pool_list.clear();
   storage_list.clear();
   type_list.clear();
   level_list.clear();
   volstatus_list.clear();
   mediatype_list.clear();
   dir_cmd(conn, ".jobs", job_list);
   dir_cmd(conn, ".jobs type=R", restore_list);
   dir_cmd(conn, ".clients", client_list);
   dir_cmd(conn, ".filesets", fileset_list);  
   dir_cmd(conn, ".msgs", messages_list);
   dir_cmd(conn, ".pools", pool_list);
   dir_cmd(conn, ".storage", storage_list);
   dir_cmd(conn, ".types", type_list);
   dir_cmd(conn, ".levels", level_list);
   dir_cmd(conn, ".volstatus", volstatus_list);
   dir_cmd(conn, ".mediatypes", mediatype_list);
   dir_cmd(conn, ".locations", location_list);

   if (mainWin->m_connDebug) {
      QString dbgmsg = QString("jobs=%1 clients=%2 filesets=%3 msgs=%4 pools=%5 storage=%6 types=%7 levels=%8 conn=%9 %10\n")
        .arg(job_list.count()).arg(client_list.count()).arg(fileset_list.count()).arg(messages_list.count())
        .arg(pool_list.count()).arg(storage_list.count()).arg(type_list.count()).arg(level_list.count())
        .arg(conn).arg(m_dir->name());
      Pmsg1(000, "%s", dbgmsg.toUtf8().data());
   }
   job_list.sort();
   client_list.sort();
   fileset_list.sort();
   messages_list.sort();
   pool_list.sort();
   storage_list.sort();
   type_list.sort();
   level_list.sort();
}

/*
 *  Overload function for dir_cmd with a QString
 *  Ease of use
 */
bool Console::dir_cmd(QString &cmd, QStringList &results)
{
   return dir_cmd(cmd.toUtf8().data(), results);
}

/*
 *  Overload function for dir_cmd, this is if connection is not worried about
 */
bool Console::dir_cmd(const char *cmd, QStringList &results)
{
   int conn;
   if (getDirComm(conn)) {
      dir_cmd(conn, cmd, results);
      return true;
   } else {
      Pmsg1(000, "dir_cmd failed to connect to %s\n", m_dir->name());
      return false;
   }
}

/*
 * Send a command to the Director, and return the
 *  results in a QStringList.  
 */
bool Console::dir_cmd(int conn, const char *cmd, QStringList &results)
{
   mainWin->waitEnter();
   DirComm *dircomm = m_dircommHash.value(conn);
   int stat;
   bool prev_notify = is_notify_enabled(conn);

   if (mainWin->m_connDebug) {
      QString dbgmsg = QString("dir_cmd conn %1 %2 %3\n").arg(conn).arg(m_dir->name()).arg(cmd);
      Pmsg1(000, "%s", dbgmsg.toUtf8().data());
   }
   if (prev_notify) {
      notify(conn, false);
   }
   dircomm->write(cmd);
   while ((stat = dircomm->read()) > 0 && dircomm->is_in_command()) {
      if (mainWin->m_displayAll) display_text(dircomm->msg());
      strip_trailing_junk(dircomm->msg());
      results << dircomm->msg();
   }
   if (stat > 0 && mainWin->m_displayAll) display_text(dircomm->msg());
   if (prev_notify) {
      notify(conn, true);         /* turn it back on */
   }
   discardToPrompt(conn);
   mainWin->waitExit();
   return true;                  /* ***FIXME*** return any command error */
}

/*
 * OverLoads for sql_cmd
 */
bool Console::sql_cmd(int &conn, QString &query, QStringList &results)
{
   return sql_cmd(conn, query.toUtf8().data(), results, false);
}

bool Console::sql_cmd(QString &query, QStringList &results)
{
   int conn;
   if (!getDirComm(conn)) {
      return false;
   }
   return sql_cmd(conn, query.toUtf8().data(), results, true);
}

bool Console::sql_cmd(const char *query, QStringList &results)
{
   int conn;
   if (!getDirComm(conn)) {
      return false;
   }
   return sql_cmd(conn, query, results, true);
}

/*
 * Send an sql query to the Director, and return the
 *  results in a QStringList.  
 */
bool Console::sql_cmd(int &conn, const char *query, QStringList &results, bool donotify)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   int stat;
   POOL_MEM cmd(PM_MESSAGE);
   bool prev_notify = is_notify_enabled(conn);

   if (!is_connectedGui()) {
      return false;
   }

   if (mainWin->m_connDebug) Pmsg2(000, "sql_cmd conn %i %s\n", conn, query);
   if (donotify) {
      dircomm->notify(false);
   }
   mainWin->waitEnter();
   
   pm_strcpy(cmd, ".sql query=\"");
   pm_strcat(cmd, query);
   pm_strcat(cmd, "\"");
   dircomm->write(cmd.c_str());
   while ((stat = dircomm->read()) > 0) {
      bool first = true;
      if (mainWin->m_displayAll) {
         display_text(dircomm->msg());
         display_text("\n");
      }
      strip_trailing_junk(dircomm->msg());
      bool doappend = true;
      if (first) {
         QString dum = dircomm->msg();
         if ((dum.left(6) == "*None*")) doappend = false;
      }
      if (doappend) {
         results << dircomm->msg();
      }
      first = false;
   }
   if (donotify && prev_notify) {
      dircomm->notify(true);
   }
   discardToPrompt(conn);
   mainWin->waitExit();
   if (donotify && prev_notify) {
      dircomm->notify(true);
   }
   return !mainWin->isClosing();      /* return false if closing */
}

/* 
 * Overloads for
 * Sending a command to the Director
 */
int Console::write_dir(const char *msg)
{
   int conn;
   if (getDirComm(conn)) {
      write_dir(conn, msg);
   }
   return conn;
}

int Console::write_dir(const char *msg, bool dowait)
{
   int conn;
   if (getDirComm(conn)) {
      write_dir(conn, msg, dowait);
   }
   return conn;
}

void Console::write_dir(int conn, const char *msg)
{
   write_dir(conn, msg, true);
}

/*
 * Send a command to the Director
 */
void Console::write_dir(int conn, const char *msg, bool dowait)
{
   DirComm *dircomm = m_dircommHash.value(conn);

   if (dircomm->m_sock) {
      mainWin->set_status(_("Processing command ..."));
      if (dowait)
         mainWin->waitEnter();
      dircomm->write(msg);
      if (dowait)
         mainWin->waitExit();
   } else {
      mainWin->set_status( tr(" Director not connected. Click on connect button."));
      mainWin->actionConnect->setIcon(QIcon(":images/disconnected.png"));
      QBrush redBrush(Qt::red);
      QTreeWidgetItem *item = mainWin->getFromHash(this);
      item->setForeground(0, redBrush);
      dircomm->m_at_prompt = false;
      dircomm->m_at_main_prompt = false;
   }
}

/*
 * get_job_defaults overload
 */
bool Console::get_job_defaults(struct job_defaults &job_defs)
{
   int conn;
   getDirComm(conn);
   return get_job_defaults(conn, job_defs, true);
}

bool Console::get_job_defaults(int &conn, struct job_defaults &job_defs)
{
   return get_job_defaults(conn, job_defs, true);
}

/*  
 * Send a job name to the director, and read all the resulting
 *  defaults. 
 */
bool Console::get_job_defaults(int &conn, struct job_defaults &job_defs, bool donotify)
{
   QString scmd;
   int stat;
   char *def;
   bool prev_notify = is_notify_enabled(conn);
   bool rtn = false;
   DirComm *dircomm = m_dircommHash.value(conn);

   if (donotify) {
      dircomm->notify(false);
   }
   beginNewCommand(conn);
   bool prevWaitState = mainWin->getWaitState();
   if (!prevWaitState)
      mainWin->waitEnter();
   if (mainWin->m_connDebug)
      Pmsg2(000, "job_defaults conn %i %s\n", conn, m_dir->name());
   scmd = QString(".defaults job=\"%1\"").arg(job_defs.job_name);
   dircomm->write(scmd);
   while ((stat = dircomm->read()) > 0) {
      if (mainWin->m_displayAll) display_text(dircomm->msg());
      def = strchr(dircomm->msg(), '=');
      if (!def) {
         continue;
      }
      /* Pointer to default value */
      *def++ = 0;
      strip_trailing_junk(def);

      if (strcmp(dircomm->msg(), "job") == 0) {
         if (strcmp(def, job_defs.job_name.toUtf8().data()) != 0) {
            goto bail_out;
         }
         continue;
      }
      if (strcmp(dircomm->msg(), "pool") == 0) {
         job_defs.pool_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "messages") == 0) {
         job_defs.messages_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "client") == 0) {
         job_defs.client_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "storage") == 0) {
         job_defs.store_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "where") == 0) {
         job_defs.where = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "level") == 0) {
         job_defs.level = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "type") == 0) {
         job_defs.type = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "fileset") == 0) {
         job_defs.fileset_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "catalog") == 0) {
         job_defs.catalog_name = def;
         continue;
      }
      if (strcmp(dircomm->msg(), "enabled") == 0) {
         job_defs.enabled = *def == '1' ? true : false;
         continue;
      }
   }
   rtn = true;
   /* Fall through wanted */
bail_out:
   if (donotify && prev_notify) {
      notify(conn, true);
   }
   if (!prevWaitState) {
      mainWin->waitExit();
   }
   return rtn;
}


/*
 * Save user settings associated with this console
 */
void Console::writeSettings()
{
   QFont font = get_font();

   QSettings settings(m_dir->name(), "bat");
   settings.beginGroup("Console");
   settings.setValue("consoleFont", font.family());
   settings.setValue("consolePointSize", font.pointSize());
   settings.setValue("consoleFixedPitch", font.fixedPitch());
   settings.endGroup();
}

/*
 * Read and restore user settings associated with this console
 */
void Console::readSettings()
{ 
   QFont font = get_font();

   QSettings settings(m_dir->name(), "bat");
   settings.beginGroup("Console");
   font.setFamily(settings.value("consoleFont", "Courier").value<QString>());
   font.setPointSize(settings.value("consolePointSize", 10).toInt());
   font.setFixedPitch(settings.value("consoleFixedPitch", true).toBool());
   settings.endGroup();
   m_textEdit->setFont(font);
}

/*
 * Set the console textEdit font
 */
void Console::set_font()
{
   bool ok;
   QFont font = QFontDialog::getFont(&ok, QFont(m_textEdit->font()), this);
   if (ok) {
      m_textEdit->setFont(font);
   }
}

/*
 * Get the console text edit font
 */
const QFont Console::get_font()
{
   return m_textEdit->font();
}

/*
 * Slot for responding to status dir button on button bar
 */
void Console::status_dir()
{
   QString cmd("status dir");
   consoleCommand(cmd);
}

/*
 * Slot for responding to messages button on button bar
 * Here we want to bring the console to the front so use pages' consoleCommand
 */
void Console::messages()
{
   QString cmd(".messages");
   consoleCommand(cmd);
   messagesPending(false);
}

/*
 * Put text into the console window
 */
void Console::display_textf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   display_text(buf);
}

void Console::display_text(const QString buf)
{
   if (buf.size() != 0) {
      m_cursor->insertText(buf);
      update_cursor();
   }
}


void Console::display_text(const char *buf)
{
   if (*buf != 0) {
      m_cursor->insertText(buf);
      update_cursor();
   }
}

void Console::display_html(const QString buf)
{
   if (buf.size() != 0) {
      m_cursor->insertHtml(buf);
      update_cursor();
   }
}

/* Position cursor to end of screen */
void Console::update_cursor()
{
   m_textEdit->moveCursor(QTextCursor::End);
   m_textEdit->ensureCursorVisible();
}

void Console::beginNewCommand(int conn)
{
   DirComm *dircomm = m_dircommHash.value(conn);

   for (int i=0; i < 3; i++) {
      dircomm->write(".");
      while (dircomm->read() > 0) {
         if (mainWin->m_commDebug) Pmsg2(000, "begin new command loop %i %s\n", i, m_dir->name());
         if (mainWin->m_displayAll) display_text(dircomm->msg());
      }
      if (dircomm->m_at_main_prompt) {
         break;
      }
   }
   display_text("\n");
}

void Console::displayToPrompt(int conn)
{ 
   DirComm *dircomm = m_dircommHash.value(conn);

   int stat = 0;
   QString buf;
   if (mainWin->m_commDebug) Pmsg1(000, "DisplaytoPrompt %s\n", m_dir->name());
   while (!dircomm->m_at_prompt) {
      if ((stat=dircomm->read()) > 0) {
         buf += dircomm->msg();
         if (buf.size() >= 8196 || m_messages_pending) {
            display_text(buf);
            buf.clear();
            messagesPending(false);
         }
      }
   }
   display_text(buf);
   if (mainWin->m_commDebug) Pmsg2(000, "endDisplaytoPrompt=%d %s\n", stat, m_dir->name());
}

void Console::discardToPrompt(int conn)
{
   DirComm *dircomm = m_dircommHash.value(conn);

   int stat = 0;
   if (mainWin->m_commDebug) Pmsg1(000, "discardToPrompt %s\n", m_dir->name());
   if (mainWin->m_displayAll) {
      displayToPrompt(conn);
   } else {
      while (!dircomm->m_at_prompt) {
         stat = dircomm->read();
         if (stat < 0) {
            break;
         }
      }
   }
   if (mainWin->m_commDebug) {
      Pmsg2(000, "endDiscardToPrompt conn=%i %s\n", conn, m_dir->name());
   }
}

QString Console::returnFromPrompt(int conn)
{ 
   DirComm *dircomm = m_dircommHash.value(conn);
   QString text("");

   int stat = 0;
   text = "";
   dircomm->read();
   text += dircomm->msg();
   if (mainWin->m_commDebug) Pmsg1(000, "returnFromPrompt %s\n", m_dir->name());
   while (!dircomm->m_at_prompt) {
      if ((stat=dircomm->read()) > 0) {
         text += dircomm->msg();
      }
   }
   if (mainWin->m_commDebug) Pmsg2(000, "endreturnFromPrompt=%d %s\n", stat, m_dir->name());
   return text;
}

/*
 * When the notifier is enabled, read_dir() will automatically be
 * called by the Qt event loop when ever there is any output 
 * from the Director, and read_dir() will then display it on
 * the console.
 *
 * When we are in a bat dialog, we want to control *all* output
 * from the Director, so we set notify to off.
 *    m_console->notifiy(false);
 */

/* dual purpose function to turn notify off and return a connection */
int Console::notifyOff()
{ 
   int conn = 0;
   if (getDirComm(conn)) {
      notify(conn, false);
   }
   return conn;
}

/* knowing a connection, turn notify off or on */
bool Console::notify(int conn, bool enable)
{ 
   DirComm *dircomm = m_dircommHash.value(conn);
   if (dircomm) {
      return dircomm->notify(enable);
   } else {
      return false;
   }
}

/* knowing a connection, return notify state */
bool Console::is_notify_enabled(int conn) const
{
   DirComm *dircomm = m_dircommHash.value(conn);
   if (dircomm) {
      return dircomm->is_notify_enabled();
   } else {
      return false;
   }
}

void Console::setDirectorTreeItem(QTreeWidgetItem *item)
{
   m_directorTreeItem = item;
}

void Console::setDirRes(DIRRES *dir) 
{ 
   m_dir = dir;
}

/*
 * To have the ability to get the name of the director resource.
 */
void Console::getDirResName(QString &name_returned)
{
   name_returned = m_dir->name();
}

/* Slot for responding to page selectors status help command */
void Console::consoleHelp()
{
   QString cmd("help");
   consoleCommand(cmd);
}

/* Slot for responding to page selectors reload bacula-dir.conf */
void Console::consoleReload()
{
   QString cmd("reload");
   consoleCommand(cmd);
}

/* For suppressing .messages
 * This may be rendered not needed if the multiple connections feature gets working */
bool Console::hasFocus()
{
   if (mainWin->tabWidget->currentIndex() == mainWin->tabWidget->indexOf(this))
      return true;
   else
      return false;
}

/* For adding feature to have the gui's messages button change when 
 * messages are pending */
bool Console::messagesPending(bool pend)
{
   bool prev = m_messages_pending;
   m_messages_pending = pend;
   mainWin->setMessageIcon();
   return prev;
}

/* terminate all existing connections */
void Console::terminate()
{
   foreach(DirComm* dircomm,  m_dircommHash) {
      dircomm->terminate();
   }
   m_console->stopTimer();
}

/* Maybe this should be checking the list, for the moment lets check 0 which should be connected */
bool Console::is_connectedGui()
{
   if (is_connected(0)) {
      return true;
   } else {
      QString message = tr("Director is currently disconnected\nPlease reconnect!");
      QMessageBox::warning(this, "Bat", message, QMessageBox::Ok );
      return false;
   }
}

int Console::read(int conn)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   return dircomm->read();
}

char *Console::msg(int conn)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   return dircomm->msg();
}

int Console::write(int conn, const QString msg)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   mainWin->waitEnter();
   int ret = dircomm->write(msg);
   mainWin->waitExit();
   return ret;
}

int Console::write(int conn, const char *msg)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   mainWin->waitEnter();
   int ret = dircomm->write(msg);
   mainWin->waitExit();
   return ret;
}

/* This checks to see if any is connected */
bool Console::is_connected()
{
   bool connected = false;
   foreach(DirComm* dircomm,  m_dircommHash) {
      if (dircomm->is_connected())
         return true;
   }
   return connected;
}

/* knowing the connection id, is it connected */
bool Console::is_connected(int conn)
{
   DirComm *dircomm = m_dircommHash.value(conn);
   return dircomm->is_connected();
}

/*
 * Need a connection.  Check existing connections or create one
 */
bool Console::getDirComm(int &conn)
{
   if (findDirComm(conn)) {
      return true;
   }
   if (mainWin->m_connDebug) Pmsg0(000, "call newDirComm\n");
   return newDirComm(conn);
}


/*
 * Try to find a free (unused but established) connection
 * KES: Note, I think there is a problem here because for
 *   some reason, the notifier is often turned off on file  
 *   descriptors that seem to me to be available.  That means
 *   that we do not use a free descriptor and thus we will create
 *   a new connection that is maybe not necessary.  Someone needs
 *   to look into whether or not notify() is correctly turned on
 *   when we are back at the command prompt and idle.
 *                         
 */
bool Console::findDirComm(int &conn)
{
   QHash<int, DirComm*>::const_iterator iter = m_dircommHash.constBegin();
   while (iter != m_dircommHash.constEnd()) {
      DirComm *dircomm = iter.value();
      if (dircomm->m_at_prompt && dircomm->m_at_main_prompt && dircomm->is_notify_enabled()) {
         conn = dircomm->m_conn;
         return true;
      }
      if (mainWin->m_connDebug) {
         Pmsg4(000, "currentDirComm=%d at_prompt=%d at_main=%d && notify=%d\n",                                      
            dircomm->m_conn, dircomm->m_at_prompt, dircomm->m_at_main_prompt, dircomm->is_notify_enabled());
      }
      ++iter;
   }
   return false;
}

/*
 *  Create a new connection
 */
bool Console::newDirComm(int &conn)
{
   m_dircommCounter++;
   if (mainWin->m_connDebug) {
      Pmsg2(000, "newDirComm=%i to: %s\n", m_dircommCounter, m_dir->name());
   }
   DirComm *dircomm = new DirComm(this, m_dircommCounter);
   m_dircommHash.insert(m_dircommCounter, dircomm);
   bool success = dircomm->connect_dir();
   if (mainWin->m_connDebug) {
      if (success) {
         Pmsg2(000, "newDirComm=%i Connected %s\n", m_dircommCounter, m_dir->name());
      } else {
         Emsg2(M_ERROR, 0, "DirComm=%i. Unable to connect to %s\n",
               m_dircommCounter, m_dir->name());
      }
   }
   if (!success) {
      m_dircommHash.remove(m_dircommCounter);
      delete dircomm;
      m_dircommCounter--;
   }
   conn = m_dircommCounter;
   return success;
}
