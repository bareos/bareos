/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

// issue job warning for python 2 deprecation
#if PY_VERSION_HEX < VERSION_HEX(3, 0, 0)
  Jmsg(plugin_ctx, M_WARNING,
       LOGPREFIX
       "Python 2 plugins are deprecated in Bareos 22 and will be removed in "
       "Bareos 23. Please switch to python 3 plugins. \n");
#endif
