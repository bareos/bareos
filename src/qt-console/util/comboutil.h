#ifndef _COMBOUTIL_H_
#define _COMBOUTIL_H_
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
 *   Combobox helpers - Riccardo Ghetta, May 2008 
 */

class QComboBox;
class QString;
class QStringList;

/* selects value val on combo, if exists */
void comboSel(QComboBox *combo, const QString &val);

/* if the combo has selected something different from "Any" uses the selection
 * to build a condition on field fldname and adds it to the condition list */
void comboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname);

/* these helpers are used to give an uniform content to common combos.
 * There are two routines per combo type:
 * - XXXXComboFill fills the combo with values.
 * - XXXXComboCond checks the combo and, if selected adds a condition
 *   on the field fldName to the list of conditions cndList
 */

/* boolean combo (yes/no) */
void boolComboFill(QComboBox *combo);
void boolComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname);

/* backup level combo */
void levelComboFill(QComboBox *combo);
void levelComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname);

/* job status combo */
void jobStatusComboFill(QComboBox *combo);
void jobStatusComboCond(QStringList &cndlist, const QComboBox *combo, const char *fldname);

#endif /* _COMBOUTIL_H_ */
