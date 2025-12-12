/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2026 Bareos GmbH & Co. KG

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

#include "logger.h"
#include <chrono>
#include <system_error>
#include <thread>

void wait() { (void)fgetc(stdin); }

int main()
{
  auto* logger = progressbar::get();

  size_t max = 100;
  size_t current = 0;

  logger->Begin(max);

  logger->Progressed(0);
  while (current < max) {
    size_t progress = 10;
    logger->Progressed(progress);
    current += progress;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  logger->End();
}
