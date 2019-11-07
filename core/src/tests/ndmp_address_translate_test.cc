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

#include "gtest/gtest.h"

#include "include/bareos.h"
#include "dird/dird.h"
#include "dird/storage.h"


void print_smc_aa(smc_element_address_assignment smc_elem_aa)
{
  printf("drive: %d, %d\n", smc_elem_aa.dte_addr, smc_elem_aa.dte_count);
  printf("robot: %d, %d\n", smc_elem_aa.mte_addr, smc_elem_aa.mte_count);
  printf("ie: %d, %d\n", smc_elem_aa.iee_addr, smc_elem_aa.iee_count);
  printf("slot: %d, %d\n\n", smc_elem_aa.se_addr, smc_elem_aa.se_count);
}

void init_smc_aa(smc_element_address_assignment& smc_elem_aa)
{
  // Tapes Drives
  smc_elem_aa.dte_addr = 1000;
  smc_elem_aa.dte_count = 10;

  // Robot Arms
  smc_elem_aa.mte_addr = 2000;
  smc_elem_aa.mte_count = 1;

  // I/E
  smc_elem_aa.iee_addr = 3000;
  smc_elem_aa.iee_count = 5;

  // storage=(normal) slots
  smc_elem_aa.se_addr = 4000;
  smc_elem_aa.se_count = 10;
}


/* SLOTS Address Mapping
SLOT   ADDR
1      4000
2      4001
3      4002
4      4003
5      4004
6      4005
7      4006
8      4007
9      4008
10     4009
11     3000
12     3001
13     3002
14     3003
15     3004
*/


//
// Tapes count from 0
// Slots from 1
// ie slots are enumerated after storage slots

TEST(ndmp, slot_to_element_addr)
{
  smc_element_address_assignment smc_elem_aa;

  init_smc_aa(smc_elem_aa);
  print_smc_aa(smc_elem_aa);

  EXPECT_EQ(1000,
            directordaemon::GetElementAddressByBareosSlotNumber(
                &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 0));
  EXPECT_EQ(1009,
            directordaemon::GetElementAddressByBareosSlotNumber(
                &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 9));
  EXPECT_EQ(kInvalidSlotNumber,
            directordaemon::GetElementAddressByBareosSlotNumber(
                &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 10));

  EXPECT_EQ(2000,
            directordaemon::GetElementAddressByBareosSlotNumber(
                &smc_elem_aa, directordaemon::slot_type_t::kSlotTypePicker, 0));
  EXPECT_EQ(kInvalidSlotNumber,
            directordaemon::GetElementAddressByBareosSlotNumber(
                &smc_elem_aa, directordaemon::slot_type_t::kSlotTypePicker, 1));


  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetElementAddressByBareosSlotNumber(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeImport, 10));
  EXPECT_EQ(3000, directordaemon::GetElementAddressByBareosSlotNumber(
                      &smc_elem_aa,
                      directordaemon::slot_type_t::kSlotTypeImport, 11));
  EXPECT_EQ(3005, directordaemon::GetElementAddressByBareosSlotNumber(
                      &smc_elem_aa,
                      directordaemon::slot_type_t::kSlotTypeImport, 16));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetElementAddressByBareosSlotNumber(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeImport, 17));


  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetElementAddressByBareosSlotNumber(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeStorage, 0));
  EXPECT_EQ(4000, directordaemon::GetElementAddressByBareosSlotNumber(
                      &smc_elem_aa,
                      directordaemon::slot_type_t::kSlotTypeStorage, 1));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetElementAddressByBareosSlotNumber(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeStorage, 11));
}


TEST(ndmp, element_addr_to_slot)
{
  smc_element_address_assignment smc_elem_aa;

  init_smc_aa(smc_elem_aa);
  print_smc_aa(smc_elem_aa);

  EXPECT_EQ(
      0, directordaemon::GetBareosSlotNumberByElementAddress(
             &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 1000));
  EXPECT_EQ(
      0, directordaemon::GetBareosSlotNumberByElementAddress(
             &smc_elem_aa, directordaemon::slot_type_t::kSlotTypePicker, 2000));
  EXPECT_EQ(11, directordaemon::GetBareosSlotNumberByElementAddress(
                    &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeImport,
                    3000));
  EXPECT_EQ(1, directordaemon::GetBareosSlotNumberByElementAddress(
                   &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeStorage,
                   4000));

  // one too low
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 999));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 1999));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeImport, 2999));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeStorage, 3999));

  // one too high
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 1010));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeDrive, 2001));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeImport, 3006));
  EXPECT_EQ(
      kInvalidSlotNumber,
      directordaemon::GetBareosSlotNumberByElementAddress(
          &smc_elem_aa, directordaemon::slot_type_t::kSlotTypeStorage, 4010));
}
