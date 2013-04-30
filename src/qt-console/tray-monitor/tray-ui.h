/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2011-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

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

#ifndef TRAYUI_H
#define TRAYUI_H

#ifdef HAVE_WIN32
# ifndef _STAT_DEFINED
#  define _STAT_DEFINED 1 /* don't pull in MinGW struct stat from wchar.h */
# endif
#endif

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QSpinBox>
#include <QMenu>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QDebug>
#include <QMessageBox>
#include <QFont>

#include "version.h"
#include "ui/ui_run.h"
#include "tray-monitor.h"

class RunDlg: public QDialog, public Ui::runForm
{
   Q_OBJECT

public:
   monitoritem *item;

   void fill(QComboBox *cb, QStringList &lst) {
      if (lst.length()) {
         cb->addItems(lst);
      } else {
         cb->setEnabled(false);
      }
   }
   RunDlg(monitoritem *i) {
      struct resources res;
      struct job_defaults jdefault;
      QDateTime dt;
      item = i;

      qDebug() << "start getting elements";
      get_list(item, ".jobs type=B", res.job_list);
      
      if (res.job_list.length() == 0) {
         QMessageBox msgBox;
         msgBox.setText("This restricted console doesn't have access to Backup jobs");
         msgBox.setIcon(QMessageBox::Warning);
         msgBox.exec();
         this->deleteLater();
         return;
      }

      get_list(item, ".pools", res.pool_list);
      get_list(item, ".clients", res.client_list);
      get_list(item, ".storage", res.storage_list);
      res.levels << "Full" << "Incremental" << "Differential";
      get_list(item, ".filesets", res.fileset_list);
      get_list(item, ".messages", res.messages_list);

      setupUi(this);

      qDebug() << "  -> done";
      label_5->setVisible(false);
      bootstrap->setVisible(false);
      jobCombo->addItems(res.job_list);
      fill(filesetCombo, res.fileset_list);
      fill(levelCombo, res.levels);
      fill(clientCombo, res.client_list);
      fill(poolCombo, res.pool_list);
      fill(storageCombo, res.storage_list);
      dateTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
      dateTimeEdit->setDateTime(dt.currentDateTime());
      fill(messagesCombo, res.messages_list);
      messagesCombo->setEnabled(false);
      job_name_change(0);
      connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(job_name_change(int)));
      connect(cancelButton, SIGNAL(pressed()), this, SLOT(deleteLater()));
      connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
      show();
   }

private slots:

   void okButtonPushed()
   {
      QString cmd;
      cmd = "run";
      if (jobCombo->isEnabled()) {
         cmd += " job=\"" + jobCombo->currentText() + "\"" ;
      }
      if (filesetCombo->isEnabled()) {
         cmd += " fileset=\"" + filesetCombo->currentText() + "\"";
      }
      cmd += " level=\"" + levelCombo->currentText() + "\"";
      if (clientCombo->isEnabled()) {
         cmd += " client=\"" + clientCombo->currentText() + "\"" ;
      }
      if (poolCombo->isEnabled()) {
         cmd += " pool=\"" + poolCombo->currentText() + "\"";
      }
      if (storageCombo->isEnabled()) {
         cmd += " storage=\"" + storageCombo->currentText() + "\"";
      }
      cmd += " priority=\"" + QString().setNum(prioritySpin->value()) + "\"";
      cmd += " when=\"" + dateTimeEdit->dateTime().toString("yyyy-MM-dd hh:mm:ss") + "\"";
#ifdef xxx
      " messages=\"" << messagesCombo->currentText() << "\"";
     /* FIXME when there is an option to modify the messages resoruce associated
      * with a  job */
#endif
      cmd += " yes";
      qDebug() << cmd;
      item->D_sock->fsend("%s", cmd.toUtf8().data());
      QString output;
      while(bnet_recv(item->D_sock) >= 0) {output += item->D_sock->msg;}
      QMessageBox msgBox;
      msgBox.setText(output);
      msgBox.exec();
      deleteLater();
   }

   void job_name_change(int)
   {
      job_defaults job_defs;
      job_defs.job_name = jobCombo->currentText();

      if (item->get_job_defaults(job_defs)) {
         typeLabel->setText("<H3>"+job_defs.type+"</H3>");
         filesetCombo->setCurrentIndex(filesetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
         levelCombo->setCurrentIndex(levelCombo->findText(job_defs.level, Qt::MatchExactly));
         clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
         poolCombo->setCurrentIndex(poolCombo->findText(job_defs.pool_name, Qt::MatchExactly));
         storageCombo->setCurrentIndex(storageCombo->findText(job_defs.store_name, Qt::MatchExactly));
         messagesCombo->setCurrentIndex(messagesCombo->findText(job_defs.messages_name, Qt::MatchExactly));

      } else {

      }
   }
};

void refresh_item();
void dotest();

class TrayUI: public QMainWindow
{
   Q_OBJECT

public:
    QWidget *centralwidget;
    QTabWidget *tabWidget;
    QStatusBar *statusbar;
    QHash<QString, QPlainTextEdit*> hash;
    monitoritem* director;

    QSystemTrayIcon *tray;
    QSpinBox *spinRefresh;
    QTimer *timer;

    QPlainTextEdit *getTextEdit(char *title)
    {
       return hash.value(QString(title));
    }

    void clearText(char *title) 
    {
       QPlainTextEdit *w = getTextEdit(title);
       if (!w) {
          return;
       }
       w->clear();
    }

    void appendText(char *title, char *line) 
    {
       QPlainTextEdit *w = getTextEdit(title);
       if (!w) {
          return;
       }
       w->appendPlainText(QString(line));
    }

    void addDirector(monitoritem *item)
    {
       director = item;
    }

