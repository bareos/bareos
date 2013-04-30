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
 *   Dirk Bartley, March 2007
 */
 
#include "bat.h"
#include <QAbstractEventDispatcher>
#include <QTableWidgetItem>
#include <QMessageBox>
#include "mediaedit.h"

/*
 * A constructor 
 */
MediaEdit::MediaEdit(QTreeWidgetItem *parentWidget, QString &mediaId) 
  : Pages()
{
   setupUi(this);
   pgInitialize(tr("Media Edit"), parentWidget);
   QTreeWidgetItem* thisitem = mainWin->getFromHash(this);
   thisitem->setIcon(0,QIcon(QString::fromUtf8(":images/cartridge-edit.png")));
   dockPage();
   setCurrent();

   connect(okButton, SIGNAL(pressed()), this, SLOT(okButtonPushed()));
   connect(cancelButton, SIGNAL(pressed()), this, SLOT(cancelButtonPushed()));
   connectSpins();
   connect(retentionSpin, SIGNAL(valueChanged(int)), this, SLOT(retentionChanged()));
   connect(useDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(useDurationChanged()));
   connect(retentionRadio, SIGNAL(pressed()), this, SLOT(retentionRadioPressed()));
   connect(useDurationRadio, SIGNAL(pressed()), this, SLOT(useDurationRadioPressed()));

   m_pool = "";
   m_recyclePool = "";
   m_status = "";
   m_slot = 0;

   /* The media's pool */
   poolCombo->addItems(m_console->pool_list);

   /* The media's Status */
   QStringList statusList = (QStringList() << "Full" << "Used" << "Append" 
       << "Error" << "Purged" << "Recycle" << "Read-Only" << "Cleaning");
   statusCombo->addItems(statusList);

   /* Set up the query for the default values */
   QStringList FieldList = (QStringList()
      << "Media.VolumeName" << "Pool.Name" << "Media.VolStatus" << "Media.Slot"
      << "Media.VolRetention" << "Media.VolUseDuration" << "Media.MaxVolJobs"
      << "Media.MaxVolFiles" << "Media.MaxVolBytes" << "Media.Recycle" << "Media.Enabled"
      << "Pol.Name");
   QStringList AsList = (QStringList()
      << "VolumeName" << "PoolName" << "Status" << "Slot"
      << "Retention" << "UseDuration" << "MaxJobs"
      << "MaxFiles" << "MaxBytes" << "Recycle" << "Enabled"
      << "RecyclePool");
   int i = 0;
   QString query("SELECT ");
   foreach (QString field, FieldList) {
      if (i != 0) {
         query += ", ";
      }
      query += field + " AS " + AsList[i];
      i += 1;
   }

   QString where = " WHERE Media.VolumeName = '" + mediaId + "' ";
   if (mediaId.contains(QRegExp("^[0-9]+$"))) {
      where = " WHERE Media.MediaId=" + mediaId;
   }
   query += " FROM Media"
            " JOIN Pool ON (Media.PoolId=Pool.PoolId)"
            " LEFT OUTER JOIN Pool AS Pol ON (Media.RecyclePoolId=Pol.PoolId)"
            + where;

   if (mainWin->m_sqlDebug) {
      Pmsg1(000, "MediaList query cmd : %s\n",query.toUtf8().data());
   }
   QStringList results;
   if (m_console->sql_cmd(query, results)) {
      QString field;
      QStringList fieldlist;

      /* Iterate through the lines of results, there should only be one. */
      foreach (QString resultline, results) {
         fieldlist = resultline.split("\t");
         i = 0;

         /* Iterate through fields in the record */
         foreach (field, fieldlist) {
            field = field.trimmed();  /* strip leading & trailing spaces */
            bool ok;
            if (i == 0) {
               m_mediaName = field;
               volumeLabel->setText(QString("Volume : %1").arg(m_mediaName));
            } else if (i == 1) {
               m_pool = field;
            } else if (i == 2) {
               m_status = field;
            } else if (i == 3) {
               m_slot = field.toInt(&ok, 10);
               if (!ok){ m_slot = 0; }
            } else if (i == 4) {
               m_retention = field.toInt(&ok, 10);
               if (!ok){ m_retention = 0; }
            } else if (i == 5) {
               m_useDuration = field.toInt(&ok, 10);
               if (!ok){ m_useDuration = 0; }
            } else if (i == 6) {
               m_maxVolJobs = field.toInt(&ok, 10);
               if (!ok){ m_maxVolJobs = 0; }
            } else if (i == 7) {
               m_maxVolFiles = field.toInt(&ok, 10);
               if (!ok){ m_maxVolFiles = 0; }
            } else if (i == 8) {
               m_maxVolBytes = field.toInt(&ok, 10);
               if (!ok){ m_maxVolBytes = 0; }
            } else if (i == 9) {
               if (field == "1") m_recycle = true;
               else m_recycle = false;
            } else if (i == 10) {
               if (field == "1") m_enabled = true;
               else m_enabled = false;
            } else if (i == 11) {
               m_recyclePool = field;
            }
            i++;
         } /* foreach field */
      } /* foreach resultline */
   } /* if results from query */

   if (m_mediaName != "") {
      int index;
      /* default value for pool */
      index = poolCombo->findText(m_pool, Qt::MatchExactly);
      if (index != -1) {
         poolCombo->setCurrentIndex(index);
      }

      /* default value for status */
      index = statusCombo->findText(m_status, Qt::MatchExactly);
      if (index != -1) {
         statusCombo->setCurrentIndex(index);
      }
      slotSpin->setValue(m_slot);
      retentionSpin->setValue(m_retention);
      useDurationSpin->setValue(m_useDuration);
      setSpins(retentionSpin->value());
      retentionRadio->setChecked(true);
      maxJobsSpin->setValue(m_maxVolJobs);
      maxFilesSpin->setValue(m_maxVolFiles);
      maxBytesSpin->setValue(m_maxVolBytes);
      if (m_recycle) recycleCheck->setCheckState(Qt::Checked);
      else recycleCheck->setCheckState(Qt::Unchecked);
      if (m_enabled) enabledCheck->setCheckState(Qt::Checked);
      else enabledCheck->setCheckState(Qt::Unchecked);
      /* default for recycle pool */
      recyclePoolCombo->addItems(m_console->pool_list);
      recyclePoolCombo->insertItem(0, "*None*");
      index = recyclePoolCombo->findText(m_recyclePool, Qt::MatchExactly);
      if (index == -1) {
         index = 0;
      }
      recyclePoolCombo->setCurrentIndex(index);
   } else {
      QMessageBox::warning(this, tr("No Volume name"), tr("No Volume name given"),
                           QMessageBox::Ok, QMessageBox::Ok);
      return;
   }
}

