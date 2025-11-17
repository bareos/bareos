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

#include "util.h"

namespace bco = bareos::common;

void time_native_to_grpc(google::protobuf::Timestamp* time, struct timespec t)
{
  time->set_seconds(t.tv_sec);
  time->set_nanos(t.tv_nsec);
}

void time_grpc_to_native(struct timespec* t,
                         const google::protobuf::Timestamp& time)
{
  t->tv_sec = time.seconds();
  t->tv_nsec = time.nanos();
}

void stat_native_to_grpc(bco::OsStat* grpc, const struct stat* os)
{
  grpc->set_dev(os->st_dev);
  grpc->set_ino(os->st_ino);
  grpc->set_mode(os->st_mode);
  grpc->set_nlink(os->st_nlink);
  grpc->set_uid(os->st_uid);
  grpc->set_gid(os->st_gid);
  grpc->set_rdev(os->st_rdev);
  grpc->set_size(os->st_size);

  time_native_to_grpc(grpc->mutable_atime(), os->st_atim);
  time_native_to_grpc(grpc->mutable_mtime(), os->st_mtim);
  time_native_to_grpc(grpc->mutable_ctime(), os->st_ctim);

  grpc->set_blksize(os->st_blksize);
  grpc->set_blocks(os->st_blocks);
}

void stat_grpc_to_native(struct stat* os, const bco::OsStat& grpc)
{
  os->st_dev = grpc.dev();
  os->st_ino = grpc.ino();
  os->st_mode = grpc.mode();
  os->st_nlink = grpc.nlink();
  os->st_uid = grpc.uid();
  os->st_gid = grpc.gid();
  os->st_rdev = grpc.rdev();
  os->st_size = grpc.size();

  time_grpc_to_native(&os->st_atim, grpc.atime());
  time_grpc_to_native(&os->st_mtim, grpc.mtime());
  time_grpc_to_native(&os->st_ctim, grpc.ctime());

  os->st_blksize = grpc.blksize();
  os->st_blocks = grpc.blocks();
}
