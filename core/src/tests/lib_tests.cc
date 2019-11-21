/**
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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "include/version_numbers.h"
#define BAREOS_TEST_LIB
#include "lib/bnet.h"
#include "lib/bstringlist.h"
#include "lib/ascii_control_characters.h"
#include "lib/util.h"

TEST(BStringList, ConstructorsTest)
{
  BStringList list1;
  EXPECT_TRUE(list1.empty());

  list1.emplace_back(std::string("Test123"));
  EXPECT_EQ(0, list1.at(0).compare(std::string("Test123")));

  BStringList list2(list1);
  EXPECT_EQ(1, list2.size());
  EXPECT_EQ(0, list2.front().compare(std::string("Test123")));
}

TEST(BStringList, AppendTest)
{
  BStringList list1;
  std::vector<std::string> list{"T", "est", "123"};
  list1.Append(list);
  EXPECT_EQ(0, list1.front().compare(std::string("T")));
  list1.erase(list1.begin());
  EXPECT_EQ(0, list1.front().compare(std::string("est")));
  list1.erase(list1.begin());
  EXPECT_EQ(0, list1.front().compare(std::string("123")));

  BStringList list2;
  list2.Append('T');
  list2.Append('e');
  EXPECT_EQ(0, list2.front().compare(std::string("T")));
  list2.erase(list2.begin());
  EXPECT_EQ(0, list2.front().compare(std::string("e")));
}

TEST(BStringList, JoinTest)
{
  BStringList list1;
  list1 << "Test";
  list1 << 1 << 23;
  EXPECT_EQ(3, list1.size());
  EXPECT_STREQ(list1.Join().c_str(), "Test123");
  EXPECT_STREQ(list1.Join(' ').c_str(), "Test 1 23");

  BStringList list2;
  list2.Append("Test");
  list2.Append("123");

  std::string s = list2.Join(AsciiControlCharacters::RecordSeparator());
  EXPECT_EQ(8, s.size());

  std::string test{"Test"};
  test += AsciiControlCharacters::RecordSeparator();  // 0x1e
  test += "123";

  EXPECT_STREQ(s.c_str(), test.c_str());
}

TEST(BStringList, SplitStringTest)
{
  std::string test{"Test@123@String"};
  BStringList list1(test, '@');
  EXPECT_EQ(3, list1.size());

  EXPECT_STREQ("Test", list1.front().c_str());
  list1.erase(list1.begin());
  EXPECT_STREQ("123", list1.front().c_str());
  list1.erase(list1.begin());
  EXPECT_STREQ("String", list1.front().c_str());
}

TEST(BStringList, SplitStringTestStringSeparator)
{
  std::string test{"Test::123::String::::"};
  BStringList list1(test, "::");
  EXPECT_EQ(5, list1.size());

  EXPECT_STREQ("Test", list1.front().c_str());
  list1.erase(list1.begin());
  EXPECT_STREQ("123", list1.front().c_str());
  list1.erase(list1.begin());
  EXPECT_STREQ("String", list1.front().c_str());
}

TEST(BNet, ReadoutCommandIdFromStringTest)
{
  bool ok;
  uint32_t id;

  std::string message1 = "1000";
  message1 += 0x1e;
  message1 += "OK: <director-name> Version: <version>";
  BStringList list_of_arguments1(message1, 0x1e);
  ok = ReadoutCommandIdFromMessage(list_of_arguments1, id);
  EXPECT_EQ(id, kMessageIdOk);
  EXPECT_EQ(ok, true);

  std::string message2 = "1001";
  message2 += 0x1e;
  message2 += "OK: <director-name> Version: <version>";
  BStringList list_of_arguments2(message2, 0x1e);
  ok = ReadoutCommandIdFromMessage(list_of_arguments2, id);
  EXPECT_NE(id, kMessageIdOk);
  EXPECT_EQ(ok, true);
}

TEST(BNet, EvaluateResponseMessage_Wrong_Id)
{
  bool ok;

  std::string message3 = "A1001";
  message3 += 0x1e;
  message3 += "OK: <director-name> Version: <version>";

  uint32_t id = kMessageIdUnknown;
  BStringList args;
  ok = EvaluateResponseMessageId(message3, id, args);

  EXPECT_EQ(id, kMessageIdUnknown);
  EXPECT_EQ(ok, false);

  const char* m3{"A1001 OK: <director-name> Version: <version>"};
  EXPECT_STREQ(args.JoinReadable().c_str(), m3);
}

TEST(BNet, EvaluateResponseMessage_Correct_Id)
{
  bool ok;
  uint32_t id;

  std::string message4 = "1001";
  message4 += 0x1e;
  message4 += "OK: <director-name> Version: <version>";

  BStringList args;
  ok = EvaluateResponseMessageId(message4, id, args);

  EXPECT_EQ(id, kMessageIdPamRequired);
  EXPECT_EQ(ok, true);

  const char* m3{"1001 OK: <director-name> Version: <version>"};
  EXPECT_STREQ(args.JoinReadable().c_str(), m3);
}

enum
{
  R_DIRECTOR = 1,
  R_CLIENT,
  R_JOB,
  R_STORAGE,
  R_CONSOLE
};

#include "lib/qualified_resource_name_type_converter.h"

static void do_get_name_from_hello_test(const char* client_string_fmt,
                                        const char* client_name,
                                        const std::string& r_type_test,
                                        const BareosVersionNumber& version_test)
{
  char bashed_client_name[20];
  strncpy(bashed_client_name, client_name, 20);
  BashSpaces(bashed_client_name);

  char output_text[64];
  sprintf(output_text, client_string_fmt, bashed_client_name);

  std::string name;
  std::string r_type_str;
  BareosVersionNumber version = BareosVersionNumber::kUndefined;

  bool ok = GetNameAndResourceTypeAndVersionFromHello(output_text, name,
                                                      r_type_str, version);

  EXPECT_TRUE(ok);
  EXPECT_STREQ(name.c_str(), client_name);
  EXPECT_STREQ(r_type_str.c_str(), r_type_test.c_str());
  EXPECT_EQ(version, version_test);
}

TEST(Util, get_name_from_hello_test)
{
  // clang-format off
  do_get_name_from_hello_test("Hello Client %s calling",
                              "Test Client",        "R_CLIENT",   BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello Storage calling Start Job %s",
                              "Test Client",        "R_JOB",      BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello %s",
                              "Console Name",       "R_CONSOLE",  BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello %s",
                              "*UserAgent*",        "R_CONSOLE",  BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello %s",
                              "*UserAgent*",        "R_CONSOLE",  BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello %s calling version 18.2.4rc2",
                              "Console",            "R_CONSOLE",  BareosVersionNumber::kRelease_18_2);
  do_get_name_from_hello_test("Hello Director %s calling\n",
                              "bareos dir",         "R_DIRECTOR", BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello Start Storage Job %s",
                              "Test Job",           "R_JOB",      BareosVersionNumber::kUndefined);
  do_get_name_from_hello_test("Hello Client %s FdProtocolVersion=123 calling\n",
                              "Test Client again",  "R_CLIENT",    BareosVersionNumber::kUndefined);
  // clang-format on
}

TEST(Util, version_number_test)
{
  EXPECT_EQ(BareosVersionNumber::kRelease_18_2,
            static_cast<BareosVersionNumber>(1802));
  EXPECT_EQ(BareosVersionNumber::kUndefined,
            static_cast<BareosVersionNumber>(1));
  EXPECT_NE(BareosVersionNumber::kRelease_18_2,
            static_cast<BareosVersionNumber>(1702));
  EXPECT_GT(BareosVersionNumber::kRelease_18_2,
            BareosVersionNumber::kUndefined);
}

TEST(Util, version_number_major_minor)
{
  BareosVersionNumber version = BareosVersionNumber::kRelease_18_2;
  BareosVersionToMajorMinor v(version);
  EXPECT_EQ(v.major, 18);
  EXPECT_EQ(v.minor, 2);
}

#include "filed/evaluate_job_command.h"

TEST(Filedaemon, evaluate_jobcommand_from_18_2_test)
{
  /* command with ssl argument */
  static const char jobcmd_kVersionFrom_18_2[] =
      "JobId=111 Job=FirstJob SDid=222 SDtime=333 Authorization=SecretOne "
      "ssl=4\n";

  filedaemon::JobCommand eval(jobcmd_kVersionFrom_18_2);

  EXPECT_TRUE(eval.EvaluationSuccesful());
  EXPECT_EQ(eval.protocol_version_,
            filedaemon::JobCommand::ProtocolVersion::kVersionFrom_18_2);
  EXPECT_EQ(eval.job_id_, 111);
  EXPECT_STREQ(eval.job_, "FirstJob");
  EXPECT_EQ(eval.vol_session_id_, 222);
  EXPECT_EQ(eval.vol_session_time_, 333);
  EXPECT_STREQ(eval.sd_auth_key_, "SecretOne");
  EXPECT_EQ(eval.tls_policy_, 4);
}

