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
 *  Run Dialog class
 *
 *   Kern Sibbald, February MMVII
 *
 *  $Id$
 */ 

#include "bat.h"
#include "run.h"

/*
 * Setup all the combo boxes and display the dialog
 */
prunePage::prunePage(const QString &volume, const QString &client) : Pages()
{
   QDateTime dt;

   m_name = tr("Prune");
   pgInitialize();
   setupUi(this);
   m_conn = m_console->notifyOff();

   QString query("SELECT VolumeName AS Media FROM Media ORDER BY Media");
   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "Query cmd : %s\n",query.toUtf8().data());
   }
   QStringList results, volumeList;
   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;
      /* Iterate through the lines of results. */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         volumeList.append(fieldlist[0]);
      } /* foreach resultline */
   } /* if results from query */

   volumeCombo->addItem(tr("Any"));
   volumeCombo->addItems(volumeList);
   clientCombo->addItem(tr("Any"));
   clientCombo->addItems(m_console->client_list);
   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   filesRadioButton->setChecked(true);
   if (clientCombo->findText(client, Qt::MatchExactly) != -1)
      clientCombo->setCurrentIndex(clientCombo->findText(client, Qt::MatchExactly));
   else
      clientCombo->setCurrentIndex(0);
   if (volumeCombo->findText(volume, Qt::MatchExactly) != -1)
      volumeCombo->setCurrentIndex(volumeCombo->findText(volume, Qt::MatchExactly));
   else
      volumeCombo->setCurrentIndex(0);
   connect(volumeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(volumeChanged()));
   connect(clientCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(clientChanged()));

   dockPage();
   setCurrent();
   this->show();
}

void prunePage::okButtonPushed()
{
   this->hide();
   QString cmd("prune");
   if (filesRadioButton->isChecked()) {
      cmd += " files";
   }
   if (jobsRadioButton->isChecked()) {
      cmd += " jobs";
   }
   if (filesRadioButton->isChecked()) {
      cmd += " volume";
   }
   if (volumeCombo->currentText() != tr("Any")) {
      cmd += " volume=\"" + volumeCombo->currentText() + "\"";
   }
   if (clientCombo->currentText() != tr("Any")) {
      cmd += " client=\"" + clientCombo->currentText() + "\"";
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


void prunePage::cancelButtonPushed()
{
   mainWin->set_status(tr(" Canceled"));
   this->hide();
   m_console->notify(m_conn, true);
   closeStackPage();
   mainWin->resetFocus();
}

void prunePage::volumeChanged()
{
   if ((volumeCombo->currentText() == tr("Any")) && (clientCombo->currentText() == tr("Any"))) {
      clientCombo->setCurrentIndex(1);
   }
}

void prunePage::clientChanged()
{
   if ((volumeCombo->currentText() == tr("Any")) && (clientCombo->currentText() == tr("Any"))) {
      volumeCombo->setCurrentIndex(1);
   }
}
