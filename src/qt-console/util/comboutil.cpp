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
 *   ComboBox helper functions
 *
 *   Riccardo Ghetta, May 2008
 *
 */

#include "bat.h"
#include <QComboBox>
#include <QString>
#include <QStringList>
#include "fmtwidgetitem.h"
#include "comboutil.h"

static const QString QS_ANY(QObject::tr("Any"));


/* selects value val on combo, if exists */
void comboSel(QComboBox *combo, const QString &val)
{
  int index = combo->findText(val, Qt::MatchExactly);
  if (index != -1) {
     combo->setCurrentIndex(index);
  }
}

/* if the combo has selected something different from "Any" uses the selection
 * to build a condition on field fldname and adds it to the condition list */
void comboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname)
{
   int index = combo->currentIndex();
   if (index != -1 && combo->itemText(index) != QS_ANY) {
      cndlist.append( QString("%1='%2'").arg(fldname).arg(combo->itemText(index)) );
   }
}


/* boolean combo (yes/no) */
void boolComboFill(QComboBox *combo)
{
   combo->addItem(QS_ANY, -1);
   combo->addItem(QObject::tr("No"), 0);
   combo->addItem(QObject::tr("Yes"), 1);
}

void boolComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname)
{
   int index = combo->currentIndex();
   if (index != -1 && combo->itemData(index).toInt() >= 0 ) {
      QString cnd = combo->itemData(index).toString();
      cndlist.append( QString("%1='%2'").arg(fldname).arg(cnd) );
   }
}

/* backup level combo */
void levelComboFill(QComboBox *combo)
{
   combo->addItem(QS_ANY);
   combo->addItem(job_level_to_str(L_FULL), L_FULL);
   combo->addItem(job_level_to_str(L_INCREMENTAL), L_INCREMENTAL);
   combo->addItem(job_level_to_str(L_DIFFERENTIAL), L_DIFFERENTIAL);
   combo->addItem(job_level_to_str(L_SINCE), L_SINCE);
   combo->addItem(job_level_to_str(L_VERIFY_CATALOG), L_VERIFY_CATALOG);
   combo->addItem(job_level_to_str(L_VERIFY_INIT), L_VERIFY_INIT);
   combo->addItem(job_level_to_str(L_VERIFY_VOLUME_TO_CATALOG), L_VERIFY_VOLUME_TO_CATALOG);
   combo->addItem(job_level_to_str(L_VERIFY_DISK_TO_CATALOG), L_VERIFY_DISK_TO_CATALOG);
   combo->addItem(job_level_to_str(L_VERIFY_DATA), L_VERIFY_DATA);
   /* combo->addItem(job_level_to_str(L_BASE), L_BASE);  base jobs ignored */
}

void levelComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname)
{
   int index = combo->currentIndex();
   if (index != -1 && combo->itemText(index) != QS_ANY ) {
      QString cnd = combo->itemData(index).toChar();
      cndlist.append( QString("%1='%2'").arg(fldname).arg(cnd) );
   }
}

/* job status combo */
void jobStatusComboFill(QComboBox *combo)
{
   static const char js[] = {
		      JS_Terminated,
                      JS_Created,
		      JS_Running,
		      JS_Blocked,
		      JS_ErrorTerminated,
		      JS_Error,
		      JS_FatalError,
		      JS_Differences,
		      JS_Canceled,
		      JS_WaitFD,
		      JS_WaitSD,
		      JS_WaitMedia,
		      JS_WaitMount,
		      JS_WaitStoreRes,
		      JS_WaitJobRes,
		      JS_WaitClientRes,
		      JS_WaitMaxJobs,
		      JS_WaitStartTime,
		      JS_WaitPriority,
		      JS_AttrDespooling,
		      JS_AttrInserting,
		      JS_DataDespooling,
		      JS_DataCommitting,
		      '\0'};

   int pos;

   combo->addItem(QS_ANY);
   for (pos = 0 ; js[pos] != '\0' ; ++pos) {
     combo->addItem(convertJobStatus( QString(js[pos]) ), js[pos]);
   }
}

void jobStatusComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname)
{
   int index = combo->currentIndex();
   if (index != -1 && combo->itemText(index) != QS_ANY ) {
      QString cnd = combo->itemData(index).toChar();
      cndlist.append( QString("%1='%2'").arg(fldname).arg(cnd) );
   }
}
