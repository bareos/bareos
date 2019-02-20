
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
#ifndef BAREOS_SRC_DIRD_NDMP_SLOT2ELEMADDR_H_
#define BAREOS_SRC_DIRD_NDMP_SLOT2ELEMADDR_H_

namespace directordaemon {

slot_number_t GetElementAddressByBareosSlotNumber(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t slotnumber);

slot_number_t GetBareosSlotNumberByElementAddress(
    smc_element_address_assignment* smc_elem_aa,
    slot_type_t slot_type,
    slot_number_t element_addr);

} /* namespace directordaemon */
#endif  // BAREOS_SRC_DIRD_NDMP_SLOT2ELEMADDR_H_
