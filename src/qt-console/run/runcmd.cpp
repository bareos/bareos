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
 *  Run Command Dialog class
 *
 *  This is called when a Run Command signal is received from the
 *    Director. We parse the Director's output and throw up a 
 *    dialog box.  This happens, for example, after the user finishes
 *    selecting files to be restored. The Director will then submit a
 *    run command, that causes this page to be popped up.
 *
 *   Kern Sibbald, March MMVII
 *
 */ 

#include "bat.h"
#include "run.h"

/*
 * Setup all the combo boxes and display the dialog
 */
runCmdPage::runCmdPage(int conn) : Pages()
{
   m_name = tr("Restore Run");
   pgInitialize();
   setupUi(this);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/restore.png")));
   m_conn = conn;
   m_console->notify(conn, false);

   fill();
   m_console->discardToPrompt(m_conn);

   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   //dockPage();
   setCurrent();
   this->show();

}

void runCmdPage::fill()
{
   QString item, val;
   QStringList items;
   QRegExp rx("^.*:\\s*(\\S.*$)");   /* Regex to get value */

   clientCombo->addItems(m_console->client_list);
   filesetCombo->addItems(m_console->fileset_list);
   replaceCombo->addItems(QStringList() << tr("never") << tr("always") << tr("ifnewer") 
        << tr("ifolder"));
   replaceCombo->setCurrentIndex(replaceCombo->findText(tr("never"), Qt::MatchExactly));
   storageCombo->addItems(m_console->storage_list);
   dateTimeEdit->setDisplayFormat(mainWin->m_dtformat);

   m_console->read(m_conn);
   item = m_console->msg(m_conn);
   items = item.split("\n");
   foreach(item, items) {
      rx.indexIn(item);
      val = rx.cap(1);
      Dmsg1(100, "Item=%s\n", item.toUtf8().data());
      Dmsg1(100, "Value=%s\n", val.toUtf8().data());

      if (item.startsWith("Title:")) {
         run->setText(val);
      }
      if (item.startsWith("JobName:")) {
         jobCombo->addItem(val);
         continue;
      }
      if (item.startsWith("Bootstrap:")) {
         bootstrap->setText(val);
         continue;
      }
      if (item.startsWith("Backup Client:")) {
         clientCombo->setCurrentIndex(clientCombo->findText(val, Qt::MatchExactly));
         continue;
      }
      if (item.startsWith("Storage:")) {
         storageCombo->setCurrentIndex(storageCombo->findText(val, Qt::MatchExactly));
         continue;
      }
      if (item.startsWith("Where:")) {
         where->setText(val);
         continue;
      }
      if (item.startsWith("When:")) {
         dateTimeEdit->setDateTime(QDateTime::fromString(val,mainWin->m_dtformat));
         continue;
      }
      if (item.startsWith("Catalog:")) {
         catalogCombo->addItem(val);
         continue;
      }
      if (item.startsWith("FileSet:")) {
         filesetCombo->setCurrentIndex(filesetCombo->findText(val, Qt::MatchExactly));
         continue;
      }
      if (item.startsWith("Priority:")) {
         bool okay;
         int pri = val.toInt(&okay, 10);
         if (okay) 
            prioritySpin->setValue(pri);
         continue;
      }
      if (item.startsWith("Replace:")) {
         int replaceIndex = replaceCombo->findText(val, Qt::MatchExactly);
         if (replaceIndex >= 0)
            replaceCombo->setCurrentIndex(replaceIndex);
         continue;
      }
   }
}

void runCmdPage::okButtonPushed()
{
   QString cmd(".mod");
   cmd += " restoreclient=\"" + clientCombo->currentText() + "\"";
   cmd += " fileset=\"" + filesetCombo->currentText() + "\"";
   cmd += " storage=\"" + storageCombo->currentText() + "\"";
   cmd += " replace=\"" + replaceCombo->currentText() + "\"";
   cmd += " when=\"" + dateTimeEdit->dateTime().toString(mainWin->m_dtformat) + "\"";
   cmd += " bootstrap=\"" + bootstrap->text() + "\"";
   cmd += " where=\"" + where->text() + "\"";
   QString pri;
   QTextStream(&pri) << " priority=\"" << prioritySpin->value() << "\"";
   cmd += pri;
   cmd += " yes\n";
   
   setConsoleCurrent();
   QString displayhtml("<font color=\"blue\">");
   displayhtml += cmd + "</font>\n";
   m_console->display_html(displayhtml);
   m_console->display_text("\n");
   m_console->write_dir(m_conn, cmd.toUtf8().data());
   m_console->displayToPrompt(m_conn);
//   consoleCommand(cmd); ***FIXME set back to consoleCommand when connection issue is resolved

   m_console->notify(m_conn, true);
   closeStackPage();
}


void runCmdPage::cancelButtonPushed()
{
   m_console->displayToPrompt(m_conn);
   m_console->write_dir(".");
   m_console->displayToPrompt(m_conn);
   mainWin->set_status(tr(" Canceled"));
   this->hide();
   m_console->notify(m_conn, true);
   closeStackPage();
   mainWin->resetFocus();
}