/*
 * Function to handle updating the record then closing the page
 */
void MediaEdit::okButtonPushed()
{
   QString scmd;
   this->hide();
   bool docmd = false;
   scmd = QString("update volume=\"%1\"")
                  .arg(m_mediaName);
   if (m_pool != poolCombo->currentText()) {
      scmd += " pool=\"" + poolCombo->currentText() + "\"";
      docmd = true;
   }
   if (m_status != statusCombo->currentText()) {
      scmd += " volstatus=\"" + statusCombo->currentText() + "\"";
      docmd = true;
   }
   if (m_slot != slotSpin->value()) {
      scmd += " slot=" + QString().setNum(slotSpin->value());
      docmd = true;
   }
   if (m_retention != retentionSpin->value()) {
      scmd += " VolRetention=" + QString().setNum(retentionSpin->value());
      docmd = true;
   }
   if (m_useDuration != useDurationSpin->value()) {
      scmd += " VolUse=" + QString().setNum(useDurationSpin->value());
      docmd = true;
   }
   if (m_maxVolJobs != maxJobsSpin->value()) {
      scmd += " MaxVolJobs=" + QString().setNum(maxJobsSpin->value());
      docmd = true;
   }
   if (m_maxVolFiles != maxFilesSpin->value()) {
      scmd += " MaxVolFiles=" + QString().setNum(maxFilesSpin->value());
      docmd = true;
   }
   if (m_maxVolBytes != maxBytesSpin->value()) {
      scmd += " MaxVolBytes=" + QString().setNum(maxBytesSpin->value());
      docmd = true;
   }
   if ((m_recycle) && (recycleCheck->checkState() == Qt::Unchecked)) {
      scmd += " Recycle=no";
      docmd = true;
   }
   if ((!m_recycle) && (recycleCheck->checkState() == Qt::Checked)) {
      scmd += " Recycle=yes";
      docmd = true;
   }
   if ((m_enabled) && (enabledCheck->checkState() == Qt::Unchecked)) {
      scmd += " enabled=no";
      docmd = true;
   }
   if ((!m_enabled) && (enabledCheck->checkState() == Qt::Checked)) {
      scmd += " enabled=yes";
      docmd = true;
   }
   if (m_recyclePool != recyclePoolCombo->currentText()) {
      scmd += " recyclepool=\"";
      if (recyclePoolCombo->currentText() != "*None*") {
         scmd += recyclePoolCombo->currentText();
      }
      scmd += "\"";
      docmd = true;
   }
   if (docmd) {
      if (mainWin->m_commandDebug) {
         Pmsg1(000, "sending command : %s\n",scmd.toUtf8().data());
      }
      consoleCommand(scmd);
   }
   closeStackPage();
}

/* close if cancel */
void MediaEdit::cancelButtonPushed()
{
   closeStackPage();
}

/*
 * Slot for user changed retention
 */
