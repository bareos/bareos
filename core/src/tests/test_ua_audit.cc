/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "dird/ua.h"

namespace directordaemon {

TEST(ua_audit, normalize_message_text_trims_trailing_newline)
{
  const std::string normalized
      = NormalizeAuditMessageText("llist jobs days=1\n");
  ASSERT_EQ(normalized, "llist jobs days=1");
}

TEST(ua_audit, normalize_message_text_trims_trailing_crlf)
{
  const std::string normalized
      = NormalizeAuditMessageText("Job queued. JobId=1\r\n");
  ASSERT_EQ(normalized, "Job queued. JobId=1");
}

TEST(ua_audit, normalize_message_text_keeps_interior_newline)
{
  const std::string normalized = NormalizeAuditMessageText("line 1\nline 2\n");
  ASSERT_EQ(normalized, "line 1\nline 2");
}

TEST(ua_audit, normalize_message_text_handles_null_input)
{
  const std::string normalized = NormalizeAuditMessageText(nullptr);
  ASSERT_EQ(normalized, "");
}

TEST(ua_audit, normalize_message_text_handles_empty_input)
{
  const std::string normalized = NormalizeAuditMessageText("");
  ASSERT_EQ(normalized, "");
}

}  // namespace directordaemon
