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

FILE* log_file;

bool write_full(FILE* f, char* buffer, size_t size)
{
  size_t total_bytes = 0;
  while (total_bytes < size) {
    auto bytes_written = fwrite(buffer + total_bytes, 1, size - total_bytes, f);

    fprintf(log_file, "[write] bytes written %llu\n", bytes_written);

    if (bytes_written == 0) { return false; }

    total_bytes += bytes_written;
  }

  return true;
}

int main(int argc, char* argv[])
{
  if (argc != 2) { return -1; }

  char* filename = argv[1];

  log_file = fopen("logfile.txt", "w");

  FILE* f = fopen(filename, "wb");
  if (f == NULL) { return -2; }

  _setmode(_fileno(stdin), _O_BINARY);

  size_t buffer_size = 1024 * 1024;
  char* buffer = (char*)malloc(buffer_size);
  while (!feof(stdin) && !ferror(stdin)) {
    size_t bytes_read = fread(buffer, 1, buffer_size, stdin);

    fprintf(log_file, "[read] bytes_read: %llu\n", bytes_read);

    if (bytes_read == 0) { break; }

    if (!write_full(f, buffer, bytes_read)) {
      fprintf(log_file, "[write] bad write\n");
      break;
    }
  }

  fprintf(log_file, "[end] eof: %s error: %s\n", feof(stdin) ? "yes" : "no",
          ferror(stdin) ? "yes" : "no");

  fclose(f);
  fclose(log_file);
}
