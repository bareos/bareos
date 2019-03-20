/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/alist.h"
#include "lib/parse_conf.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"

using namespace storagedaemon;

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

static void InitGlobals() { my_config = nullptr; }

static DeviceResource* GetMultipliedDeviceResource(
    ConfigurationParser& my_config)
{
  const char* name = "MultipliedDeviceResource0001";

  CommonResourceHeader* p = my_config.GetResWithName(R_DEVICE, name, false);
  DeviceResource* device = reinterpret_cast<DeviceResource*>(p);
  if (device->count) { return device; }

  return nullptr;
}

TEST(sd, MultipliedDeviceTest_ConfigParameter)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());
  auto device = GetMultipliedDeviceResource(*my_config);
  ASSERT_TRUE(device);

  EXPECT_EQ(device->count, 3);
}

static uint32_t CountAllDeviceResources(ConfigurationParser& my_config)
{
  uint32_t count = 0;
  CommonResourceHeader* p = nullptr;
  while ((p = my_config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = reinterpret_cast<DeviceResource*>(p);
    if (device->multiplied_device_resource) { count++; }
  }
  return count;
}

TEST(sd, MultipliedDeviceTest_CountAllAutomaticallyCreatedResources)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());
  auto count = CountAllDeviceResources(*my_config);

  /* configurable using Device config for "AnotherMultipliedDeviceResource" */
  int amount_to_check = 103;
  EXPECT_EQ(count, amount_to_check);
}

DeviceResource* GetDeviceResourceByName(ConfigurationParser& my_config,
                                        const char* name)
{
  CommonResourceHeader* p = my_config.GetResWithName(R_DEVICE, name, false);
  return reinterpret_cast<DeviceResource*>(p);
}

static uint32_t CheckNamesOfConfiguredDeviceResources_1(
    ConfigurationParser& my_config)
{
  uint32_t count_str_ok = 0;
  uint32_t count_devices = 0;

  DeviceResource* source_device =
      GetDeviceResourceByName(my_config, "MultipliedDeviceResource0001");
  if (!source_device) { return 0; }

  /* find all matching multiplied-devices, this includes the source device */
  CommonResourceHeader* p = nullptr;
  while ((p = my_config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = reinterpret_cast<DeviceResource*>(p);
    if (device->multiplied_device_resource == source_device) {
      const char* name = nullptr;
      ++count_devices;
      switch (count_devices) {
        case 1:
          name = "MultipliedDeviceResource0001";
          break;
        case 2:
          name = "MultipliedDeviceResource0002";
          break;
        case 3:
          name = "MultipliedDeviceResource0003";
          break;
        default:
          return 0;
      } /* switch (count_devices) */
      std::string name_of_device(device->resource_name_);
      std::string name_to_compare(name ? name : "???");
      if (name_of_device == name_to_compare) { ++count_str_ok; }
    } /* if (device->multiplied_device_resource) */
  }   /* while GetNextRes */
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNames_1)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  auto count = CheckNamesOfConfiguredDeviceResources_1(*my_config);

  EXPECT_EQ(count, 3);
}

static uint32_t CheckNamesOfConfiguredDeviceResources_2(
    ConfigurationParser& my_config)
{
  uint32_t count_str_ok = 0;
  uint32_t count_devices = 0;

  DeviceResource* source_device =
      GetDeviceResourceByName(my_config, "AnotherMultipliedDeviceResource0001");
  if (!source_device) { return 0; }

  CommonResourceHeader* p = nullptr;
  while ((p = my_config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = reinterpret_cast<DeviceResource*>(p);
    if (device->multiplied_device_resource == source_device) {
      const char* name = nullptr;
      ++count_devices;
      switch (count_devices) {
        case 1:
          name = "AnotherMultipliedDeviceResource0001";
          break;
        case 2:
          name = "AnotherMultipliedDeviceResource0002";
          break;
        case 3:
          name = "AnotherMultipliedDeviceResource0003";
          break;
        /* configurable using Device config for
         * "AnotherMultipliedDeviceResource" */
        case 9999:
          name = "AnotherMultipliedDeviceResource9999";
          break;
        default:
          if (count_devices < 10000) {
            ++count_str_ok;
            continue;
          } else {
            return 0;
          }
      } /* switch (count_devices) */
      std::string name_of_device(device->resource_name_);
      std::string name_to_compare(name ? name : "???");
      if (name_of_device == name_to_compare) { ++count_str_ok; }
    } /* if (device->multiplied_device_resource) */
  }   /* while GetNextRes */
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNames_2)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  auto count = CheckNamesOfConfiguredDeviceResources_2(*my_config);

  EXPECT_EQ(count, 100);
}

