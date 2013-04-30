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
 *   Dirk Bartley, March 2007
 */

#include "bat.h"
#include "pages.h"

/* A global function */
bool isWin32Path(QString &fullPath) 
{
   if (fullPath.size()<2) {
      return false;
   }

   bool toret = fullPath[1].toAscii() == ':' && fullPath[0].isLetter();
   if (mainWin->m_miscDebug) {
      if (toret)
         Pmsg1(000, "returning from isWin32Path true %s\n", fullPath.toUtf8().data());
      else
         Pmsg1(000, "returning from isWin32Path false %s\n", fullPath.toUtf8().data());
   }
   return toret;
}

/* Need to initialize variables here */
Pages::Pages() : QWidget()
{
   m_docked = false;
   m_onceDocked = false;
   m_closeable = true;
   m_dockOnFirstUse = true;
   m_console = NULL;
   m_parent = NULL;
}

/* first Use Dock */
void Pages::firstUseDock()
{
   if (!m_onceDocked && m_dockOnFirstUse) {
      dockPage();
   }
}

/*
 * dockPage
 * This function is intended to be called from within the Pages class to pull
 * a window from floating to in the stack widget.
 */

void Pages::dockPage()
{
   if (isDocked()) {
      return;
   }

   /* These two lines are for making sure if it is being changed from a window
    * that it has the proper window flag and parent.
    */
   setWindowFlags(Qt::Widget);

   /* calculate the index that the tab should be inserted into */
   int tabPos = 0;
   QTreeWidgetItemIterator it(mainWin->treeWidget);
   while (*it) {
      Pages *somepage = mainWin->getFromHash(*it);
      if (this == somepage) {
         tabPos += 1;
         break;
      }
      int pageindex = mainWin->tabWidget->indexOf(somepage);
      if (pageindex != -1) { tabPos = pageindex; }
      ++it;
   }

   /* This was being done already */
   m_parent->insertTab(tabPos, this, m_name);

   /* Set docked flag */
   m_docked = true;
   m_onceDocked = true;
   mainWin->tabWidget->setCurrentWidget(this);
   /* lets set the page selectors action for docking or undocking */
   setContextMenuDockText();
}

/*
 * undockPage
 * This function is intended to be called from within the Pages class to put
 * a window from the stack widget to a floating window.
 */

void Pages::undockPage()
{
   if (!isDocked()) {
      return;
   }

   /* Change from a stacked widget to a normal window */
   m_parent->removeTab(m_parent->indexOf(this));
   setWindowFlags(Qt::Window);
   show();
   /* Clear docked flag */
   m_docked = false;
   /* The window has been undocked, lets change the context menu */
   setContextMenuDockText();
}

/*
 * This function is intended to be called with the subclasses.  When it is 
 * called the specific sublclass does not have to be known to Pages.  When it 
 * is called this function will change the page from it's current state of being
 * docked or undocked and change it to the other.
 */

void Pages::togglePageDocking()
{
   if (m_docked) {
      undockPage();
   } else {
      dockPage();
   }
}

/*
 * This function is because I wanted for some reason to keep it protected but still 
 * give any subclasses the ability to find out if it is currently stacked or not.
 */
bool Pages::isDocked()
{
   return m_docked;
}

/*
 * This function is because after the tabbed widget was added I could not tell
 * from is docked if it had been docked yet.  To prevent status pages from requesting
 * status from the director
 */
bool Pages::isOnceDocked()
{
   return m_onceDocked;
}


/*
 * To keep m_closeable protected as well
 */
bool Pages::isCloseable()
{
   return m_closeable;
}

void Pages::hidePage()
{
   if (!m_parent || (m_parent->indexOf(this) <= 0)) {
      return;
   }
   /* Remove any tab that may exist */
   m_parent->removeTab(m_parent->indexOf(this));
   hide();
   /* Clear docked flag */
   m_docked = false;
   /* The window has been undocked, lets change the context menu */
   setContextMenuDockText();
}

/*
 * When a window is closed, this slot is called.  The idea is to put it back in the
 * stack here, and it works.  I wanted to get it to the top of the stack so that the
 * user immediately sees where his window went.  Also, if he undocks the window, then
 * closes it with the tree item highlighted, it may be confusing that the highlighted
 * treewidgetitem is not the stack item in the front.
 */

void Pages::closeEvent(QCloseEvent* event)
{
   /* A Widget was closed, lets toggle it back into the window, and set it in front. */
   dockPage();

   /* this fixes my woes of getting the widget to show up on top when closed */
   event->ignore();

   /* Set the current tree widget item in the Page Selector window to the item 
    * which represents "this" 
    * Which will also bring "this" to the top of the stacked widget */
   setCurrent();
}

/*
 * The next three are virtual functions.  The idea here is that each subclass will have the
 * built in virtual function to override if the programmer wants to populate the window
 * when it it is first clicked.
 */
void Pages::PgSeltreeWidgetClicked()
{
}

/*
 *  Virtual function which is called when this page is visible on the stack.
 *  This will be overridden by classes that want to populate if they are on the
 *  top.
 */
void Pages::currentStackItem()
{
}

/*
 * Function to close the stacked page and remove the widget from the
 * Page selector window
 */
void Pages::closeStackPage()
{
   /* First get the tree widget item and destroy it */
   QTreeWidgetItem *item=mainWin->getFromHash(this);
   /* remove the QTreeWidgetItem <-> page from the hash */
   if (item) {
      mainWin->hashRemove(item, this);
      /* remove the item from the page selector by destroying it */
      delete item;
   }
   /* remove this */
   delete this;
}

