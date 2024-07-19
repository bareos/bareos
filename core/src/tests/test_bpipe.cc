/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
using ::testing::IsNull;
using ::testing::NotNull;

#include "lib/bpipe.h"
#include "lib/berrno.h"

#undef fgets

static int events[NSIG];
class SignalCatcher {
  static void SignalHandler(int signal) { events[signal]++; }
  int sig_num;
  struct sigaction oldact {};

 public:
  SignalCatcher(int t_sig_num) noexcept : sig_num(t_sig_num)
  {
    struct sigaction sa {};
    sa.sa_handler = &SignalHandler;
    sigaction(sig_num, &sa, &oldact);
    events[sig_num] = 0;
  }
  ~SignalCatcher() noexcept
  {
    sigaction(sig_num, &oldact, nullptr);
    events[sig_num] = 0;
  }
  int num_events() const { return events[sig_num]; }
};


/* Run a command with exit code == 0 */
TEST(bpipe, success)
{
  Bpipe* bp = OpenBpipe("true", 30, "r");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, IsNull());
  ASSERT_THAT(bp->rfd, NotNull());
  EXPECT_EQ(ferror(bp->rfd), 0);
  EXPECT_NE(fgetc(bp->rfd), 0);
  EXPECT_NE(feof(bp->rfd), 0);
  EXPECT_EQ(CloseBpipe(bp), 0);
}

/* Run a command with exit code != 0 */
TEST(bpipe, failure)
{
  Bpipe* bp = OpenBpipe("false", 30, "r");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, IsNull());
  ASSERT_THAT(bp->rfd, NotNull());
  EXPECT_EQ(ferror(bp->rfd), 0);
  EXPECT_NE(fgetc(bp->rfd), 0);
  EXPECT_NE(feof(bp->rfd), 0);
  EXPECT_NE(CloseBpipe(bp), 0);
}

/* Write data into a pipe */
TEST(bpipe, simple_write)
{
  Bpipe* bp = OpenBpipe("cat", 30, "w");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, NotNull());
  ASSERT_THAT(bp->rfd, IsNull());
  ASSERT_THAT(bp->timer_id, NotNull());
  EXPECT_EQ(ferror(bp->wfd), 0);
  EXPECT_NE(fputs("Some String\n", bp->wfd), 0);
  EXPECT_EQ(ferror(bp->wfd), 0);
  EXPECT_EQ(CloseWpipe(bp), 1);
  ASSERT_THAT(bp->wfd, IsNull());
  ASSERT_FALSE(bp->timer_id->killed);
  EXPECT_EQ(CloseBpipe(bp), 0);
}

/* Run command and kill with timeout */
TEST(bpipe, timeout)
{
  using namespace std::chrono_literals;
  Bpipe* bp = OpenBpipe("cat", 1, "r");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->timer_id, NotNull());
  ASSERT_FALSE(bp->timer_id->killed);
  std::this_thread::sleep_for(2s);
  ASSERT_TRUE(bp->timer_id->killed);
  EXPECT_EQ(CloseBpipe(bp), b_errno_signal | 15);
}

/* Run command and reset timeout */
TEST(bpipe, child_operates_properly)
{
  using namespace std::chrono_literals;
  Bpipe* bp = OpenBpipe("cat", 1, "w");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, NotNull());
  ASSERT_THAT(bp->rfd, IsNull());
  ASSERT_THAT(bp->timer_id, NotNull());
  EXPECT_EQ(ferror(bp->wfd), 0);
  EXPECT_NE(fputs("First String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), 0);
  TimerKeepalive(*bp->timer_id);
  std::this_thread::sleep_for(1s);
  EXPECT_NE(fputs("Second String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), 0);
  TimerKeepalive(*bp->timer_id);
  std::this_thread::sleep_for(1s);
  EXPECT_NE(fputs("Third String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), 0);
  TimerKeepalive(*bp->timer_id);
  EXPECT_FALSE(bp->timer_id->killed);
  EXPECT_EQ(CloseWpipe(bp), 1);
  EXPECT_THAT(bp->wfd, IsNull());
  EXPECT_EQ(CloseBpipe(bp), 0);
}

/* Run command, reset timeout, then hang and be killed. */
TEST(bpipe, child_operates_flaky)
{
  using namespace std::chrono_literals;
  Bpipe* bp = OpenBpipe("cat", 1, "w");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, NotNull());
  ASSERT_THAT(bp->rfd, IsNull());
  ASSERT_THAT(bp->timer_id, NotNull());
  EXPECT_EQ(ferror(bp->wfd), 0);
  EXPECT_NE(fputs("First String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), 0);
  TimerKeepalive(*bp->timer_id);
  std::this_thread::sleep_for(1s);
  EXPECT_NE(fputs("Second String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), 0);
  TimerKeepalive(*bp->timer_id);
  std::this_thread::sleep_for(5s);  // here we pretend to hang
  EXPECT_TRUE(bp->timer_id->killed);
  EXPECT_EQ(CloseBpipe(bp), b_errno_signal | 15);
}

/* Write to a command that already exited. */
TEST(bpipe, sigpipe)
{
  using namespace std::chrono_literals;
  SignalCatcher sigpipe{SIGPIPE};
  Bpipe* bp = OpenBpipe("true", 30, "w");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, NotNull());
  ASSERT_THAT(bp->rfd, IsNull());
  std::this_thread::sleep_for(1s);  // give "true" a second to finish
  EXPECT_EQ(ferror(bp->wfd), 0);
  EXPECT_EQ(sigpipe.num_events(), 0);
  EXPECT_NE(fputs("First String\n", bp->wfd), 0);
  EXPECT_EQ(fflush(bp->wfd), EOF);     // here we fail
  EXPECT_EQ(sigpipe.num_events(), 1);  // and we got a SIGPIPE
  EXPECT_EQ(CloseBpipe(bp), 0);
}

/* Read from a command that doesn't write to stdout and be killed. */
TEST(bpipe, stalled_read)
{
  using namespace std::chrono_literals;
  char buffer[1024];
  Bpipe* bp = OpenBpipe("cat", 1, "r");
  ASSERT_THAT(bp, NotNull());
  ASSERT_THAT(bp->wfd, IsNull());
  ASSERT_THAT(bp->rfd, NotNull());
  EXPECT_EQ(ferror(bp->rfd), 0);
  EXPECT_THAT(fgets(buffer, sizeof(buffer), bp->rfd), IsNull());
  EXPECT_EQ(CloseBpipe(bp), b_errno_signal | 15);
}
