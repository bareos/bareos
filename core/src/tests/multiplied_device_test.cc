/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2025 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "lib/alist.h"
#include "lib/parse_conf.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"
#include <iostream>

using namespace storagedaemon;

typedef std::unique_ptr<ConfigurationParser> PConfigParser;

static void InitGlobals()
{
  my_config = nullptr;
  OSDependentInit();
}

static DeviceResource* GetDeviceResourceByName(ConfigurationParser& config,
                                               const char* name)
{
  BareosResource* p = config.GetResWithName(R_DEVICE, name, false);
  std::cout << name << p << std::endl;
  return dynamic_cast<DeviceResource*>(p);
}

static DeviceResource* GetMultipliedDeviceResource(ConfigurationParser& config)
{
  const char* name = "MultipliedDeviceResource";

  BareosResource* p = config.GetResWithName(R_DEVICE, name, false);
  DeviceResource* d = dynamic_cast<DeviceResource*>(p);
  if (d && d->count) { return d; }

  return nullptr;
}

TEST(sd, MultipliedDeviceTest_ConfigParameter)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());
  auto d = GetMultipliedDeviceResource(*test_config);
  ASSERT_TRUE(d);

  EXPECT_EQ(d->count, 3);
}

static uint32_t CountAllDeviceResources(ConfigurationParser& config)
{
  uint32_t count = 0;
  BareosResource* p = nullptr;
  while ((p = config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* d = dynamic_cast<DeviceResource*>(p);
    if (d && d->multiplied_device_resource) { count++; }
  }
  return count;
}

TEST(sd, MultipliedDeviceTest_ImplicitAutochangerCreation)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();
  ASSERT_TRUE(test_config->ParseConfig());

  DeviceResource* explicit_device
      = GetDeviceResourceByName(*test_config, "DeviceWithExplicitAutochanger");
  ASSERT_TRUE(explicit_device);
  ASSERT_TRUE(explicit_device->changer_res);
  DeviceResource* implicit_device
      = GetDeviceResourceByName(*test_config, "DeviceWithImplicitAutochanger");
  ASSERT_TRUE(implicit_device);
  ASSERT_TRUE(implicit_device->changer_res);

  EXPECT_EQ(std::string(explicit_device->changer_res->resource_name_),
            "ExplicitAutochanger");
  EXPECT_EQ(std::string(implicit_device->changer_res->resource_name_),
            "DeviceWithImplicitAutochanger");
}

TEST(sd, MultipliedDeviceTest_CountAllAutomaticallyCreatedResources)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());
  auto count = CountAllDeviceResources(*test_config);

  /* configurable using Device config for "AnotherMultipliedDeviceResource" */
  int amount_to_check = 117;
  EXPECT_EQ(count, amount_to_check);
}