/*
 * Function to set members from the external mainwin and it's overload being
 * passed a specific QTreeWidgetItem to be it's parent on the tree
 */
void Pages::pgInitialize()
{
   pgInitialize(QString(), NULL);
}

void Pages::pgInitialize(const QString &name)
{
   pgInitialize(name, NULL);
}

void Pages::pgInitialize(const QString &tname, QTreeWidgetItem *parentTreeWidgetItem)
{
   m_docked = false;
   m_onceDocked = false;
   if (tname.size()) {
      m_name = tname;
   }
   m_parent = mainWin->tabWidget;
   m_console = mainWin->currentConsole();

   if (!parentTreeWidgetItem) {
      parentTreeWidgetItem = m_console->directorTreeItem();
   }

   QTreeWidgetItem *item = new QTreeWidgetItem(parentTreeWidgetItem);
   QString name; 
   treeWidgetName(name);
   item->setText(0, name);
   mainWin->hashInsert(item, this);
   setTitle();
}

/*
 * Virtual Function to return a name
 * All subclasses should override this function
 */
void Pages::treeWidgetName(QString &name)
{
   name = m_name;
}

/*
 * Function to simplify executing a console command and bringing the
 * console to the front of the stack
 */
void Pages::consoleCommand(QString &command)
{
   consoleCommand(command, true);
}

void Pages::consoleCommand(QString &command, bool setCurrent)
{
   int conn;
   bool donotify = false;
   if (m_console->getDirComm(conn))  {
      if (m_console->is_notify_enabled(conn)) {
         donotify = true;
         m_console->notify(conn, false);
      }
      consoleCommand(command, conn, setCurrent);
      if (donotify) { m_console->notify(conn, true); }
   }
}

void Pages::consoleCommand(QString &command, int conn)
{
   consoleCommand(command, conn, true);
}

void Pages::consoleCommand(QString &command, int conn, bool setCurrent)
{
   /* Bring this director's console to the front of the stack */
   if (setCurrent) { setConsoleCurrent(); }
   QString displayhtml("<font color=\"blue\">");
   displayhtml += command + "</font>\n";
   m_console->display_html(displayhtml);
   m_console->display_text("\n");
   mainWin->waitEnter();
   m_console->write_dir(conn, command.toUtf8().data(), false);
   m_console->displayToPrompt(conn);
   mainWin->waitExit();
}

/*
 * Function for handling undocked windows becoming active.
 * Change the currently selected item in the page selector window to the now
 * active undocked window.  This will also make the console for the undocked
 * window m_currentConsole.
 */
void Pages::changeEvent(QEvent *event)
{
   if ((event->type() ==  QEvent::ActivationChange) && (isActiveWindow())) {
      setCurrent();
   }
}

/*
 * Function to simplify getting the name of the class and the director into
 * the caption or title of the window
 */
void Pages::setTitle()
{
   QString wdgname, director;
   treeWidgetName(wdgname);
   m_console->getDirResName(director);
   QString title = tr("%1 of Director %2").arg(wdgname).arg(director);
   setWindowTitle(title);
}

/*
 * Bring the current directors console window to the top of the stack.
 */
void Pages::setConsoleCurrent()
{
   mainWin->treeWidget->setCurrentItem(mainWin->getFromHash(m_console));
}

/*
 * Bring this window to the top of the stack.
 */
void Pages::setCurrent()
{
   mainWin->treeWidget->setCurrentItem(mainWin->getFromHash(this));
}

/*
 * Function to set the text of the toggle dock context menu when page and
 * widget item are NOT known.  
 */
void Pages::setContextMenuDockText()
{
   QTreeWidgetItem *item = mainWin->getFromHash(this);
   QString docktext;
   if (isDocked()) {
      docktext = tr("UnDock %1 Window").arg(item->text(0));
   } else {
      docktext = tr("ReDock %1 Window").arg(item->text(0));
   }
      
   mainWin->actionToggleDock->setText(docktext);
   setTreeWidgetItemDockColor();
}

/*
 * Function to set the color of the tree widget item based on whether it is
 * docked or not.
 */
void Pages::setTreeWidgetItemDockColor()
{
   QTreeWidgetItem* item = mainWin->getFromHash(this);
   if (item) {
      if (item->text(0) != tr("Console")) {
         if (isDocked()) {
         /* Set the brush to blue if undocked */
            QBrush blackBrush(Qt::black);
            item->setForeground(0, blackBrush);
         } else {
         /* Set the brush back to black if docked */
            QBrush blueBrush(Qt::blue);
            item->setForeground(0, blueBrush);
         }
      }
   }
}

/* Function to get a list of volumes */
void Pages::getVolumeList(QStringList &volumeList)
{
   QString query("SELECT VolumeName AS Media FROM Media ORDER BY Media");
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",query.toUtf8().data());
   }
   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;
      /* Iterate through the lines of results. */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         volumeList.append(fieldlist[0]);
      } /* foreach resultline */
   } /* if results from query */
}

/* Function to get a list of volumes */
void Pages::getStatusList(QStringList &statusLongList)
{
   QString statusQuery("SELECT JobStatusLong FROM Status");
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",statusQuery.toUtf8().data());
   }
   QStringList statusResults;
   if (m_console->sql_cmd(statusQuery, statusResults)) {
      QString field;
      QStringList fieldlist;
      /* Iterate through the lines of results. */
      foreach (QString resultline, statusResults) {
         fieldlist = resultline.split("\t");
         statusLongList.append(fieldlist[0]);
      } /* foreach resultline */
   } /* if results from statusquery */
}
