/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

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

/*
 * Main Window control for bat (qt-console)
 *
 * Kern Sibbald, January MMVII
 */

#include "bat.h"
#include "version.h"
#include "joblist/joblist.h"
#include "storage/storage.h"
#include "fileset/fileset.h"
#include "label/label.h"
#include "run/run.h"
#include "pages.h"
#include "restore/restore.h"
#include "medialist/medialist.h"
#include "joblist/joblist.h"
#include "clients/clients.h"
#include "restore/restoretree.h"
#include "help/help.h"
#include "jobs/jobs.h"
#include "medialist/mediaview.h"
#include "status/dirstat.h"
#include "util/fmtwidgetitem.h"

/*
 * Daemon message callback
 */
void message_callback(int /* type */, char *msg)
{
   QMessageBox::warning(mainWin, "Bat", msg, QMessageBox::Ok);
}

MainWin::MainWin(QWidget *parent) : QMainWindow(parent)
{
   app->setOverrideCursor(QCursor(Qt::WaitCursor));
   m_isClosing = false;
   m_waitState = false;
   m_doConnect = false;
   m_treeStackTrap = false;
   m_dtformat = "yyyy-MM-dd HH:mm:ss";
   mainWin = this;
   setupUi(this);                     /* Setup UI defined by main.ui (designer) */
   register_message_callback(message_callback);
   readPreferences();
   treeWidget->clear();
   treeWidget->setColumnCount(1);
   treeWidget->setHeaderLabel( tr("Select Page") );
   treeWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
   tabWidget->setTabsClosable(true);  /* wait for QT 4.5 */
   createPages();

   resetFocus(); /* lineEdit->setFocus() */

   this->show();

   readSettings();

   this->about();

   foreach(Console *console, m_consoleHash) {
      console->connect_dir();
   }
   /*
    * Note, the notifier is now a global flag, although each notifier
    *  can be individually turned on and off at a socket level.  Once
    *  the notifier is turned off, we don't accept anything from anyone
    *  this prevents unwanted messages from getting into the input
    *  dialogs such as restore that read from the director and "know"
    *  what to expect.
    */
   m_notify = true;
   m_currentConsole = (Console*)getFromHash(m_firstItem);
   QTimer::singleShot(2000, this, SLOT(popLists()));
   if (m_miscDebug) {
      QString directoryResourceName;
      m_currentConsole->getDirResName(directoryResourceName);
      Pmsg1(100, "Setting initial window to %s\n", directoryResourceName.toUtf8().data());
   }
   app->restoreOverrideCursor();
}

void MainWin::popLists()
{
   foreach(Console *console, m_consoleHash) {
      console->populateLists(true);
   }
   m_doConnect = true;
   connectConsoleSignals();
   connectSignals();
   app->restoreOverrideCursor();
   m_currentConsole->setCurrent();
}

void MainWin::createPages()
{
   DIRRES *dir;
   QTreeWidgetItem *item, *topItem;
   m_firstItem = NULL;

   LockRes();
   foreach_res(dir, R_DIRECTOR) {

      /* Create console tree stacked widget item */
      m_currentConsole = new Console(tabWidget);
      m_currentConsole->setDirRes(dir);
      m_currentConsole->readSettings();

      /* The top tree item representing the director */
      topItem = new QTreeWidgetItem(treeWidget);
      topItem->setText(0, dir->name());
      topItem->setIcon(0, QIcon(":images/server.png"));
      /* Set background to grey for ease of identification of inactive Director */
      QBrush greyBrush(Qt::lightGray);
      topItem->setBackground(0, greyBrush);
      m_currentConsole->setDirectorTreeItem(topItem);
      m_consoleHash.insert(topItem, m_currentConsole);

      /* Create Tree Widget Item */
      item = new QTreeWidgetItem(topItem);
      item->setText(0, tr("Console"));
      if (!m_firstItem){ m_firstItem = item; }
      item->setIcon(0,QIcon(QString::fromUtf8(":images/utilities-terminal.png")));

      /* insert the cosole and tree widget item into the hashes */
      hashInsert(item, m_currentConsole);
      m_currentConsole->dockPage();

      /* Set Color of treeWidgetItem for the console
      * It will be set to green in the console class if the connection is made.
      */
      QBrush redBrush(Qt::red);
      item->setForeground(0, redBrush);

      /*
       * Create instances in alphabetic order of the rest
       *  of the classes that will by default exist under each Director.
       */
      new bRestore();
      new Clients();
      new FileSet();
      new Jobs();
      createPageJobList("", "", "", "", NULL);
      new MediaList();
      new MediaView();
      new Storage();
      if (m_openBrowser) {
         new restoreTree();
      }
      if (m_openDirStat) {
         new DirStat();
      }
      treeWidget->expandItem(topItem);
      tabWidget->setCurrentWidget(m_currentConsole);
   }
   UnlockRes();
}