void MediaEdit::retentionChanged()
{
   retentionRadio->setChecked(true);
   setSpins(retentionSpin->value());
}

/*
 * Slot for user changed the use duration
 */
void MediaEdit::useDurationChanged()
{
   useDurationRadio->setChecked(true);
   setSpins(useDurationSpin->value());
}

/*
 * Set the 5 duration spins from a known duration value
 */
void MediaEdit::setSpins(int value)
{
   int years, months, days, hours, minutes, seconds, left;
        
   years = abs(value / 31536000);
   left = value - years * 31536000;
   months = abs(left / 2592000);
   left = left - months * 2592000;
   days = abs(left / 86400);
   left = left - days * 86400;
   hours = abs(left / 3600);
   left = left - hours * 3600;
   minutes = abs(left / 60);
   seconds = left - minutes * 60;
   disconnectSpins();
   yearsSpin->setValue(years);
   monthsSpin->setValue(months);
   daysSpin->setValue(days);
   hoursSpin->setValue(hours);
   minutesSpin->setValue(minutes);
   secondsSpin->setValue(seconds);
   connectSpins();
}

/*
 * This slot is called any time any one of the 5 duration spins a changed.
 */
void MediaEdit::durationChanged()
{
   disconnectSpins();
   if (secondsSpin->value() == -1) {
      secondsSpin->setValue(59);
      minutesSpin->setValue(minutesSpin->value()-1);
   }
   if (minutesSpin->value() == -1) {
      minutesSpin->setValue(59);
      hoursSpin->setValue(hoursSpin->value()-1);
   }
   if (hoursSpin->value() == -1) {
      hoursSpin->setValue(23);
      daysSpin->setValue(daysSpin->value()-1);
   }
   if (daysSpin->value() == -1) {
      daysSpin->setValue(29);
      monthsSpin->setValue(monthsSpin->value()-1);
   }
   if (monthsSpin->value() == -1) {
      monthsSpin->setValue(11);
      yearsSpin->setValue(yearsSpin->value()-1);
   }
   if (yearsSpin->value() == -1) {
      yearsSpin->setValue(0);
   }

   if (secondsSpin->value() == 60) {
      secondsSpin->setValue(0);
      minutesSpin->setValue(minutesSpin->value()+1);
   }
   if (minutesSpin->value() == 60) {
      minutesSpin->setValue(0);
      hoursSpin->setValue(hoursSpin->value()+1);
   }
   if (hoursSpin->value() == 24) {
      hoursSpin->setValue(0);
      daysSpin->setValue(daysSpin->value()+1);
   }
   if (daysSpin->value() == 30) {
      daysSpin->setValue(0);
      monthsSpin->setValue(monthsSpin->value()+1);
   }
   if (monthsSpin->value() == 12) {
      monthsSpin->setValue(0);
      yearsSpin->setValue(yearsSpin->value()+1);
   }
   connectSpins();
   if (retentionRadio->isChecked()) {
      int retention;
      retention = secondsSpin->value() + minutesSpin->value() * 60 + 
         hoursSpin->value() * 3600 + daysSpin->value() * 86400 +
         monthsSpin->value() * 2592000 +
         yearsSpin->value() * 31536000;
      disconnect(retentionSpin, SIGNAL(valueChanged(int)), this, SLOT(retentionChanged()));
      retentionSpin->setValue(retention);
      connect(retentionSpin, SIGNAL(valueChanged(int)), this, SLOT(retentionChanged()));
   }
   if (useDurationRadio->isChecked()) {
      int useDuration;
      useDuration = secondsSpin->value() + minutesSpin->value() * 60 + 
         hoursSpin->value() * 3600 + daysSpin->value() * 86400 +
         monthsSpin->value() * 2592000 +
         yearsSpin->value() * 31536000;
      disconnect(useDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(useDurationChanged()));
      useDurationSpin->setValue(useDuration);
      connect(useDurationSpin, SIGNAL(valueChanged(int)), this, SLOT(useDurationChanged()));
   }
}

/* Connect the spins */
void MediaEdit::connectSpins()
{
   connect(secondsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   connect(minutesSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   connect(hoursSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   connect(daysSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   connect(monthsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   connect(yearsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
}

/* disconnect spins so that we can set the value of other spin from changed duration spin */
void MediaEdit::disconnectSpins()
{
   disconnect(secondsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   disconnect(minutesSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   disconnect(hoursSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   disconnect(daysSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   disconnect(monthsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
   disconnect(yearsSpin, SIGNAL(valueChanged(int)), this, SLOT(durationChanged()));
}

/* slot for setting spins when retention radio checked */
void MediaEdit::retentionRadioPressed()
{
   setSpins(retentionSpin->value());
}

/* slot for setting spins when duration radio checked */
void MediaEdit::useDurationRadioPressed()
{
   setSpins(useDurationSpin->value());
}
