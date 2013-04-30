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
 *  Select dialog class
 *
 *   Kern Sibbald, March MMVII
 *
 */ 

#include "bat.h"
#include "select.h"

/*
 * Read the items for the selection
 */
selectDialog::selectDialog(Console *console, int conn) : QDialog()
{
   m_conn = conn;
   QDateTime dt;
   int stat;
   QListWidgetItem *item;
   int row = 0;

   m_console = console;
   m_console->notify(m_conn, false);
   setupUi(this);
   connect(listBox, SIGNAL(currentRowChanged(int)), this, SLOT(index_change(int)));
   setAttribute(Qt::WA_DeleteOnClose);
   m_console->read(m_conn);                 /* get title */
   labelWidget->setText(m_console->msg(m_conn));
   while ((stat=m_console->read(m_conn)) > 0) {
      item = new QListWidgetItem;
      item->setText(m_console->msg(m_conn));
      listBox->insertItem(row++, item);
   }
   m_console->displayToPrompt(m_conn);
   this->show();
}

void selectDialog::accept()
{
   char cmd[100];

   this->hide();
   bsnprintf(cmd, sizeof(cmd), "%d", m_index+1);
   m_console->write_dir(m_conn, cmd);
   m_console->displayToPrompt(m_conn);
   this->close();
   mainWin->resetFocus();
   m_console->displayToPrompt(m_conn);
   m_console->notify(m_conn, true);
}


void selectDialog::reject()
{
   this->hide();
   mainWin->set_status(tr(" Canceled"));
   this->close();
   mainWin->resetFocus();
   m_console->beginNewCommand(m_conn);
   m_console->notify(m_conn, true);
}

/*
 * Called here when the jobname combo box is changed.
 *  We load the default values for the new job in the
 *  other combo boxes.
 */
void selectDialog::index_change(int index)
{
   m_index = index;
}

/*
 * Handle yesno PopUp when Bacula asks a yes/no question.
 */
/*
 * Read the items for the selection
 */
yesnoPopUp::yesnoPopUp(Console *console, int conn)  : QDialog()
{
   QMessageBox msgBox;

   setAttribute(Qt::WA_DeleteOnClose);
   console->read(conn);                 /* get yesno question */
   msgBox.setWindowTitle(tr("Bat Question"));
   msgBox.setText(console->msg(conn));
   msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
   console->displayToPrompt(conn);
   switch (msgBox.exec()) {
   case QMessageBox::Yes:
      console->write_dir(conn, "yes");
      break;
   case QMessageBox::No:
      console->write_dir(conn, "no");
      break;
   }
   console->displayToPrompt(conn);
   mainWin->resetFocus();
}
