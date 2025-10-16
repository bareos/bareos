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

#ifndef BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_
#define BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_

#include <google/protobuf/message.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

namespace prototools {
struct ProtoInputStream {
  google::protobuf::io::FileInputStream stream;

  ProtoInputStream(int in_fd) : stream{in_fd} {}

  ProtoInputStream(const ProtoInputStream&) = delete;
  ProtoInputStream& operator=(const ProtoInputStream&) = delete;
  ProtoInputStream(ProtoInputStream&&) = delete;
  ProtoInputStream& operator=(ProtoInputStream&&) = delete;

  template <typename Message> bool Read(Message& msg)
  {
    google::protobuf::io::CodedInputStream coded{&stream};
    uint64_t RequestSize = 0;
    if (!coded.ReadVarint64(&RequestSize)) { return false; }

    // make sure we do not read too much data; protobuf parsing always
    // consumes as much data as possible; there are no "end message" markers
    auto limit = coded.PushLimit(RequestSize);
    if (!msg.ParseFromCodedStream(&coded)) {
      coded.PopLimit(limit);
      return false;
      // JobLog(ctx, M_ERROR, FMT_STRING("could not parse request"));
      // TODO: we should somehow abort here, since we cannot recover from this
    }
    coded.PopLimit(limit);

    return true;
  }
};

struct ProtoOutputStream {
  google::protobuf::io::FileOutputStream stream;

  ProtoOutputStream(int out_fd) : stream{out_fd} {}

  ProtoOutputStream(const ProtoOutputStream&) = delete;
  ProtoOutputStream& operator=(const ProtoOutputStream&) = delete;
  ProtoOutputStream(ProtoOutputStream&&) = delete;
  ProtoOutputStream& operator=(ProtoOutputStream&&) = delete;

  template <typename Message> bool WriteBuffered(Message& msg)
  {
    google::protobuf::io::CodedOutputStream coded{&stream};
    uint64_t size = msg.ByteSizeLong();
    coded.WriteVarint64(size);
    int pre = coded.ByteCount();
    msg.SerializeWithCachedSizes(&coded);
    if (coded.HadError()) { return false; }
    int post = coded.ByteCount();
    if (post - pre != static_cast<std::int64_t>(size)) { return false; }
    return true;
  }
  template <typename Message> inline bool Write(Message& msg)
  {
    WriteBuffered(msg);

    // this api is only used to send single messages
    // so we need to flush after every write as it never makes sense
    // to "pool" multiple requests.
    return stream.Flush();
  }
};

struct ProtoBidiStream {
  ProtoInputStream input;
  ProtoOutputStream output;

  std::size_t read_count{}, write_count{};

  // for a bidirectional file descriptor; think socket
  ProtoBidiStream(int bidi_fd) : ProtoBidiStream(bidi_fd, bidi_fd) {}

  ProtoBidiStream(int in_fd, int out_fd) : input{in_fd}, output{out_fd} {}

  ProtoBidiStream(const ProtoBidiStream&) = delete;
  ProtoBidiStream& operator=(const ProtoBidiStream&) = delete;
  ProtoBidiStream(ProtoBidiStream&&) = delete;
  ProtoBidiStream& operator=(ProtoBidiStream&&) = delete;


  template <typename Message> bool Read(Message& msg)
  {
    read_count += 1;
    return input.Read(msg);
  }

  template <typename Message> bool Write(Message& msg)
  {
    write_count += 1;
    return output.Write(msg);
  }

  template <typename Message> bool WriteBuffered(Message& msg)
  {
    write_count += 1;
    return output.WriteBuffered(msg);
  }
};
};  // namespace prototools

#endif  // BAREOS_PLUGINS_FILED_GRPC_PROTOTOOLS_H_