    void addTab(char *title)
    {
       QString t = QString(title);
       QWidget *tab = new QWidget();
       QVBoxLayout *vLayout = new QVBoxLayout(tab);
       QPlainTextEdit *plainTextEdit = new QPlainTextEdit(tab);
       plainTextEdit->setObjectName(t);
       plainTextEdit->setReadOnly(true);
       plainTextEdit->setFont(QFont("courier"));
       vLayout->addWidget(plainTextEdit);
       hash.insert(t, plainTextEdit);
       tabWidget->addTab(tab, t);
    }

    void startTimer()
    {
       if (!timer) {
          timer = new QTimer(this);
          connect(timer, SIGNAL(timeout()), this, SLOT(refresh_screen()));
       }
       timer->start(spinRefresh->value()*1000);
    }

    void setupUi(QMainWindow *TrayMonitor)
    {
        timer = NULL;
        director = NULL;
        if (TrayMonitor->objectName().isEmpty())
            TrayMonitor->setObjectName(QString::fromUtf8("TrayMonitor"));
        TrayMonitor->setWindowIcon(QIcon(":/images/cartridge1.png")); 
        TrayMonitor->resize(789, 595);
        centralwidget = new QWidget(TrayMonitor);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        QVBoxLayout *verticalLayout = new QVBoxLayout(centralwidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        tabWidget = new QTabWidget(centralwidget);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setTabPosition(QTabWidget::North);
        tabWidget->setTabShape(QTabWidget::Rounded);
        tabWidget->setTabsClosable(false);
        verticalLayout->addWidget(tabWidget);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(centralwidget);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setStandardButtons(QDialogButtonBox::Close);
        connect(buttonBox, SIGNAL(rejected()), this, SLOT(cb_show()));

        TrayMonitor->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(TrayMonitor);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        TrayMonitor->setStatusBar(statusbar);

        QHBoxLayout *hLayout = new QHBoxLayout();
        QLabel *refreshlabel = new QLabel(centralwidget);
        refreshlabel->setText("Refresh:");
        hLayout->addWidget(refreshlabel);
        spinRefresh = new QSpinBox(centralwidget);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(spinRefresh->sizePolicy().hasHeightForWidth());
        spinRefresh->setSizePolicy(sizePolicy);
        spinRefresh->setMinimum(1);
        spinRefresh->setMaximum(600);
        spinRefresh->setSingleStep(10);
        spinRefresh->setValue(60);
        hLayout->addWidget(spinRefresh);
        hLayout->addWidget(buttonBox);

        verticalLayout->addLayout(hLayout);

        tray = new QSystemTrayIcon(TrayMonitor);
        QMenu* stmenu = new QMenu(TrayMonitor);

        QAction *actShow = new QAction(QApplication::translate("TrayMonitor",
                               "Display",
                                0, QApplication::UnicodeUTF8),TrayMonitor);
        QAction* actQuit = new QAction(QApplication::translate("TrayMonitor",
                               "Quit",
                                0, QApplication::UnicodeUTF8),TrayMonitor);
        QAction* actAbout = new QAction(QApplication::translate("TrayMonitor",
                               "About",
                                0, QApplication::UnicodeUTF8),TrayMonitor);
	QAction* actRun = new QAction(QApplication::translate("TrayMonitor",
                               "Run...",
                                0, QApplication::UnicodeUTF8),TrayMonitor);
	stmenu->addAction(actShow);
        stmenu->addAction(actRun);
        stmenu->addAction(actAbout);
        stmenu->addSeparator();
	stmenu->addAction(actQuit);
        
        connect(actRun, SIGNAL(triggered()), this, SLOT(cb_run()));
        connect(actShow, SIGNAL(triggered()), this, SLOT(cb_show()));
        connect(actQuit, SIGNAL(triggered()), this, SLOT(cb_quit()));
        connect(actAbout, SIGNAL(triggered()), this, SLOT(cb_about()));
        connect(spinRefresh, SIGNAL(valueChanged(int)), this, SLOT(cb_refresh(int)));
        connect(tray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
                this, SLOT(cb_trayIconActivated(QSystemTrayIcon::ActivationReason)));

	tray->setContextMenu(stmenu);
	QIcon icon(":/images/cartridge1.png");
	tray->setIcon(icon);
        tray->setToolTip(QString("Bacula Tray Monitor"));
	tray->show();

        retranslateUi(TrayMonitor);
        QMetaObject::connectSlotsByName(TrayMonitor);
    } // setupUi

    void retranslateUi(QMainWindow *TrayMonitor)
    {
       TrayMonitor->setWindowTitle(QApplication::translate("TrayMonitor", "Bacula Tray Monitor", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

private slots:
    void cb_quit() {
       QApplication::quit();
    }

    void cb_refresh(int val) {
       if (timer) {
          timer->setInterval(val*1000);
       }
    }

    void cb_about() {
       QMessageBox::about(this, "Bacula Tray Monitor", "Bacula Tray Monitor\n"
                          "For more information, see: www.baculasystems.com\n"
                          "Copyright (C) 1999-2010, Bacula Systems(R) SA\n"
                          "Licensed under GNU AGPLv3.");
    }

    void cb_run() {
       if (director) {
          RunDlg *runbox = new RunDlg(director);
          runbox->show();
       }
    }

    void cb_trayIconActivated(QSystemTrayIcon::ActivationReason r) {
       if (r == QSystemTrayIcon::Trigger) {
          cb_show();
       }
    }

    void refresh_screen() {
//       qDebug() << "refresh_screen()";
       if (isVisible()) {
          refresh_item();
//          qDebug() << "  -> OK";
       }
    }

    void cb_show() {
       if (isVisible()) {
          hide();
       } else {
          refresh_item();
          show();
       }
    }
};


#endif  /* TRAYUI_H */
