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
 *  Run Dialog class
 *
 *   Kern Sibbald, February MMVII
 *
 */ 

#include "bat.h"
#include "run.h"


runPage::runPage() : Pages()
{
   init();
   show();
}

runPage::runPage(const QString &defJob) : Pages()
{
   m_dockOnFirstUse = false;
   init();
   if (defJob != "")
      jobCombo->setCurrentIndex(jobCombo->findText(defJob, Qt::MatchExactly));
   show();
}


runPage::runPage(const QString &defJob, const QString &level, 
                 const QString &pool, const QString &storage,
                 const QString &client, const QString &fileset)
   : Pages()
{
   m_dockOnFirstUse = false;
   init();
   jobCombo->setCurrentIndex(jobCombo->findText(defJob, Qt::MatchExactly));
   job_name_change(0);
   filesetCombo->setCurrentIndex(filesetCombo->findText(fileset,
                                                        Qt::MatchExactly));
   levelCombo->setCurrentIndex(levelCombo->findText(level, Qt::MatchExactly));
   clientCombo->setCurrentIndex(clientCombo->findText(client,Qt::MatchExactly));
   poolCombo->setCurrentIndex(poolCombo->findText(pool, Qt::MatchExactly));

   if (storage != "") {         // TODO: enable storage
      storageCombo->setCurrentIndex(storageCombo->findText(storage, 
                                                           Qt::MatchExactly));
   }
   show();
}


/*
 * Setup all the combo boxes and display the dialog
 */
void runPage::init()
{
   QDateTime dt;
   QDesktopWidget *desk = QApplication::desktop(); 
   QRect scrn;

   m_name = tr("Run");
   pgInitialize();
   setupUi(this);
   /* Get screen rectangle */
   scrn = desk->screenGeometry(desk->primaryScreen());
   /* Position this window in the middle of the screen */
   this->move((scrn.width()-this->width())/2, (scrn.height()-this->height())/2);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/run.png")));
   m_conn = m_console->notifyOff();

   m_console->beginNewCommand(m_conn);
   jobCombo->addItems(m_console->job_list);
   filesetCombo->addItems(m_console->fileset_list);
   levelCombo->addItems(m_console->level_list);
   clientCombo->addItems(m_console->client_list);
   poolCombo->addItems(m_console->pool_list);
   storageCombo->addItems(m_console->storage_list);
   dateTimeEdit->setDisplayFormat(mainWin->m_dtformat);
   dateTimeEdit->setDateTime(dt.currentDateTime());
   /*printf("listing messages resources");  ***FIME ***
   foreach(QString mes, m_console->messages_list) {
      printf("%s\n", mes.toUtf8().data());
   }*/
   messagesCombo->addItems(m_console->messages_list);
   messagesCombo->setEnabled(false);
   job_name_change(0);
   connect(jobCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(job_name_change(int)));
   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));

   // find a way to place the new window at the cursor position
   // or in the middle of the page
//   dockPage();
   setCurrent();
}

void runPage::okButtonPushed()
{
   this->hide();
   QString cmd;
   QTextStream(&cmd) << "run" << 
      " job=\"" << jobCombo->currentText() << "\"" <<
      " fileset=\"" << filesetCombo->currentText() << "\"" <<
      " level=\"" << levelCombo->currentText() << "\"" <<
      " client=\"" << clientCombo->currentText() << "\"" <<
      " pool=\"" << poolCombo->currentText() << "\"" <<
      " storage=\"" << storageCombo->currentText() << "\"" <<
      " priority=\"" << prioritySpin->value() << "\""
      " when=\"" << dateTimeEdit->dateTime().toString(mainWin->m_dtformat) << "\"";
#ifdef xxx
      " messages=\"" << messagesCombo->currentText() << "\"";
     /* FIXME when there is an option to modify the messages resoruce associated
      * with a  job */
#endif
   if (bootstrap->text() != "") {
      cmd += " bootstrap=\"" + bootstrap->text() + "\""; 
   }
   cmd += " yes";

   if (mainWin->m_commandDebug) {
      Pmsg1(000, "command : %s\n", cmd.toUtf8().data());
   }

   consoleCommand(cmd);
   m_console->notify(m_conn, true);
   closeStackPage();
   mainWin->resetFocus();
}


void runPage::cancelButtonPushed()
{
   mainWin->set_status(tr(" Canceled"));
   this->hide();
   m_console->notify(m_conn, true);
   closeStackPage();
   mainWin->resetFocus();
}

/*
 * Called here when the jobname combo box is changed.
 *  We load the default values for the new job in the
 *  other combo boxes.
 */
void runPage::job_name_change(int index)
{
   job_defaults job_defs;

   (void)index;
   job_defs.job_name = jobCombo->currentText();
   if (m_console->get_job_defaults(job_defs)) {
      QString cmd;
      typeLabel->setText("<H3>"+job_defs.type+"</H3>");
      filesetCombo->setCurrentIndex(filesetCombo->findText(job_defs.fileset_name, Qt::MatchExactly));
      levelCombo->setCurrentIndex(levelCombo->findText(job_defs.level, Qt::MatchExactly));
      clientCombo->setCurrentIndex(clientCombo->findText(job_defs.client_name, Qt::MatchExactly));
      poolCombo->setCurrentIndex(poolCombo->findText(job_defs.pool_name, Qt::MatchExactly));
      storageCombo->setCurrentIndex(storageCombo->findText(job_defs.store_name, Qt::MatchExactly));
      messagesCombo->setCurrentIndex(messagesCombo->findText(job_defs.messages_name, Qt::MatchExactly));
      m_console->level_list.clear();
      cmd = ".levels " + job_defs.type;
      m_console->dir_cmd(cmd, m_console->level_list);
      levelCombo->clear();
      levelCombo->addItems(m_console->level_list);
      levelCombo->setCurrentIndex(levelCombo->findText(job_defs.level, 0 /*Qt::MatchExactly*/));
   }
}
