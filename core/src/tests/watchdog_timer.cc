/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#endif

#include "lib/btimers.h"
#include "lib/bsignal.h"
#include "lib/watchdog_timer.h"

#include <iostream>
#include <thread>

static bool signal_handler_called = false;

static void SignalHandler(int signo, siginfo_t* info, void* extra)
{
  signal_handler_called = true;
}

static void InstallSignalHandler()
{
  struct sigaction action;

  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = SignalHandler;

  EXPECT_EQ(sigaction(TIMEOUT_SIGNAL, &action, NULL), 0);
}

TEST(watchdog, legacy_thread_timer)
{
  InstallSignalHandler();

  signal_handler_called = false;

  start_thread_timer(nullptr, pthread_self(), 1);
  int timeout = 0;
  while (!signal_handler_called && ++timeout < 20) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_LT(timeout, 20);
  EXPECT_TRUE(signal_handler_called);

  TimerThread::Stop();
}

TEST(watchdog, base_class)
{
  const TimerThread::Timer* wd;
  {
    WatchdogTimer watchdog(nullptr);
    wd = watchdog.GetTimerControlledItem();
  }
  EXPECT_FALSE(IsRegisteredTimer(wd));
}

TEST(watchdog, thread_watchdog)
{
  InstallSignalHandler();

  const TimerThread::Timer* wd;

  BThreadWatchdog watchdog(std::chrono::milliseconds(100), nullptr);
  wd = watchdog.GetTimerControlledItem();

  signal_handler_called = false;
  int timeout = 0;

  while (!signal_handler_called && ++timeout < 20) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_FALSE(IsRegisteredTimer(wd));
  EXPECT_TRUE(signal_handler_called);
}

TEST(watchdog, thread_watchdog_separated_start_command)
{
  InstallSignalHandler();

  const TimerThread::Timer* wd;

  BThreadWatchdog watchdog(nullptr);
  wd = watchdog.GetTimerControlledItem();

  signal_handler_called = false;
  int timeout = 0;

  watchdog.Start(std::chrono::milliseconds(100));

  while (!signal_handler_called && ++timeout < 20) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_LT(timeout, 20);
  EXPECT_FALSE(IsRegisteredTimer(wd));
  EXPECT_TRUE(signal_handler_called);
}

TEST(watchdog, timer_stop)
{
  const TimerThread::Timer* wd;

  BThreadWatchdog watchdog(std::chrono::seconds(1), nullptr);
  wd = watchdog.GetTimerControlledItem();

  watchdog.Stop();
  EXPECT_FALSE(IsRegisteredTimer(wd));
  EXPECT_EQ(watchdog.GetTimerControlledItem(), nullptr);
}
