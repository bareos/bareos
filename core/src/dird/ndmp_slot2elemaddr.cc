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
slot_number_t GetElementAddressByBareosSlotNumber(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t slotnumber)
{
  if (slot_type == slot_type_storage) {
    if ((slotnumber > static_cast<slot_number_t>(smc_elem_aa->se_count)) ||
        (slotnumber < 1)) {
      return -1;
    } else {
      return (slotnumber + smc_elem_aa->se_addr -
              1);  // normal slots count start from 1
    }
  } else if (slot_type == slot_type_import) {
    if ((slotnumber <
         (static_cast<slot_number_t>(smc_elem_aa->se_count) + 1)) ||
        (slotnumber >
         (static_cast<slot_number_t>(smc_elem_aa->se_count) +
          static_cast<slot_number_t>(smc_elem_aa->iee_count) + 1))) {
      return -1;
    } else {
      return (slotnumber -
              static_cast<slot_number_t>(
                  smc_elem_aa->se_count)  // i/e slots follow after normal slots
              - 1                         // normal slots count start from 1
              + static_cast<slot_number_t>(smc_elem_aa->iee_addr));
    }
  } else if (slot_type == slot_type_picker) {
    if ((slotnumber < 0) ||
        slotnumber > (static_cast<slot_number_t>(smc_elem_aa->mte_count) - 1)) {
      return -1;
    } else {
      return (slotnumber + static_cast<slot_number_t>(smc_elem_aa->mte_addr));
    }
  } else if (slot_type == slot_type_drive) {
    if ((slotnumber >
         (static_cast<slot_number_t>(smc_elem_aa->dte_count)) - 1) ||
        (slotnumber < 0)) {
      return -1;
    } else {
      return (slotnumber + static_cast<slot_number_t>(smc_elem_aa->dte_addr));
    }
  } else if (slot_type == slot_type_unknown) {
    return -1;
  } else {
    return -1;
  }
}

/**
 * calculate the slotnumber for element address and slot_type
 */
slot_number_t GetBareosSlotNumberByElementAddress(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t element_addr)
{
  if (slot_type == slot_type_storage) {
    if (element_addr < static_cast<slot_number_t>(smc_elem_aa->se_addr) ||
        (element_addr > static_cast<slot_number_t>(smc_elem_aa->se_addr) +
                            static_cast<slot_number_t>(smc_elem_aa->se_count) -
                            1)) {
      return -1;
    } else {
      return (element_addr - static_cast<slot_number_t>(smc_elem_aa->se_addr) +
              1);  // slots count start from 1
    }
  } else if (slot_type == slot_type_import) {
    if ((element_addr < static_cast<slot_number_t>(smc_elem_aa->iee_addr)) ||
        (element_addr >
         static_cast<slot_number_t>(smc_elem_aa->iee_addr) +
             static_cast<slot_number_t>(smc_elem_aa->iee_count - 1))) {
      return -1;
    } else {
      return (element_addr +
              static_cast<slot_number_t>(
                  smc_elem_aa->se_count)  // i/e slots follow after normal slots
              - static_cast<slot_number_t>(smc_elem_aa->iee_addr) +
              1);  // slots count start from 1
    }
  } else if (slot_type == slot_type_drive) {
    if ((element_addr < static_cast<slot_number_t>(smc_elem_aa->dte_addr)) ||
        (element_addr > static_cast<slot_number_t>(smc_elem_aa->dte_addr) +
                            static_cast<slot_number_t>(smc_elem_aa->dte_count) -
                            1)) {
      return -1;
    } else {
      return (element_addr - static_cast<slot_number_t>(smc_elem_aa->dte_addr));
    }
  } else if (slot_type == slot_type_picker) {
    return (element_addr - static_cast<slot_number_t>(smc_elem_aa->mte_addr));

  } else if (slot_type == slot_type_unknown) {
    return -1;

  } else {
    return -1;
  }
}
} /* namespace directordaemon */
