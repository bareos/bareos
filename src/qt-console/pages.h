#ifndef _PAGES_H_
#define _PAGES_H_
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

#include <QtGui>
#include <QList>

/*
 *  The Pages Class
 *
 *  This class is inherited by all widget windows which are on the stack
 *  It is for the purpose of having a consistent set of functions and properties
 *  in all of the subclasses to accomplish tasks such as pulling a window out
 *  of or into the stack.  It also provides virtual functions called
 *  from in mainwin so that subclasses can contain functions to allow them
 *  to populate the screens at the time of first viewing, (when selected) as
 *  opposed to  the first creation of the console connection.  The 
 *  console is not connected until after the page selector tree has been
 *  populated.
 */

class Console;

class Pages : public QWidget
{
public:
   /* methods */
   Pages();
   void dockPage();
   void undockPage();
   void hidePage();
   void togglePageDocking();
   bool isDocked();
   bool isOnceDocked();
   bool isCloseable();
   virtual void PgSeltreeWidgetClicked();
   virtual void currentStackItem();
   void closeStackPage();
   Console *console() { return m_console; };
   void setCurrent();
   void setContextMenuDockText();
   void setTreeWidgetItemDockColor();
   void consoleCommand(QString &);
   void consoleCommand(QString &, int conn);
   void consoleCommand(QString &, bool setCurrent);
   void consoleCommand(QString &, int conn, bool setCurrent);
   QString &name() { return m_name; };
   void getVolumeList(QStringList &);
   void getStatusList(QStringList &);
   void firstUseDock();

   /* members */
   QTabWidget *m_parent;
   QList<QAction*> m_contextActions;


public slots:
   /* closeEvent is a virtual function inherited from QWidget */
   virtual void closeEvent(QCloseEvent* event);

protected:
   /* methods */
   void pgInitialize();
   void pgInitialize(const QString &);
   void pgInitialize(const QString &, QTreeWidgetItem *);
   virtual void treeWidgetName(QString &);
   virtual void changeEvent(QEvent *event);
   void setConsoleCurrent();
   void setTitle();

   /* members */
   bool m_closeable;
   bool m_docked;
   bool m_onceDocked;
   bool m_dockOnFirstUse;
   Console *m_console;
   QString m_name;
};

#endif /* _PAGES_H_ */
