/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_DIRD_UA_TREE_H_
#define BAREOS_DIRD_UA_TREE_H_

#include "dird/ua.h"

namespace directordaemon {

int markcmd(UaContext* ua, TreeContext* tree);
int Markdircmd(UaContext* ua, TreeContext* tree);
int countcmd(UaContext* ua, TreeContext* tree);
int findcmd(UaContext* ua, TreeContext* tree);
int lscmd(UaContext* ua, TreeContext* tree);
int Lsmarkcmd(UaContext* ua, TreeContext* tree);
int dircmd(UaContext* ua, TreeContext* tree);
int DotDircmd(UaContext* ua, TreeContext* tree);
int Estimatecmd(UaContext* ua, TreeContext* tree);
int HelpCmd(UaContext* ua, TreeContext* tree);
int cdcmd(UaContext* ua, TreeContext* tree);
int pwdcmd(UaContext* ua, TreeContext* tree);
int DotPwdcmd(UaContext* ua, TreeContext* tree);
int Unmarkcmd(UaContext* ua, TreeContext* tree);
int UnMarkdircmd(UaContext* ua, TreeContext* tree);
int QuitCmd(UaContext* ua, TreeContext* tree);
int donecmd(UaContext* ua, TreeContext* tree);
int DotLsdircmd(UaContext* ua, TreeContext* tree);
int DotLscmd(UaContext* ua, TreeContext* tree);
int DotHelpcmd(UaContext* ua, TreeContext* tree);
int DotLsmarkcmd(UaContext* ua, TreeContext* tree);

int MarkElement(char* element, UaContext* ua, TreeContext* tree);
int UnmarkElement(char* element, UaContext* ua, TreeContext* tree);

bool UserSelectFilesFromTree(TreeContext* tree);
int InsertTreeHandler(void* ctx, int num_fields, char** row);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_TREE_H_
