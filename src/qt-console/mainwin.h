/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2009 Free Software Foundation Europe e.V.

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
 *   Version $Id$
 *
 * qt-console main window class definition.
 *
 *  Written by Kern Sibbald, January MMVII
 */

#ifndef _MAINWIN_H_
#define _MAINWIN_H_

#include <QtGui>
#include <QList>
#include "ui_main.h"

class Console;
class Pages;

class MainWin : public QMainWindow, public Ui::MainForm    
{
   Q_OBJECT

public:
   MainWin(QWidget *parent = 0);
   void set_statusf(const char *fmt, ...);
   void set_status_ready();
   void set_status(const char *buf);
   void set_status(const QString &str);
   void writeSettings();
   void readSettings();
   void resetFocus() { lineEdit->setFocus(); };
   void hashInsert(QTreeWidgetItem *, Pages *);
   void hashRemove(Pages *);
   void hashRemove(QTreeWidgetItem *, Pages *);
   void setMessageIcon();
   bool getWaitState() {return m_waitState; };
   bool isClosing() {return m_isClosing; };
   Console *currentConsole();
   QTreeWidgetItem *currentTopItem();
   Pages* getFromHash(QTreeWidgetItem *);
   QTreeWidgetItem* getFromHash(Pages *);
   /* This hash is to get the page when the page selector widget is known */
   QHash<QTreeWidgetItem*,Pages*> m_pagehash;
   /* This hash is to get the page selector widget when the page is known */
   QHash<Pages*,QTreeWidgetItem*> m_widgethash;
   /* This is a list of consoles */
   QHash<QTreeWidgetItem*,Console*> m_consoleHash;
   void createPageJobList(const QString &, const QString &,
            const QString &, const QString &, QTreeWidgetItem *);
   QString m_dtformat;
   /* Begin Preferences variables */
   bool m_commDebug;
   bool m_connDebug;
   bool m_displayAll;
   bool m_sqlDebug;
   bool m_commandDebug;
   bool m_miscDebug;
   bool m_recordLimitCheck;
   int m_recordLimitVal;
   bool m_daysLimitCheck;
   int m_daysLimitVal;
   bool m_checkMessages;
   int m_checkMessagesInterval;
   bool m_longList;
   bool m_rtPopDirDebug;
   bool m_rtDirCurICDebug;
   bool m_rtDirICDebug;
   bool m_rtFileTabICDebug;
   bool m_rtVerTabICDebug;
   bool m_rtUpdateFTDebug;
   bool m_rtUpdateVTDebug;
   bool m_rtChecksDebug;
   bool m_rtIconStateDebug;
   bool m_rtRestore1Debug;
   bool m_rtRestore2Debug;
   bool m_rtRestore3Debug;
   bool m_openBrowser;
   bool m_openPlot;
   bool m_openDirStat;

   /* Global */
   bool m_notify;                     /* global flag to turn on/off all notifiers */

public slots:
   void input_line();
   void about();
   void help();
   void treeItemClicked(QTreeWidgetItem *item, int column);
   void labelButtonClicked();
   void runButtonClicked();
   void estimateButtonClicked();
   void browseButtonClicked();
   void statusPageButtonClicked();
   void jobPlotButtonClicked();
   void restoreButtonClicked();
   void undockWindowButton();
   void treeItemChanged(QTreeWidgetItem *, QTreeWidgetItem *);
   void stackItemChanged(int);
   void toggleDockContextWindow();
   void closePage(int item);
   void closeCurrentPage();
   void setPreferences();
   void readPreferences();
   void waitEnter();
   void waitExit();
   void repopLists();
   void reloadRepopLists();
   void popLists();
   void goToPreviousPage();

protected:
   void closeEvent(QCloseEvent *event);
   void keyPressEvent(QKeyEvent *event);

private:
   void connectConsole();
   void createPages();
   void connectSignals(); 
   void disconnectSignals(); 
   void connectConsoleSignals();
   void disconnectConsoleSignals(Console *console);

private:
   Console *m_currentConsole;
   Pages *m_pagespophold;
   QStringList m_cmd_history;
   int m_cmd_last;
   QTreeWidgetItem *m_firstItem;
   QTreeWidgetItem *m_waitTreeItem;
   bool m_isClosing;
   bool m_waitState;
   bool m_doConnect;
   QList<QTreeWidgetItem *> m_treeWidgetStack;
   bool m_treeStackTrap;
};

#include "ui_prefs.h"

class prefsDialog : public QDialog, public Ui::PrefsForm
{
   Q_OBJECT

public:
   prefsDialog();

private slots:
   void accept();
   void reject();
};

#endif /* _MAINWIN_H_ */
