/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "include/make_unique.h"
#include "lib/bsock.h"
#include "lib/cram_md5.h"

#include <cassert>
#include <chrono>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>

template <typename T>
class Fifo {
 public:
  void Put(T c)
  {
    m.lock();
    deque.push_back(c);
    m.unlock();
  }

  T Get()
  {
    m.lock();
    T c = deque.front();
    deque.pop_front();
    m.unlock();
    return c;
  }

  void Clear() { deque.clear(); }
  bool Empty() { return deque.empty(); }

 private:
  std::deque<T> deque;
  std::mutex m;
};

class TestSocket : public BareosSocket {
 public:
  TestSocket() { sleep_time_after_authentication_error = 0; }
  ~TestSocket() = default;

  enum class Result
  {
    kInit,
    kSuccess,
    kReceiveTimeout
  };

  bool connect(TestSocket& other)
  {
    fifo1.Clear();
    fifo2.Clear();

    input_fifo = &fifo1;
    output_fifo = &fifo2;

    other.input_fifo = &fifo2;
    other.output_fifo = &fifo1;

    return true;
  }

  void FinInit(JobControlRecord* jcr,
               int sockfd,
               const char* who,
               const char* host,
               int port,
               struct sockaddr* lclient_addr) override
  {
  }
  bool open(JobControlRecord* jcr,
            const char* name,
            const char* host,
            char* service,
            int port,
            utime_t heart_beat,
            int* fatal) override
  {
    return true;
  }

  BareosSocket* clone() override { return nullptr; }
  bool connect(JobControlRecord* jcr,
               int retry_interval,
               utime_t max_retry_time,
               utime_t heart_beat,
               const char* name,
               const char* host,
               char* service,
               int port,
               bool verbose) override
  {
    return true;
  }

  int32_t recv() override
  {
    result = Result::kInit;
    tries = 5;

    assert(input_fifo != nullptr);

    while (input_fifo->Empty() && tries) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      --tries;
    }
    if (tries == 0) {
      result = Result::kReceiveTimeout;
      return 0;
    }

    int i = 0;
    while (!input_fifo->Empty()) { msg[i++] = input_fifo->Get(); }
    msg[i] = 0;

    message_length = i;
    result = Result::kSuccess;

    return message_length;
  }

  bool send() override
  {
    assert(output_fifo != nullptr);
    for (int i = 0; i < message_length; i++) { output_fifo->Put(msg[i]); }
    return true;
  }

  int32_t read_nbytes(char* ptr, int32_t nbytes) override { return 0; }
  int32_t write_nbytes(char* ptr, int32_t nbytes) override { return 0; }
  void close() override {}   /* close connection and destroy packet */
  void destroy() override {} /* destroy socket packet */
  int GetPeer(char* buf, socklen_t buflen) override { return 0; }
  bool SetBufferSize(uint32_t size, int rw) override { return false; }
  int SetNonblocking() override { return 0; }
  int SetBlocking() override { return 0; }
  void RestoreBlocking(int flags) override {}
  bool ConnectionReceivedTerminateSignal() override { return false; }
  int WaitData(int sec, int usec = 0) override { return 1; }
  int WaitDataIntr(int sec, int usec = 0) override { return 1; }

  Fifo<char>* input_fifo{nullptr};
  Fifo<char>* output_fifo{nullptr};

  Result result{Result::kInit};

 private:
  int tries{0};
  static Fifo<char> fifo1;
  static Fifo<char> fifo2;
};

// static variables from TestSocket
Fifo<char> TestSocket::fifo1;
Fifo<char> TestSocket::fifo2;

/* clang-format off */
// translation table for error codes
static std::map<TestSocket::Result, std::string>
    socket_result_to_string {
        {TestSocket::Result::kInit, "still in init state"},
        {TestSocket::Result::kSuccess, "success"},
        {TestSocket::Result::kReceiveTimeout, "receive timeout"}};

static std::map<CramMd5Handshake::HandshakeResult, std::string>
    cram_result_to_string {
        {CramMd5Handshake::HandshakeResult::NOT_INITIALIZED, "NOT_INITIALIZED"},
        {CramMd5Handshake::HandshakeResult::SUCCESS, "SUCCESS"},
        {CramMd5Handshake::HandshakeResult::FORMAT_MISMATCH, "FORMAT_MISMATCH"},
        {CramMd5Handshake::HandshakeResult::NETWORK_ERROR, "NETWORK_ERROR"},
        {CramMd5Handshake::HandshakeResult::WRONG_HASH, "NETWORK_ERROR"},
        {CramMd5Handshake::HandshakeResult::REPLAY_ATTACK,"REPLAY_ATTACK"}};
/* clang-format on */

/********************** Tests *****************************/

static std::string CreateQualifiedResourceName(const char* r_type_str,
                                               const char* name)
{
  return std::string(r_type_str) + static_cast<char>(0x1e) + std::string(name);
}

