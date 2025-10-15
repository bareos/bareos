/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "bareos.pb.h"
#include "gtest/gtest.h"

#include <sys/socket.h>

#include "prototools.h"

TEST(File, WriteSendsImmediately)
{
  FILE* file = tmpfile();
  int fd = fileno(file);

  ASSERT_EQ(lseek(fd, 0, SEEK_CUR), 0);

  prototools::ProtoBidiStream stream{fd};

  bareos::core::CoreRequest send_req;
  auto* inner = send_req.mutable_acceptfile();
  inner->set_file("some string");

  ASSERT_TRUE(stream.Write(send_req));

  // there is some expected overhead for the byte size
  auto file_size = lseek(fd, 0, SEEK_CUR);
  EXPECT_GT(file_size, send_req.ByteSizeLong());

  std::vector<char> c;
  c.resize(file_size);
  rewind(file);
  ASSERT_EQ(fread(c.data(), 1, c.size(), file), c.size());

  // make sure the string was send.
  // this is technically not guarenteed to work (since protobuf may encode the
  // string) but it works for now
  EXPECT_NE(
      memmem(c.data(), c.size(), inner->file().c_str(), inner->file().size()),
      nullptr);

  fclose(file);
}

TEST(UDS, SendReceive)
{
  int sockets[2];

  ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0)
      << "could not create socket pair: " << strerror(errno);

  prototools::ProtoBidiStream end_1{sockets[0]};
  prototools::ProtoBidiStream end_2{sockets[1]};

  bareos::core::CoreRequest send_req;
  auto* inner = send_req.mutable_acceptfile();
  inner->set_file("some string");

  ASSERT_TRUE(end_1.Write(send_req));

  bareos::core::CoreRequest recv_req;

  ASSERT_TRUE(end_2.Read(recv_req));

  ASSERT_TRUE(recv_req.has_acceptfile());
  EXPECT_EQ(send_req.acceptfile().file(), recv_req.acceptfile().file());

  close(sockets[0]);
  close(sockets[1]);
}

TEST(UDS, PingPong)
{
  int sockets[2];

  ASSERT_GE(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets), 0)
      << "could not create socket pair: " << strerror(errno);

  prototools::ProtoBidiStream end_1{sockets[0]};
  prototools::ProtoBidiStream end_2{sockets[1]};

  bareos::core::CoreRequest send_req;
  auto* inner = send_req.mutable_acceptfile();
  inner->set_file("some string");

  ASSERT_TRUE(end_1.Write(send_req));

  bareos::core::CoreRequest recv_req;

  ASSERT_TRUE(end_2.Read(recv_req));
  ASSERT_TRUE(end_2.Write(recv_req));

  bareos::core::CoreRequest pingpong_req;
  ASSERT_TRUE(end_1.Read(pingpong_req));

  ASSERT_TRUE(pingpong_req.has_acceptfile());
  EXPECT_EQ(send_req.acceptfile().file(), pingpong_req.acceptfile().file());

  close(sockets[0]);
  close(sockets[1]);
}