/*
 * create an instance of the the joblist class on the stack
 */
void MainWin::createPageJobList(const QString &media, const QString &client,
              const QString &job, const QString &fileset, QTreeWidgetItem *parentTreeWidgetItem)
{
   QTreeWidgetItem *holdItem;

   /* save current tree widget item in case query produces no results */
   holdItem = treeWidget->currentItem();
   JobList* joblist = new JobList(media, client, job, fileset, parentTreeWidgetItem);
   /* If this is a query of jobs on a specific media */
   if ((media != "") || (client != "") || (job != "") || (fileset != "")) {
      joblist->setCurrent();
      /* did query produce results, if not close window and set back to hold */
      if (joblist->m_resultCount == 0) {
         joblist->closeStackPage();
         treeWidget->setCurrentItem(holdItem);
      }
   }
}

/*
 * Handle up and down arrow keys for the command line
 *  history.
 */
void MainWin::keyPressEvent(QKeyEvent *event)
{
   if (m_cmd_history.size() == 0) {
      event->ignore();
      return;
   }
   switch (event->key()) {
   case Qt::Key_Down:
      if (m_cmd_last < 0 || m_cmd_last >= (m_cmd_history.size()-1)) {
         event->ignore();
         return;
      }
      m_cmd_last++;
      break;
   case Qt::Key_Up:
      if (m_cmd_last == 0) {
         event->ignore();
         return;
      }
      if (m_cmd_last < 0 || m_cmd_last > (m_cmd_history.size()-1)) {
         m_cmd_last = m_cmd_history.size() - 1;
      } else {
         m_cmd_last--;
      }
      break;
   default:
      event->ignore();
      return;
   }
   lineEdit->setText(m_cmd_history[m_cmd_last]);
}

void MainWin::connectSignals()
{
   /* Connect signals to slots */
   connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   connect(actionAbout_bat, SIGNAL(triggered()), this, SLOT(about()));
   connect(actionBat_Help, SIGNAL(triggered()), this, SLOT(help()));
   connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   connect(treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   connect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(stackItemChanged(int)));
   connect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closePage(int)));
   connect(actionQuit, SIGNAL(triggered()), app, SLOT(closeAllWindows()));
   connect(actionLabel, SIGNAL(triggered()), this,  SLOT(labelButtonClicked()));
   connect(actionRun, SIGNAL(triggered()), this,  SLOT(runButtonClicked()));
   connect(actionEstimate, SIGNAL(triggered()), this,  SLOT(estimateButtonClicked()));
   connect(actionBrowse, SIGNAL(triggered()), this,  SLOT(browseButtonClicked()));
   connect(actionStatusDirPage, SIGNAL(triggered()), this,  SLOT(statusPageButtonClicked()));
   connect(actionRestore, SIGNAL(triggered()), this,  SLOT(restoreButtonClicked()));
   connect(actionUndock, SIGNAL(triggered()), this,  SLOT(undockWindowButton()));
   connect(actionToggleDock, SIGNAL(triggered()), this,  SLOT(toggleDockContextWindow()));
   connect(actionClosePage, SIGNAL(triggered()), this,  SLOT(closeCurrentPage()));
   connect(actionPreferences, SIGNAL(triggered()), this,  SLOT(setPreferences()));
   connect(actionRepopLists, SIGNAL(triggered()), this,  SLOT(repopLists()));
   connect(actionReloadRepop, SIGNAL(triggered()), this,  SLOT(reloadRepopLists()));
}

