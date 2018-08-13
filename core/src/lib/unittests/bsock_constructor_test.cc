/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#include <memory>

/* test private members */
#define protected public
#define private public
#include <bareos.h>


TEST(bsock, bareossockettcp_standard_constructor_test)
{
   std::shared_ptr<BareosSocketTCP> s = std::make_shared<BareosSocketTCP>();

   EXPECT_EQ(s->fd_, -1);
   EXPECT_EQ(s->read_seqno, 0);
   EXPECT_NE(s->msg, nullptr);
   EXPECT_NE(s->errmsg,nullptr);
   EXPECT_EQ(s->spool_fd_, -1);
   EXPECT_EQ(s->src_addr, nullptr);
   EXPECT_EQ(s->in_msg_no, 0);
   EXPECT_EQ(s->out_msg_no, 0);
   EXPECT_EQ(s->message_length, 0);
   EXPECT_EQ(s->timer_start, 0);
   EXPECT_EQ(s->b_errno, 0);
   EXPECT_EQ(s->blocking_, 1);
   EXPECT_EQ(s->errors, 0);
   EXPECT_EQ(s->suppress_error_msgs_, false);
   EXPECT_EQ(s->sleep_time_after_authentication_error, 5);
   EXPECT_EQ(s->client_addr.sa_family, AF_UNSPEC);
   EXPECT_EQ(s->client_addr.sa_data[0], 0);
   EXPECT_EQ(s->peer_addr.sin_port, 0);
   EXPECT_EQ(s->peer_addr.sin_addr.s_addr, 0);
   EXPECT_EQ(s->local_daemon_type_, BareosDaemonType::kUndefined);
   EXPECT_EQ(s->remote_daemon_type_, BareosDaemonType::kUndefined);

/* protected: */
   EXPECT_EQ(s->jcr_, nullptr);
//   EXPECT_EQ(s->mutex_, PTHREAD_MUTEX_INITIALIZER);
   EXPECT_EQ(s->who_, nullptr);
   EXPECT_EQ(s->host_, nullptr);
   EXPECT_EQ(s->port_ , -1);
   EXPECT_EQ(s->tid_, nullptr);
   EXPECT_EQ(s->data_end_, 0);
   EXPECT_EQ(s->FileIndex_, 0);
   EXPECT_EQ(s->timed_out_ ? true :false, false);
   EXPECT_EQ(s->terminated_, false);
   EXPECT_EQ(s->cloned_, false);
   EXPECT_EQ(s->spool_, false);
   EXPECT_EQ(s->use_locking_, false);
   EXPECT_EQ(s->use_bursting_, false);
   EXPECT_EQ(s->use_keepalive_, true);
   EXPECT_EQ(s->bwlimit_, 0);
   EXPECT_EQ(s->nb_bytes_, 0);
   EXPECT_EQ(s->last_tick_, 0);
   EXPECT_EQ(s->tls_established_, false);
}

TEST(bsock, bareossockettcp_copy_constructor_test)
{
   std::shared_ptr<BareosSocketTCP> s = std::make_shared<BareosSocketTCP>();
}
