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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <io.h>

int main(int argc, char* argv[])
{
  if (argc != 1) { return -1; }

  _setmode(_fileno(stdin), _O_BINARY);

  size_t buffer_size = 1024 * 1024;
  char* buffer = (char*)malloc(buffer_size);

  size_t total_bytes = 0;
  while (!feof(stdin) && !ferror(stdin)) {
    size_t bytes_read = fread(buffer, 1, buffer_size, stdin);

    if (bytes_read == 0) { break; }

    total_bytes += bytes_read;
  }

  printf("total bytes: %zu, stdin eof: %s, stdin error: %s\n", total_bytes,
         feof(stdin) ? "yes" : "no", ferror(stdin) ? "yes" : "no");
  return 0;
}