void MainWin::disconnectSignals()
{
   /* Connect signals to slots */
   disconnect(lineEdit, SIGNAL(returnPressed()), this, SLOT(input_line()));
   disconnect(actionAbout_bat, SIGNAL(triggered()), this, SLOT(about()));
   disconnect(actionBat_Help, SIGNAL(triggered()), this, SLOT(help()));
   disconnect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(treeItemClicked(QTreeWidgetItem *, int)));
   disconnect(treeWidget, SIGNAL( currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   disconnect(tabWidget, SIGNAL(currentChanged(int)), this, SLOT(stackItemChanged(int)));
   disconnect(tabWidget, SIGNAL(tabCloseRequested(int)), this, SLOT(closePage(int)));
   disconnect(actionQuit, SIGNAL(triggered()), app, SLOT(closeAllWindows()));
   disconnect(actionLabel, SIGNAL(triggered()), this,  SLOT(labelButtonClicked()));
   disconnect(actionRun, SIGNAL(triggered()), this,  SLOT(runButtonClicked()));
   disconnect(actionEstimate, SIGNAL(triggered()), this,  SLOT(estimateButtonClicked()));
   disconnect(actionBrowse, SIGNAL(triggered()), this,  SLOT(browseButtonClicked()));
   disconnect(actionStatusDirPage, SIGNAL(triggered()), this,  SLOT(statusPageButtonClicked()));
   disconnect(actionRestore, SIGNAL(triggered()), this,  SLOT(restoreButtonClicked()));
   disconnect(actionUndock, SIGNAL(triggered()), this,  SLOT(undockWindowButton()));
   disconnect(actionToggleDock, SIGNAL(triggered()), this,  SLOT(toggleDockContextWindow()));
   disconnect(actionClosePage, SIGNAL(triggered()), this,  SLOT(closeCurrentPage()));
   disconnect(actionPreferences, SIGNAL(triggered()), this,  SLOT(setPreferences()));
   disconnect(actionRepopLists, SIGNAL(triggered()), this,  SLOT(repopLists()));
   disconnect(actionReloadRepop, SIGNAL(triggered()), this,  SLOT(reloadRepopLists()));
}

/*
 *  Enter wait state
 */
void MainWin::waitEnter()
{
   if (m_waitState || m_isClosing) {
      return;
   }
   m_waitState = true;
   if (mainWin->m_connDebug) Pmsg0(000, "Entering Wait State\n");
   app->setOverrideCursor(QCursor(Qt::WaitCursor));
   disconnectSignals();
   disconnectConsoleSignals(m_currentConsole);
   m_waitTreeItem = treeWidget->currentItem();
}

/*
 *  Leave wait state
 */
void MainWin::waitExit()
{
   if (!m_waitState || m_isClosing) {
      return;
   }
   if (mainWin->m_connDebug) Pmsg0(000, "Exiting Wait State\n");
   if (m_waitTreeItem && (m_waitTreeItem != treeWidget->currentItem())) {
      treeWidget->setCurrentItem(m_waitTreeItem);
   }
   if (m_doConnect) {
      connectSignals();
      connectConsoleSignals();
   }
   app->restoreOverrideCursor();
   m_waitState = false;
}

void MainWin::connectConsoleSignals()
{
   connect(actionConnect, SIGNAL(triggered()), m_currentConsole, SLOT(connect_dir()));
   connect(actionSelectFont, SIGNAL(triggered()), m_currentConsole, SLOT(set_font()));
   connect(actionMessages, SIGNAL(triggered()), m_currentConsole, SLOT(messages()));
}

void MainWin::disconnectConsoleSignals(Console *console)
{
   disconnect(actionConnect, SIGNAL(triggered()), console, SLOT(connect_dir()));
   disconnect(actionMessages, SIGNAL(triggered()), console, SLOT(messages()));
   disconnect(actionSelectFont, SIGNAL(triggered()), console, SLOT(set_font()));
}


/*
 * Two functions to respond to menu items to repop lists and execute reload and repopulate
 * the lists for jobs, clients, filesets .. ..
 */
void MainWin::repopLists()
{
   m_currentConsole->populateLists(false);
}
void MainWin::reloadRepopLists()
{
   QString cmd = "reload";
   m_currentConsole->consoleCommand(cmd);
   m_currentConsole->populateLists(false);
}

/*
 * Reimplementation of QWidget closeEvent virtual function
 */
void MainWin::closeEvent(QCloseEvent *event)
{
   m_isClosing = true;
   writeSettings();
   /* Remove all groups from settings for OpenOnExit so that we can start some of the status windows */
   foreach(Console *console, m_consoleHash){
      QSettings settings(console->m_dir->name(), "bat");
      settings.beginGroup("OpenOnExit");
      settings.remove("");
      settings.endGroup();
   }
   /* close all non console pages, this will call settings in destructors */
   while (m_consoleHash.count() < m_pagehash.count()) {
      foreach(Pages *page, m_pagehash) {
         if (page !=  page->console()) {
            QTreeWidgetItem* pageSelectorTreeWidgetItem = mainWin->getFromHash(page);
            if (pageSelectorTreeWidgetItem->childCount() == 0) {
               page->console()->setCurrent();
               page->closeStackPage();
            }
         }
      }
   }
   foreach(Console *console, m_consoleHash){
      console->writeSettings();
      console->terminate();
      console->closeStackPage();
   }
   event->accept();
}

void MainWin::writeSettings()
{
   QSettings settings("bareos.org", "bat");

   settings.beginGroup("MainWin");
   settings.setValue("winSize", size());
   settings.setValue("winPos", pos());
   settings.setValue("state", saveState());
   settings.endGroup();

}

void MainWin::readSettings()
{
   QSettings settings("bareos.org", "bat");

   settings.beginGroup("MainWin");
   resize(settings.value("winSize", QSize(1041, 801)).toSize());
   move(settings.value("winPos", QPoint(200, 150)).toPoint());
   restoreState(settings.value("state").toByteArray());
   settings.endGroup();
}

/*
 * This subroutine is called with an item in the Page Selection window
 *   is clicked
 */
void MainWin::treeItemClicked(QTreeWidgetItem *item, int /*column*/)
{
   /* Is this a page that has been inserted into the hash  */
   Pages* page = getFromHash(item);
   if (page) {
      int stackindex = tabWidget->indexOf(page);

      if (stackindex >= 0) {
         tabWidget->setCurrentWidget(page);
      }
      page->dockPage();
      /* run the virtual function in case this class overrides it */
      page->PgSeltreeWidgetClicked();
   } else {
      Dmsg0(000, "Page not in hash");
   }
}

/*
 * Called with a change of the highlighed tree widget item in the page selector.
 */
void MainWin::treeItemChanged(QTreeWidgetItem *currentitem, QTreeWidgetItem *previousitem)
{
   if (m_isClosing) return; /* if closing the application, do nothing here */

   Pages *previousPage, *nextPage;
   Console *previousConsole = NULL;
   Console *nextConsole;

   /* remove all actions before adding actions appropriate for new page */
   foreach(QAction* pageAction, treeWidget->actions()) {
      treeWidget->removeAction(pageAction);
   }

   /* first determine the next item */

   /* knowing the treeWidgetItem, get the page from the hash */
   nextPage = getFromHash(currentitem);
   nextConsole = m_consoleHash.value(currentitem);
   /* Is this a page that has been inserted into the hash  */
   if (nextPage) {
      nextConsole = nextPage->console();
      /* then is it a treeWidgetItem representing a director */
   } else if (nextConsole) {
      /* let the next page BE the console */
      nextPage = nextConsole;
   } else {
      /* Should never get here */
      nextPage = NULL;
      nextConsole = NULL;
   }

   /* The Previous item */

   /* this condition prevents a segfault.  The first time there is no previousitem*/
   if (previousitem) {
      if (m_treeStackTrap == false) { /* keep track of previous items for going Back */
         m_treeWidgetStack.append(previousitem);
      }
      /* knowing the treeWidgetItem, get the page from the hash */
      previousPage = getFromHash(previousitem);
      previousConsole = m_consoleHash.value(previousitem);
      if (previousPage) {
         previousConsole = previousPage->console();
      } else if (previousConsole) {
         previousPage = previousConsole;
      }
      if ((previousPage) || (previousConsole)) {
         if (nextConsole != previousConsole) {
            /* remove connections to the current console */
            disconnectConsoleSignals(previousConsole);
            QTreeWidgetItem *dirItem = previousConsole->directorTreeItem();
            QBrush greyBrush(Qt::lightGray);
            dirItem->setBackground(0, greyBrush);
         }
      }
   }

   /* process the current (next) item */

   if ((nextPage) || (nextConsole)) {
      if (nextConsole != previousConsole) {
         /* make connections to the current console */
         m_currentConsole = nextConsole;
         connectConsoleSignals();
         setMessageIcon();
         /* Set director's tree widget background to magenta for ease of identification */
         QTreeWidgetItem *dirItem = m_currentConsole->directorTreeItem();
         QBrush magentaBrush(Qt::magenta);
         dirItem->setBackground(0, magentaBrush);
      }
      /* set the value for the currently active console */
      int stackindex = tabWidget->indexOf(nextPage);
      nextPage->firstUseDock();

      /* Is this page currently on the stack or is it undocked */
      if (stackindex >= 0) {
         /* put this page on the top of the stack */
         tabWidget->setCurrentIndex(stackindex);
      } else {
         /* it is undocked, raise it to the front */
         nextPage->raise();
      }
      /* for the page selectors menu action to dock or undock, set the text */
      nextPage->setContextMenuDockText();

      treeWidget->addAction(actionToggleDock);
      /* if this page is closeable, and it has no childern, then add that action */
      if ((nextPage->isCloseable()) && (currentitem->child(0) == NULL))
         treeWidget->addAction(actionClosePage);

      /* Add the actions to the Page Selectors tree widget that are part of the
       * current items list of desired actions regardless of whether on top of stack*/
      treeWidget->addActions(nextPage->m_contextActions);
   }
}

void MainWin::labelButtonClicked()
{
   new labelPage();
}

void MainWin::runButtonClicked()
{
   new runPage("");
}

void MainWin::estimateButtonClicked()
{
   new estimatePage();
}

void MainWin::browseButtonClicked()
{
   new restoreTree();
}

void MainWin::statusPageButtonClicked()
{
   /* if one exists, then just set it current */
   bool found = false;
   foreach(Pages *page, m_pagehash) {
      if (m_currentConsole == page->console()) {
         if (page->name() == tr("Director Status")) {
            found = true;
            page->setCurrent();
         }
      }
   }
   if (!found) {
      new DirStat();
   }
}

void MainWin::restoreButtonClicked()
{
   new prerestorePage();
   if (mainWin->m_miscDebug) Pmsg0(000, "in restoreButtonClicked after prerestorePage\n");
}

/*
 * The user just finished typing a line in the command line edit box
 */
void MainWin::input_line()
{
   int conn;
   QString cmdStr = lineEdit->text();    /* Get the text */
   lineEdit->clear();                    /* clear the lineEdit box */
   if (m_currentConsole->is_connected()) {
      if (m_currentConsole->findDirComm(conn)) {
         m_currentConsole->consoleCommand(cmdStr, conn);
      } else {
         /* Use consoleCommand to allow typing anything */
         m_currentConsole->consoleCommand(cmdStr);
      }
   } else {
      set_status(tr("Director not connected. Click on connect button."));
   }
   m_cmd_history.append(cmdStr);
   m_cmd_last = -1;
   if (treeWidget->currentItem() != getFromHash(m_currentConsole))
      m_currentConsole->setCurrent();
}

void MainWin::about()
{
   QString fmt = QString("<br><h2>Bareos Administration Tool %1</h2>"
                         "Dear user, please be aware that"
                         "<h2> <span style=\"color:#aa0000;\">BAT is deprecated "
                         "<br>in favour of "
                         "<a href=\"http://www.bareos.org/en/bareos-webui.html\">Bareos-Webui</a></span></h2>"
                         "<p>Bareos 16.2.x will be the last releases that includes BAT.</p>"
                         "<p>Therefore we would like to ask for your help to improve the Bareos-Webui. "
                         "If you miss any functionality from BAT in Bareos-Webui, "
                         "please inform us on the "
                         "<a href=\"https://groups.google.com/forum/?fromgroups#!forum/bareos-users\">bareos-users</a> maillinglist or"
                         "<br>send a mail to <a href=\"mailto:info@bareos.org\">info@bareos.org</a>.</p>"
                         "<p>Original by Dirk H Bartley and Kern Sibbald"
                         "<br>For more information, see: www.bareos.com"
                         "<br>Copyright &copy; 2007-2010 Free Software Foundation Europe e.V."
                         "<br>Copyright &copy; 2011-2012 Planets Communications B.V."
                         "<br>Copyright &copy; 2013-%2 Bareos GmbH & Co. KG"
                         "<br>BAREOS &reg; is a registered trademark of Bareos GmbH & Co. KG"
                         "<br>Licensed under GNU AGPLv3.").arg(VERSION).arg(BYEAR);

   QMessageBox::about(this, tr("About Bareos Administration Tool"), fmt);
}

void MainWin::help()
{
   Help::displayFile("index.html");
}

void MainWin::set_statusf(const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[1000];
   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   set_status(buf);
}

void MainWin::set_status_ready()
{
   set_status(tr(" Ready"));
}

void MainWin::set_status(const QString &str)
{
   statusBar()->showMessage(str);
}

void MainWin::set_status(const char *buf)
{
   statusBar()->showMessage(buf);
}

/*
 * Function to respond to the button bar button to undock
 */
void MainWin::undockWindowButton()
{
   Pages* page = (Pages*)tabWidget->currentWidget();
   if (page) {
      page->togglePageDocking();
   }
}

/*
 * Function to respond to action on page selector context menu to toggle the
 * dock status of the window associated with the page selectors current
 * tree widget item.
 */
void MainWin::toggleDockContextWindow()
{
   QTreeWidgetItem *currentitem = treeWidget->currentItem();

   /* Is this a page that has been inserted into the hash  */
   if (getFromHash(currentitem)) {
      Pages* page = getFromHash(currentitem);
      if (page) {
         page->togglePageDocking();
      }
   }
}

/*
 * This function is called when the stack item is changed.  Call
 * the virtual function here.  Avoids a window being undocked leaving
 * a window at the top of the stack unpopulated.
 */
void MainWin::stackItemChanged(int)
{
   if (m_isClosing) return; /* if closing the application, do nothing here */
   Pages* page = (Pages*)tabWidget->currentWidget();
   /* run the virtual function in case this class overrides it */
   if (page) {
      page->currentStackItem();
   }
   if (!m_waitState) {
      disconnect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(treeItemClicked(QTreeWidgetItem *, int)));
      disconnect(treeWidget, SIGNAL( currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
      treeWidget->setCurrentItem(getFromHash(page));
      connect(treeWidget, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this, SLOT(treeItemClicked(QTreeWidgetItem *, int)));
      connect(treeWidget, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)));
   }
}

