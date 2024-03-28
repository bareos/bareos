/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

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
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#endif

#include "include/fcntl_def.h"
#include "lib/berrno.h"

// Error message differ on supported OS
#ifdef HAVE_LINUX_OS
const char* open_error_message = "No such file or directory";
const char* socket_error_message = "Invalid argument";
const char* bind_error_message = "Bad file descriptor";
#elif defined HAVE_FREEBSD_OS
const char* open_error_message = "No such file or directory";
const char* socket_error_message = "Protocol wrong type for socket";
const char* bind_error_message = "Invalid argument";
#elif defined HAVE_SUN_OS
const char* open_error_message = "No such file or directory";
const char* socket_error_message = "Invalid argument";
const char* bind_error_message = "Bad file number";
#elif defined _MSVC_LANG
const char* open_error_message
    = "No such file or directory (errno=2 | win_error=0x00000002)";
const char* win_open_error_message
    = "The system cannot find the file specified.\r\n";
const char* socket_error_message
    = "No such file or directory (errno=2 | win_error=0x0000273C)";
const char* win_socket_error_message
    = "The support for the specified socket type does not exist in this "
      "address family.\r\n";
const char* bind_error_message
    = "No such file or directory (errno=2 | win_error=0x000027";
#elif defined HAVE_WIN32
const char* open_error_message
    = "No such file or directory (errno=2 | win_error=0x00000002)";
const char* win_open_error_message = "File not found (0x00000002)";
const char* socket_error_message
    = "No such file or directory (errno=2 | win_error=0x0000273C)";
const char* win_socket_error_message = "Unknown windows error (0x0000273C)";
const char* bind_error_message
    = "No such file or directory (errno=2 | win_error=0x000027";
#else
#  error "error_messages for current OS undefined"
#endif


TEST(berrno, errors)
{
  OSDependentInit();
  int operation_result = 0;

  operation_result = open("filethatdoesnotexist", O_RDONLY);

  EXPECT_LT(operation_result, 0);
  if (operation_result < 0) {
    BErrNo be;
    EXPECT_STREQ(be.bstrerror(), open_error_message);
  }

#ifdef HAVE_WIN32
  errno = 0;
  operation_result = socket(AF_INET, -1, 0);
  EXPECT_LT(operation_result, 0);
  if (operation_result < 0) {
    BErrNo be;
    EXPECT_STREQ(be.bstrerror(), win_socket_error_message);
  }

  operation_result = open("filethatdoesnotexist", O_RDONLY);
  errno |= b_errno_win32;
  EXPECT_LT(operation_result, 0);
  if (operation_result < 0) {
    BErrNo be;
    EXPECT_STREQ(be.bstrerror(), win_open_error_message);
  }

#endif

  errno = 2;
  operation_result = socket(AF_INET, -1, 0);
  EXPECT_LT(operation_result, 0);
  if (operation_result < 0) {
    BErrNo be;
    EXPECT_STREQ(be.bstrerror(), socket_error_message);
  }

  operation_result = bind(-5, nullptr, 0);
  EXPECT_LT(operation_result, 0);
  if (operation_result < 0) {
    BErrNo be;
    EXPECT_THAT(be.bstrerror(), testing::StartsWith(bind_error_message));
  }
}
