/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_RUN_HOUR_VALIDATOR_H_
#define BAREOS_SRC_DIRD_RUN_HOUR_VALIDATOR_H_

namespace directordaemon {

struct DateTimeBitfield;

class RunHourValidator {
 public:
  RunHourValidator(time_t time);
  void PrintDebugMessage(int debuglevel) const;
  bool TriggersOn(const DateTimeBitfield& date_time_bitfield);
  time_t Time() const { return time_; }

 private:
  int hour_{0};
  int mday_{0};
  int wday_{0};
  int month_{0};
  int wom_{0};
  int woy_{0};
  int yday_{0};
  time_t time_{0};
  bool is_last_week_{false};
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_RUN_HOUR_VALIDATOR_H_