static uint32_t CheckAutochangerInAllDevices(ConfigurationParser& my_config)
{
  std::map<std::string, std::string> names = {
      {"MultipliedDeviceResource0001", "virtual-multiplied-device-autochanger"},
      {"MultipliedDeviceResource0002", "virtual-multiplied-device-autochanger"},
      {"MultipliedDeviceResource0003", "virtual-multiplied-device-autochanger"},
      {"AnotherMultipliedDeviceResource0001",
       "another-virtual-multiplied-device-autochanger"},
      {"AnotherMultipliedDeviceResource0002",
       "another-virtual-multiplied-device-autochanger"},
      {"AnotherMultipliedDeviceResource0100",
       "another-virtual-multiplied-device-autochanger"}};

  uint32_t count_str_ok = 0;
  CommonResourceHeader* p = nullptr;

  while ((p = my_config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = reinterpret_cast<DeviceResource*>(p);
    if (device->multiplied_device_resource) {
      if (device->changer_res && device->changer_res->resource_name_) {
        std::string changer_name(device->changer_res->resource_name_);
        if (names.find(device->resource_name_) != names.end()) {
          if (names.at(device->resource_name_) == changer_name) { ++count_str_ok; }
        }
      }
    }
  }
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNameOfAutomaticallyAttachedAutochanger)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  auto count = CheckAutochangerInAllDevices(*my_config);

  EXPECT_EQ(count, 6);
}

static uint32_t CheckSomeDevicesInAutochanger(ConfigurationParser& my_config)
{
  uint32_t count_str_ok = 0;
  CommonResourceHeader* p = nullptr;

  std::set<std::string> names = {{"MultipliedDeviceResource0001"},
                                 {"MultipliedDeviceResource0002"},
                                 {"MultipliedDeviceResource0003"}};

  while ((p = my_config.GetNextRes(R_AUTOCHANGER, p))) {
    AutochangerResource* autochanger =
        reinterpret_cast<AutochangerResource*>(p);
    std::string autochanger_name(autochanger->resource_name_);
    std::string autochanger_name_test("virtual-multiplied-device-autochanger");
    if (autochanger_name == autochanger_name_test) {
      DeviceResource* device = nullptr;
      foreach_alist (device, autochanger->device) {
        std::string device_name(device->resource_name_);
        if (names.find(device_name) != names.end()) { ++count_str_ok; }
      }
    }
  }
  return count_str_ok;
}

TEST(sd,
     MultipliedDeviceTest_CheckNameOfDevicesAutomaticallyAttachedToAutochanger)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  auto count = CheckSomeDevicesInAutochanger(*my_config);

  EXPECT_EQ(count, 3);
}

TEST(sd, MultipliedDeviceTest_CheckPointerReferenceOfOriginalDevice)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  CommonResourceHeader* p;
  p = my_config->GetResWithName(R_DEVICE, "MultipliedDeviceResource0001");
  ASSERT_TRUE(p);
  DeviceResource* original_device = reinterpret_cast<DeviceResource*>(p);
  EXPECT_EQ(original_device, original_device->multiplied_device_resource);
}

TEST(sd, MultipliedDeviceTest_CheckPointerReferenceOfCopiedDevice)
{
  InitGlobals();
  std::string path_to_config =
      PROJECT_SOURCE_DIR "/src/tests/configs/stored_multiplied_device/";

  PConfigParser my_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = my_config.get();

  ASSERT_TRUE(my_config->ParseConfig());

  CommonResourceHeader* p;
  p = my_config->GetResWithName(R_DEVICE, "MultipliedDeviceResource0001");
  ASSERT_TRUE(p);
  DeviceResource* original_device = reinterpret_cast<DeviceResource*>(p);
  p = my_config->GetResWithName(R_DEVICE, "MultipliedDeviceResource0002");
  ASSERT_TRUE(p);
  DeviceResource* multiplied_device = reinterpret_cast<DeviceResource*>(p);
  EXPECT_EQ(original_device, multiplied_device->multiplied_device_resource);
}