class CramSockets {
 public:
  CramSockets(const char* r_code_str_1,
              const char* name_1,
              const char* r_code_str_2,
              const char* name_2)
      : s1(std::make_unique<TestSocket>())
      , s2(std::make_unique<TestSocket>())
      , cram1(
            CramMd5Handshake(s1.get(),
                             "Secret-Password",
                             TlsPolicy::kBnetTlsNone,
                             CreateQualifiedResourceName(r_code_str_1, name_1)))
      , cram2(
            CramMd5Handshake(s2.get(),
                             "Secret-Password",
                             TlsPolicy::kBnetTlsNone,
                             CreateQualifiedResourceName(r_code_str_1, name_2)))
  {
    s1->connect(*s2);

    auto future1 = std::async(&CramMd5Handshake::DoHandshake, &cram1, true);
    auto future2 = std::async(&CramMd5Handshake::DoHandshake, &cram2, false);

    future1.wait();
    future2.wait();

    handshake_ok_1 = future1.get();
    handshake_ok_2 = future2.get();
  }
  std::unique_ptr<TestSocket> s1;
  std::unique_ptr<TestSocket> s2;
  CramMd5Handshake cram1;
  CramMd5Handshake cram2;
  bool handshake_ok_1, handshake_ok_2;
};

TEST(cram_md5, same_director_qualified_name)
{
  // should NOT pass cram
  CramSockets cs("R_DIRECTOR", "Test1", "R_DIRECTOR", "Test1");

  EXPECT_FALSE(cs.handshake_ok_1);
  EXPECT_EQ(cs.cram1.result, CramMd5Handshake::HandshakeResult::NETWORK_ERROR)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram1.result) << std::endl
      << "  Wich is probably a side effect of a challange attack." << std::endl;

  EXPECT_FALSE(cs.handshake_ok_2);
  EXPECT_EQ(cs.cram2.result,
            CramMd5Handshake::HandshakeResult::REPLAY_ATTACK)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram2.result) << std::endl;

  EXPECT_EQ(cs.s1->result, TestSocket::Result::kReceiveTimeout)
      << "---> Outbound TestSocket Error: "
      << socket_result_to_string.at(cs.s1->result);
  EXPECT_EQ(cs.s2->result, TestSocket::Result::kSuccess)
      << "---> Inbound TestSocket Error: "
      << socket_result_to_string.at(cs.s2->result);
}

TEST(cram_md5, different_director_qualified_name)
{
  // should pass cram
  CramSockets cs("R_DIRECTOR", "Test1", "R_DIRECTOR", "Test2");

  EXPECT_TRUE(cs.handshake_ok_1);
  EXPECT_EQ(cs.cram1.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram1.result) << std::endl
      << "  Wich is probably a side effect of a challange attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_2);
  EXPECT_EQ(cs.cram2.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram2.result) << std::endl;

  EXPECT_EQ(cs.s1->result, TestSocket::Result::kSuccess)
      << "---> Outbound TestSocket Error: "
      << socket_result_to_string.at(cs.s1->result);
  EXPECT_EQ(cs.s2->result, TestSocket::Result::kSuccess)
      << "---> Inbound TestSocket Error: "
      << socket_result_to_string.at(cs.s2->result);
}

TEST(cram_md5, same_stored_qualified_name)
{
  // should pass cram
  CramSockets cs("R_STORAGE", "Test1", "R_STORAGE", "Test1");

  EXPECT_TRUE(cs.handshake_ok_1);
  EXPECT_EQ(cs.cram1.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram1.result) << std::endl
      << "  Wich is probably a side effect of a challange attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_2);
  EXPECT_EQ(cs.cram2.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram2.result) << std::endl;

  EXPECT_EQ(cs.s1->result, TestSocket::Result::kSuccess)
      << "---> Outbound TestSocket Error: "
      << socket_result_to_string.at(cs.s1->result);
  EXPECT_EQ(cs.s2->result, TestSocket::Result::kSuccess)
      << "---> Inbound TestSocket Error: "
      << socket_result_to_string.at(cs.s2->result);
}

TEST(cram_md5, different_stored_qualified_name)
{
  // should pass cram
  CramSockets cs("R_STORAGE", "Test1", "R_STORAGE", "Test2");

  EXPECT_TRUE(cs.handshake_ok_1);
  EXPECT_EQ(cs.cram1.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram1.result) << std::endl
      << "  Wich is probably a side effect of a challange attack." << std::endl;

  EXPECT_TRUE(cs.handshake_ok_2);
  EXPECT_EQ(cs.cram2.result, CramMd5Handshake::HandshakeResult::SUCCESS)
      << "  CramMd5Handshake::HandshakeResult::"
      << cram_result_to_string.at(cs.cram2.result) << std::endl;

  EXPECT_EQ(cs.s1->result, TestSocket::Result::kSuccess)
      << "---> Outbound TestSocket Error: "
      << socket_result_to_string.at(cs.s1->result);
  EXPECT_EQ(cs.s2->result, TestSocket::Result::kSuccess)
      << "---> Inbound TestSocket Error: "
      << socket_result_to_string.at(cs.s2->result);
}

