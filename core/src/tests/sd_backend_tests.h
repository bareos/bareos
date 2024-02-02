/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_TESTS_SD_BACKEND_TESTS_H_
#define BAREOS_TESTS_SD_BACKEND_TESTS_H_
namespace storagedaemon {
/* import this to parse the config */
extern bool ParseSdConfig(const char* configfile, int exit_code);
}  // namespace storagedaemon

class sd : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
};

void sd::SetUp()
{
  using namespace storagedaemon;
  OSDependentInit();

  debug_level = 900;

  /* configfile is a global char* from stored_globals.h */
  configfile = strdup("configs/" CONFIG_SUBDIR "/");
  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);
  /* we do not run CheckResources() here, so take care the test configration
   * is not broken. Also autochangers will not work. */
}
void sd::TearDown()
{
  using namespace storagedaemon;
  Dmsg0(100, "TearDown start\n");

  {
    DeviceResource* d = nullptr;
    foreach_res (d, R_DEVICE) {
      Dmsg1(10, "Term device %s (%s)\n", d->resource_name_,
            d->archive_device_string);
      if (d->dev) {
        d->dev->ClearVolhdr();
        delete d->dev;
        d->dev = nullptr;
      }
    }
  }

  if (configfile) { free(configfile); }
  if (my_config) { delete my_config; }

  TermReservationsLock();
}
#endif  // BAREOS_TESTS_SD_BACKEND_TESTS_H_