static uint32_t CheckNamesOfConfiguredDeviceResources_1(
    ConfigurationParser& config)
{
  uint32_t count_str_ok = 0;
  uint32_t count_devices = 0;

  DeviceResource* source_device_resource
      = GetDeviceResourceByName(config, "MultipliedDeviceResource");
  if (!source_device_resource) { return 0; }

  /* find all matching multiplied-devices, this includes the source device */
  BareosResource* p = nullptr;
  while ((p = config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = dynamic_cast<DeviceResource*>(p);
    if (device->multiplied_device_resource == source_device_resource) {
      const char* name = nullptr;
      switch (count_devices++) {
        case 0:
          name = "MultipliedDeviceResource0000";
          break;
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
  } /* while GetNextRes */
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNames_1)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());

  auto count = CheckNamesOfConfiguredDeviceResources_1(*test_config);

  EXPECT_EQ(count, 4);
}

static uint32_t CheckNamesOfConfiguredDeviceResources_2(
    ConfigurationParser& config)
{
  uint32_t count_str_ok = 0;
  uint32_t count_devices = 0;

  DeviceResource* source_device_resource
      = GetDeviceResourceByName(config, "AnotherMultipliedDeviceResource");
  if (!source_device_resource) { return 0; }

  BareosResource* p = nullptr;
  while ((p = config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device_resource = dynamic_cast<DeviceResource*>(p);
    if (device_resource->multiplied_device_resource == source_device_resource) {
      const char* name = nullptr;
      switch (count_devices++) {
        case 0:
          name = "AnotherMultipliedDeviceResource0000";
          break;
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
      std::string name_of_device(device_resource->resource_name_);
      std::string name_to_compare(name ? name : "???");
      if (name_of_device == name_to_compare) { ++count_str_ok; }
    } /* if (device_resource->multiplied_device_resource) */
  } /* while GetNextRes */
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNames_2)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());

  auto count = CheckNamesOfConfiguredDeviceResources_2(*test_config);

  EXPECT_EQ(count, 101);
}

static uint32_t CheckAutochangerInAllDevices(ConfigurationParser& config)
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
  BareosResource* p = nullptr;

  while ((p = config.GetNextRes(R_DEVICE, p))) {
    DeviceResource* device = dynamic_cast<DeviceResource*>(p);
    if (device && device->multiplied_device_resource) {
      if (device->changer_res && device->changer_res->resource_name_) {
        std::string changer_name(device->changer_res->resource_name_);
        if (names.find(device->resource_name_) != names.end()) {
          if (names.at(device->resource_name_) == changer_name) {
            ++count_str_ok;
          }
        }
      }
    }
  }
  return count_str_ok;
}

TEST(sd, MultipliedDeviceTest_CheckNameOfAutomaticallyAttachedAutochanger)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());

  auto count = CheckAutochangerInAllDevices(*test_config);

  EXPECT_EQ(count, 6);
}

static uint32_t CheckSomeDevicesInAutochanger(ConfigurationParser& config)
{
  uint32_t count_str_ok = 0;
  BareosResource* p = nullptr;

  std::set<std::string> names = {{"MultipliedDeviceResource0001"},
                                 {"MultipliedDeviceResource0002"},
                                 {"MultipliedDeviceResource0003"}};

  while ((p = config.GetNextRes(R_AUTOCHANGER, p))) {
    AutochangerResource* autochanger = dynamic_cast<AutochangerResource*>(p);
    if (autochanger && autochanger->device_resources) {
      std::string autochanger_name(autochanger->resource_name_);
      std::string autochanger_name_test(
          "virtual-multiplied-device-autochanger");
      if (autochanger_name == autochanger_name_test) {
        for (auto* d : autochanger->device_resources) {
          std::string device_name(d->resource_name_);
          if (names.find(device_name) != names.end()) { ++count_str_ok; }
        }
      }
    }
  }
  return count_str_ok;
}

TEST(sd,
     MultipliedDeviceTest_CheckNameOfDevicesAutomaticallyAttachedToAutochanger)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());

  auto count = CheckSomeDevicesInAutochanger(*test_config);

  EXPECT_EQ(count, 3);
}

TEST(sd, MultipliedDeviceTest_CheckPointerReferenceOfCopiedDevice)
{
  InitGlobals();
  std::string path_to_config = "configs/stored_multiplied_device/";

  PConfigParser test_config(InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = test_config.get();

  ASSERT_TRUE(test_config->ParseConfig());

  BareosResource* p;
  p = test_config->GetResWithName(R_DEVICE, "MultipliedDeviceResource");
  ASSERT_TRUE(p);
  DeviceResource* original_device_resource = dynamic_cast<DeviceResource*>(p);
  p = test_config->GetResWithName(R_DEVICE, "MultipliedDeviceResource0001");
  ASSERT_TRUE(p);
  DeviceResource* multiplied_device_resource = dynamic_cast<DeviceResource*>(p);
  EXPECT_EQ(original_device_resource,
            multiplied_device_resource->multiplied_device_resource);
}
