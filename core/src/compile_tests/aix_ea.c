/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include <sys/ea.h>

void* ptr;
void* ptr2;
void* ptr3;

int main(int argc, char** argv)
{
  (void)argc;
  (void)argv;

  // AIX extended attribute functions
  getea(ptr, ptr2, ptr3, 0);
  lgetea(ptr, ptr2, ptr3, 0);
  setea(ptr, ptr2, ptr3, 0, 0);
  lsetea(ptr, ptr2, ptr3, 0, 0);
  listea(ptr, ptr2, 0);
  llistea(ptr, ptr2, 0);
}
