/**
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

#ifndef BAREOS_LIB_ASCII_CONTROL_CHARACTERS_H_
#define BAREOS_LIB_ASCII_CONTROL_CHARACTERS_H_ 1

class AsciiControlCharacters {
 public:
  static char UnitSeparator()
  {
    return unit_separator_;
  } /* smallest data item separator           */
  static char RecordSeparator()
  {
    return record_separator_;
  } /* data record separator within a group   */
  static char GroupSeparator()
  {
    return group_separator_;
  } /* group separator to separate datasets   */

 private:
  static constexpr char unit_separator_ = 0x1f;
  static constexpr char record_separator_ = 0x1e;
  static constexpr char group_separator_ = 0x1d;
};

#endif /* BAREOS_LIB_ASCII_CONTROL_CHARACTERS_H_ */