/*
 * Function to simplify insertion of QTreeWidgetItem <-> Page association
 * into a double direction hash.
 */
void MainWin::hashInsert(QTreeWidgetItem *item, Pages *page)
{
   m_pagehash.insert(item, page);
   m_widgethash.insert(page, item);
}

/*
 * Function to simplify removal of QTreeWidgetItem <-> Page association
 * into a double direction hash.
 */
void MainWin::hashRemove(QTreeWidgetItem *item, Pages *page)
{
   /* I had all sorts of return status checking code here.  Do we have a log
    * level capability in bat.  I would have left it in but it used printf's
    * and it should really be some kind of log level facility ???
    * ******FIXME********/
   m_pagehash.remove(item);
   m_widgethash.remove(page);
}

/*
 * Function to retrieve a Page* when the item in the page selector's tree is
 * known.
 */
Pages* MainWin::getFromHash(QTreeWidgetItem *item)
{
   return m_pagehash.value(item);
}

/*
 * Function to retrieve the page selectors tree widget item when the page is
 * known.
 */
QTreeWidgetItem* MainWin::getFromHash(Pages *page)
{
   return m_widgethash.value(page);
}

void MainWin::closeCurrentPage()
{
   closePage(-1);
}

/*
 * Function to respond to action on page selector context menu to close the
 * current window.
 */
