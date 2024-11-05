/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#include "lib/bpipe.h"

#include <iostream>
#include <string>
#include <string_view>
#include <sstream>
#include "lib/berrno.h"

#include "bpipe_env_test.h"

struct result {
  int return_value;
  std::string output;
};

result make_result(Bpipe* pipe)
{
  std::string output;
  output.resize(10 * 1024);
  std::size_t total_size = 0;

  for (;;) {
    if (total_size <= output.size()) { output.resize(output.size() * 2); }

    auto count = fread(output.data() + total_size, 1,
                       output.size() - total_size, pipe->rfd);
    if (count <= 0) { break; }
    total_size += static_cast<size_t>(count);
  }

  int ret = CloseBpipe(pipe);

  output.resize(total_size);
  // non zero return values get ored with b_errno_exit/signal.
  return result{ret & ~(b_errno_exit | b_errno_signal), std::move(output)};
}

std::optional<result> run_env_tester(const char* value)
{
  std::unordered_map<std::string, std::string> env{{environment_key, value}};
  auto* pipe = OpenBpipe(env_tester, 0, "r", false, env);

  if (!pipe) {
    BErrNo b;
    std::cout << "Could not open program" << b.code() << " " << b.bstrerror()
              << std::endl;
    return std::nullopt;
  }
  return make_result(pipe);
}

std::optional<result> run_env_tester()
{
  auto* pipe = OpenBpipe(env_tester, 0, "r", false);

  if (!pipe) {
    BErrNo b;
    std::cout << "Could not open program" << b.code() << " " << b.bstrerror()
              << std::endl;
    return std::nullopt;
  }
  return make_result(pipe);
}

std::string ExpectedOutput(const char* value) { return format(value); }

std::string_view trim_right(std::string_view v)
{
  while (v.size() > 0 && isspace(v.back())) { v = v.substr(0, v.size() - 1); }

  return v;
}

[[maybe_unused]] static bool setup = (OSDependentInit(), true);

TEST(bpipe, env_val_with_space)
{
  auto* value = "a value";

  auto actual = run_env_tester(value);
  auto expected = ExpectedOutput(value);

  ASSERT_NE(actual, std::nullopt) << "env tester was not started correctly";
  EXPECT_EQ(actual->return_value, 0) << "bad return value";
  EXPECT_EQ(trim_right(actual->output), expected) << "bad output";
}

TEST(bpipe, normal_env_val)
{
  auto* value = "avalue";

  auto actual = run_env_tester(value);
  auto expected = ExpectedOutput(value);

  ASSERT_NE(actual, std::nullopt) << "env tester was not started correctly";
  EXPECT_EQ(actual->return_value, 0) << "bad return value";
  EXPECT_EQ(trim_right(actual->output), expected) << "bad output";
}

TEST(bpipe, empty_env_val)
{
  auto* value = "";

  auto actual = run_env_tester(value);
  auto expected = ExpectedOutput(value);

  ASSERT_NE(actual, std::nullopt) << "env tester was not started correctly";
  EXPECT_EQ(actual->return_value, 0) << "bad return value";
  EXPECT_EQ(trim_right(actual->output), expected) << "bad output";
}

TEST(bpipe, no_env)
{
  auto actual = run_env_tester();

  ASSERT_NE(actual, std::nullopt) << "env tester was not started correctly";
  EXPECT_EQ(actual->return_value, 1) << "bad return value";
  EXPECT_EQ(trim_right(actual->output), "") << "bad output";
}