TEST(Filedaemon, evaluate_jobcommand_before_18_2_test)
{
  /* command without ssl argument */
  static char jobcmdssl_KVersionBefore_18_2[] =
      "JobId=123 Job=SecondJob SDid=456 SDtime=789 Authorization=SecretTwo";

  filedaemon::JobCommand eval(jobcmdssl_KVersionBefore_18_2);

  EXPECT_TRUE(eval.EvaluationSuccesful());
  EXPECT_EQ(eval.protocol_version_,
            filedaemon::JobCommand::ProtocolVersion::KVersionBefore_18_2);
  EXPECT_EQ(eval.job_id_, 123);
  EXPECT_STREQ(eval.job_, "SecondJob");
  EXPECT_EQ(eval.vol_session_id_, 456);
  EXPECT_EQ(eval.vol_session_time_, 789);
  EXPECT_STREQ(eval.sd_auth_key_, "SecretTwo");
}

TEST(Filedaemon, evaluate_jobcommand_wrong_format_test)
{
  /* malformed command  */
  static char jobcmdssl_KVersionBefore_18_2[] = "JobId=123 Job=Foo";

  filedaemon::JobCommand eval(jobcmdssl_KVersionBefore_18_2);

  EXPECT_FALSE(eval.EvaluationSuccesful());
  EXPECT_EQ(eval.protocol_version_,
            filedaemon::JobCommand::ProtocolVersion::kVersionUndefinded);
}