void MainWin::closePage(int item)
{
   QTreeWidgetItem *currentitem;
   Pages *page = NULL;

   if (item >= 0) {
     page = (Pages *)tabWidget->widget(item);
   } else {
      currentitem = treeWidget->currentItem();
      /* Is this a page that has been inserted into the hash  */
      if (getFromHash(currentitem)) {
         page = getFromHash(currentitem);
      }
   }

   if (page) {
      if (page->isCloseable()) {
         page->closeStackPage();
      } else {
         page->hidePage();
      }
   }
}

/* Quick function to return the current console */
Console *MainWin::currentConsole()
{
   return m_currentConsole;
}

/* Quick function to return the tree item for the director */
QTreeWidgetItem *MainWin::currentTopItem()
{
   return m_currentConsole->directorTreeItem();
}

/* Preferences menu item clicked */
void MainWin::setPreferences()
{
   prefsDialog prefs;
   prefs.commDebug->setCheckState(m_commDebug ? Qt::Checked : Qt::Unchecked);
   prefs.connDebug->setCheckState(m_connDebug ? Qt::Checked : Qt::Unchecked);
   prefs.displayAll->setCheckState(m_displayAll ? Qt::Checked : Qt::Unchecked);
   prefs.sqlDebug->setCheckState(m_sqlDebug ? Qt::Checked : Qt::Unchecked);
   prefs.commandDebug->setCheckState(m_commandDebug ? Qt::Checked : Qt::Unchecked);
   prefs.miscDebug->setCheckState(m_miscDebug ? Qt::Checked : Qt::Unchecked);
   prefs.recordLimit->setCheckState(m_recordLimitCheck ? Qt::Checked : Qt::Unchecked);
   prefs.recordSpinBox->setValue(m_recordLimitVal);
   prefs.daysLimit->setCheckState(m_daysLimitCheck ? Qt::Checked : Qt::Unchecked);
   prefs.daysSpinBox->setValue(m_daysLimitVal);
   prefs.checkMessages->setCheckState(m_checkMessages ? Qt::Checked : Qt::Unchecked);
   prefs.checkMessagesSpin->setValue(m_checkMessagesInterval);
   prefs.executeLongCheckBox->setCheckState(m_longList ? Qt::Checked : Qt::Unchecked);
   prefs.rtPopDirCheckBox->setCheckState(m_rtPopDirDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtDirCurICCheckBox->setCheckState(m_rtDirCurICDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtDirICCheckBox->setCheckState(m_rtDirICDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtFileTabICCheckBox->setCheckState(m_rtFileTabICDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtVerTabICCheckBox->setCheckState(m_rtVerTabICDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtUpdateFTCheckBox->setCheckState(m_rtUpdateFTDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtUpdateVTCheckBox->setCheckState(m_rtUpdateVTDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtChecksCheckBox->setCheckState(m_rtChecksDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtIconStateCheckBox->setCheckState(m_rtIconStateDebug ? Qt::Checked : Qt::Unchecked);
   prefs.rtRestore1CheckBox->setCheckState(m_rtRestore1Debug ? Qt::Checked : Qt::Unchecked);
   prefs.rtRestore2CheckBox->setCheckState(m_rtRestore2Debug ? Qt::Checked : Qt::Unchecked);
   prefs.rtRestore3CheckBox->setCheckState(m_rtRestore3Debug ? Qt::Checked : Qt::Unchecked);
   switch (ItemFormatterBase::getBytesConversion()) {
   case ItemFormatterBase::BYTES_CONVERSION_NONE:
      prefs.radioConvertOff->setChecked(Qt::Checked);
      break;
   case ItemFormatterBase::BYTES_CONVERSION_IEC:
      prefs.radioConvertIEC->setChecked(Qt::Checked);
      break;
   default:
      prefs.radioConvertStandard->setChecked(Qt::Checked);
      break;
   }
   prefs.openPlotCheckBox->setCheckState(m_openPlot ? Qt::Checked : Qt::Unchecked);
   prefs.openBrowserCheckBox->setCheckState(m_openBrowser ? Qt::Checked : Qt::Unchecked);
   prefs.openDirStatCheckBox->setCheckState(m_openDirStat ? Qt::Checked : Qt::Unchecked);
   prefs.exec();
}

/* Preferences dialog */
prefsDialog::prefsDialog() : QDialog()
{
   setupUi(this);
}

void prefsDialog::accept()
{
   this->hide();
   mainWin->m_commDebug = this->commDebug->checkState() == Qt::Checked;
   mainWin->m_connDebug = this->connDebug->checkState() == Qt::Checked;
   mainWin->m_displayAll = this->displayAll->checkState() == Qt::Checked;
   mainWin->m_sqlDebug = this->sqlDebug->checkState() == Qt::Checked;
   mainWin->m_commandDebug = this->commandDebug->checkState() == Qt::Checked;
   mainWin->m_miscDebug = this->miscDebug->checkState() == Qt::Checked;
   mainWin->m_recordLimitCheck = this->recordLimit->checkState() == Qt::Checked;
   mainWin->m_recordLimitVal = this->recordSpinBox->value();
   mainWin->m_daysLimitCheck = this->daysLimit->checkState() == Qt::Checked;
   mainWin->m_daysLimitVal = this->daysSpinBox->value();
   mainWin->m_checkMessages = this->checkMessages->checkState() == Qt::Checked;
   mainWin->m_checkMessagesInterval = this->checkMessagesSpin->value();
   mainWin->m_longList = this->executeLongCheckBox->checkState() == Qt::Checked;

   mainWin->m_rtPopDirDebug = this->rtPopDirCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtDirCurICDebug = this->rtDirCurICCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtDirICDebug = this->rtDirICCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtFileTabICDebug = this->rtFileTabICCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtVerTabICDebug = this->rtVerTabICCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtUpdateFTDebug = this->rtUpdateFTCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtUpdateVTDebug = this->rtUpdateVTCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtChecksDebug = this->rtChecksCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtIconStateDebug = this->rtIconStateCheckBox->checkState() == Qt::Checked;
   mainWin->m_rtRestore1Debug = this->rtRestore1CheckBox->checkState() == Qt::Checked;
   mainWin->m_rtRestore2Debug = this->rtRestore2CheckBox->checkState() == Qt::Checked;
   mainWin->m_rtRestore3Debug = this->rtRestore3CheckBox->checkState() == Qt::Checked;
   if (this->radioConvertOff->isChecked()) {
      ItemFormatterBase::setBytesConversion(ItemFormatterBase::BYTES_CONVERSION_NONE);
   } else if (this->radioConvertIEC->isChecked()){
      ItemFormatterBase::setBytesConversion(ItemFormatterBase::BYTES_CONVERSION_IEC);
   } else {
      ItemFormatterBase::setBytesConversion(ItemFormatterBase::BYTES_CONVERSION_SI);
   }
   mainWin->m_openPlot = this->openPlotCheckBox->checkState() == Qt::Checked;
   mainWin->m_openBrowser = this->openBrowserCheckBox->checkState() == Qt::Checked;
   mainWin->m_openDirStat = this->openDirStatCheckBox->checkState() == Qt::Checked;

   QSettings settings("www.bareos.org", "bat");
   settings.beginGroup("Debug");
   settings.setValue("commDebug", mainWin->m_commDebug);
   settings.setValue("connDebug", mainWin->m_connDebug);
   settings.setValue("displayAll", mainWin->m_displayAll);
   settings.setValue("sqlDebug", mainWin->m_sqlDebug);
   settings.setValue("commandDebug", mainWin->m_commandDebug);
   settings.setValue("miscDebug", mainWin->m_miscDebug);
   settings.endGroup();
   settings.beginGroup("JobList");
   settings.setValue("recordLimitCheck", mainWin->m_recordLimitCheck);
   settings.setValue("recordLimitVal", mainWin->m_recordLimitVal);
   settings.setValue("daysLimitCheck", mainWin->m_daysLimitCheck);
   settings.setValue("daysLimitVal", mainWin->m_daysLimitVal);
   settings.endGroup();
   settings.beginGroup("Timers");
   settings.setValue("checkMessages", mainWin->m_checkMessages);
   settings.setValue("checkMessagesInterval", mainWin->m_checkMessagesInterval);
   settings.endGroup();
   settings.beginGroup("Misc");
   settings.setValue("longList", mainWin->m_longList);
   settings.setValue("byteConvert", ItemFormatterBase::getBytesConversion());
   settings.setValue("openplot", mainWin->m_openPlot);
   settings.setValue("openbrowser", mainWin->m_openBrowser);
   settings.setValue("opendirstat", mainWin->m_openDirStat);
   settings.endGroup();
   settings.beginGroup("RestoreTree");
   settings.setValue("rtPopDirDebug", mainWin->m_rtPopDirDebug);
   settings.setValue("rtDirCurICDebug", mainWin->m_rtDirCurICDebug);
   settings.setValue("rtDirCurICRetDebug", mainWin->m_rtDirICDebug);
   settings.setValue("rtFileTabICDebug", mainWin->m_rtFileTabICDebug);
   settings.setValue("rtVerTabICDebug", mainWin->m_rtVerTabICDebug);
   settings.setValue("rtUpdateFTDebug", mainWin->m_rtUpdateFTDebug);
   settings.setValue("rtUpdateVTDebug", mainWin->m_rtUpdateVTDebug);
   settings.setValue("rtChecksDebug", mainWin->m_rtChecksDebug);
   settings.setValue("rtIconStateDebug", mainWin->m_rtIconStateDebug);
   settings.setValue("rtRestore1Debug", mainWin->m_rtRestore1Debug);
   settings.setValue("rtRestore2Debug", mainWin->m_rtRestore2Debug);
   settings.setValue("rtRestore3Debug", mainWin->m_rtRestore3Debug);
   settings.endGroup();
}

void prefsDialog::reject()
{
   this->hide();
   mainWin->set_status(tr("Canceled"));
}

/* read preferences for the prefences dialog box */
void MainWin::readPreferences()
{
   QSettings settings("www.bareos.org", "bat");
   settings.beginGroup("Debug");
   m_commDebug = settings.value("commDebug", false).toBool();
   m_connDebug = settings.value("connDebug", false).toBool();
   m_displayAll = settings.value("displayAll", false).toBool();
   m_sqlDebug = settings.value("sqlDebug", false).toBool();
   m_commandDebug = settings.value("commandDebug", false).toBool();
   m_miscDebug = settings.value("miscDebug", false).toBool();
   settings.endGroup();
   settings.beginGroup("JobList");
   m_recordLimitCheck = settings.value("recordLimitCheck", true).toBool();
   m_recordLimitVal = settings.value("recordLimitVal", 50).toInt();
   m_daysLimitCheck = settings.value("daysLimitCheck", false).toBool();
   m_daysLimitVal = settings.value("daysLimitVal", 28).toInt();
   settings.endGroup();
   settings.beginGroup("Timers");
   m_checkMessages = settings.value("checkMessages", false).toBool();
   m_checkMessagesInterval = settings.value("checkMessagesInterval", 28).toInt();
   settings.endGroup();
   settings.beginGroup("Misc");
   m_longList = settings.value("longList", false).toBool();
   ItemFormatterBase::setBytesConversion(
         (ItemFormatterBase::BYTES_CONVERSION) settings.value("byteConvert",
         ItemFormatterBase::BYTES_CONVERSION_IEC).toInt());
   m_openPlot = settings.value("openplot", false).toBool();
   m_openBrowser = settings.value("openbrowser", false).toBool();
   m_openDirStat = settings.value("opendirstat", false).toBool();
   settings.endGroup();
   settings.beginGroup("RestoreTree");
   m_rtPopDirDebug = settings.value("rtPopDirDebug", false).toBool();
   m_rtDirCurICDebug = settings.value("rtDirCurICDebug", false).toBool();
   m_rtDirICDebug = settings.value("rtDirCurICRetDebug", false).toBool();
   m_rtFileTabICDebug = settings.value("rtFileTabICDebug", false).toBool();
   m_rtVerTabICDebug = settings.value("rtVerTabICDebug", false).toBool();
   m_rtUpdateFTDebug = settings.value("rtUpdateFTDebug", false).toBool();
   m_rtUpdateVTDebug = settings.value("rtUpdateVTDebug", false).toBool();
   m_rtChecksDebug = settings.value("rtChecksDebug", false).toBool();
   m_rtIconStateDebug = settings.value("rtIconStateDebug", false).toBool();
   m_rtRestore1Debug = settings.value("rtRestore1Debug", false).toBool();
   m_rtRestore2Debug = settings.value("rtRestore2Debug", false).toBool();
   m_rtRestore3Debug = settings.value("rtRestore3Debug", false).toBool();
   settings.endGroup();
}

void MainWin::setMessageIcon()
{
   if (m_currentConsole->is_messagesPending())
      actionMessages->setIcon(QIcon(QString::fromUtf8(":/images/mail-message-pending.png")));
   else
      actionMessages->setIcon(QIcon(QString::fromUtf8(":/images/mail-message-new.png")));
}

void MainWin::goToPreviousPage()
{
   m_treeStackTrap = true;
   bool done = false;
   while (!done) {
      /* If stack list is emtpty, then done */
      if (m_treeWidgetStack.isEmpty()) {
         done = true;
      } else {
         QTreeWidgetItem* testItem = m_treeWidgetStack.takeLast();
         QTreeWidgetItemIterator it(treeWidget);
         /* lets avoid a segfault by setting an item current that no longer exists */
         while (*it) {
            if (*it == testItem) {
               if (testItem != treeWidget->currentItem()) {
                  treeWidget->setCurrentItem(testItem);
                  done = true;
               }
               break;
            }
            ++it;
         }
      }
   }
   m_treeStackTrap = false;
}
