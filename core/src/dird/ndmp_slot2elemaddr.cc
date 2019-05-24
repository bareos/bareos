/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2019 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include "dird/dird.h"
#include "ndmp/smc.h"

namespace directordaemon {
/**
 * calculate the element address for given slotnumber and slot_type
 */
/* clang-format off */
slot_number_t GetElementAddressByBareosSlotNumber(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t slotnumber)
{
  slot_number_t calculated_slot;

  switch (slot_type) {
    case slot_type_t::kSlotTypeStorage:
      if ((slotnumber > smc_elem_aa->se_count) ||
          !IsSlotNumberValid(slotnumber)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = slotnumber
                        + smc_elem_aa->se_addr
                        - 1;  // normal slots count start from 1
      }
      break;
    case slot_type_t::kSlotTypeImport:
      if ((slotnumber < (smc_elem_aa->se_count + 1)) ||
          (slotnumber > (smc_elem_aa->se_count + smc_elem_aa->iee_count + 1))) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = slotnumber
                        - smc_elem_aa->se_count // i/e slots follow after normal slots
                        + smc_elem_aa->iee_addr
                        - 1;                    // normal slots count start from 1
      }
      break;
    case slot_type_t::kSlotTypePicker:
      if ((slotnumber == kInvalidSlotNumber) ||
          slotnumber > (smc_elem_aa->mte_count - 1)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = slotnumber
                        + smc_elem_aa->mte_addr;
      }
      break;
    case slot_type_t::kSlotTypeDrive:
      if ((slotnumber > (smc_elem_aa->dte_count) - 1) ||
          (slotnumber == kInvalidSlotNumber)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = slotnumber
                        + smc_elem_aa->dte_addr;
      }
      break;
    default:
    case slot_type_t::kSlotTypeUnknown:
      calculated_slot = kInvalidSlotNumber;
      break;
  }
  return calculated_slot;
}

/**
 * calculate the slotnumber for element address and slot_type
 */
slot_number_t GetBareosSlotNumberByElementAddress(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t element_addr)
{
  slot_number_t calculated_slot;

  switch (slot_type) {
    case slot_type_t::kSlotTypeStorage:
      if (element_addr < smc_elem_aa->se_addr ||
          (element_addr > smc_elem_aa->se_addr + smc_elem_aa->se_count - 1)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = element_addr
                        - smc_elem_aa->se_addr
                        + 1;  // slots count start from 1
      }
      break;
    case slot_type_t::kSlotTypeImport:
      if ((element_addr < smc_elem_aa->iee_addr) ||
          (element_addr > smc_elem_aa->iee_addr + smc_elem_aa->iee_count - 1)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = element_addr
                        + smc_elem_aa->se_count  // i/e slots follow after normal slots
                        - smc_elem_aa->iee_addr
                        + 1;                     // slots count start from 1
      }
      break;
    case slot_type_t::kSlotTypeDrive:
      if ((element_addr < smc_elem_aa->dte_addr) ||
          (element_addr > smc_elem_aa->dte_addr + smc_elem_aa->dte_count - 1)) {
        calculated_slot = kInvalidSlotNumber;
      } else {
        calculated_slot = element_addr
                        - smc_elem_aa->dte_addr;
      }
      break;
    case slot_type_t::kSlotTypePicker:
      calculated_slot = element_addr
                      - smc_elem_aa->mte_addr;
      break;
    default:
    case slot_type_t::kSlotTypeUnknown:
      calculated_slot = kInvalidSlotNumber;
      break;
  }
  return calculated_slot;
}
/* clang-format on */

} /* namespace directordaemon */
