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
#include <stdlib.h>

/* test private members */
#define protected public
#define private public
#include <bareos.h>


TEST(bsock, bareossockettcp_standard_constructor_test)
{
   std::shared_ptr<BareosSocketTCP> p = std::make_shared<BareosSocketTCP>();

/* private BAREOS_SOCKET: */
   EXPECT_EQ(p->fd_, -1);
   EXPECT_EQ(p->read_seqno, 0);
   EXPECT_NE(p->msg, nullptr);
   EXPECT_NE(p->errmsg,nullptr);
   EXPECT_EQ(p->spool_fd_, -1);
   EXPECT_EQ(p->src_addr, nullptr);
   EXPECT_EQ(p->in_msg_no, 0);
   EXPECT_EQ(p->out_msg_no, 0);
   EXPECT_EQ(p->message_length, 0);
   EXPECT_EQ(p->timer_start, 0);
   EXPECT_EQ(p->b_errno, 0);
   EXPECT_EQ(p->blocking_, 1);
   EXPECT_EQ(p->errors, 0);
   EXPECT_EQ(p->suppress_error_msgs_, false);
   EXPECT_EQ(p->sleep_time_after_authentication_error, 5);
   EXPECT_EQ(p->client_addr.sa_family, AF_UNSPEC);
   EXPECT_EQ(p->client_addr.sa_data[0], 0);
   EXPECT_EQ(p->peer_addr.sin_port, 0);
   EXPECT_EQ(p->peer_addr.sin_addr.s_addr, 0);
   EXPECT_EQ(p->local_daemon_type_, BareosDaemonType::kUndefined);
   EXPECT_EQ(p->remote_daemon_type_, BareosDaemonType::kUndefined);

/* protected BAREOS_SOCKET: */
   EXPECT_EQ(p->jcr_, nullptr);
//   EXPECT_EQ(p->mutex_, PTHREAD_MUTEX_INITIALIZER);
   EXPECT_EQ(p->who_, nullptr);
   EXPECT_EQ(p->host_, nullptr);
   EXPECT_EQ(p->port_ , -1);
   EXPECT_EQ(p->tid_, nullptr);
   EXPECT_EQ(p->data_end_, 0);
   EXPECT_EQ(p->FileIndex_, 0);
   EXPECT_EQ(p->timed_out_ ? true :false, false);
   EXPECT_EQ(p->terminated_, false);
   EXPECT_EQ(p->cloned_, false);
   EXPECT_EQ(p->spool_, false);
   EXPECT_EQ(p->use_locking_, false);
   EXPECT_EQ(p->use_bursting_, false);
   EXPECT_EQ(p->use_keepalive_, true);
   EXPECT_EQ(p->bwlimit_, 0);
   EXPECT_EQ(p->nb_bytes_, 0);
   EXPECT_EQ(p->last_tick_, 0);
   EXPECT_EQ(p->tls_established_, false);
}

TEST(bsock, bareossockettcp_copy_constructor_test)
{
/* private BAREOS_SOCKET */
   std::shared_ptr<BareosSocketTCP> p = std::make_shared<BareosSocketTCP>();

   srand(time(NULL));

   p->fd_ = rand();
   p->read_seqno = rand();
//   POOLMEM *msg; --> already initialized
//   POOLMEM *errmsg;  --> already initialized
   p->spool_fd_ = rand();
   p->src_addr = (IPADDR *)0x1111;
   p->in_msg_no = rand();
   p->out_msg_no = rand();
   p->message_length = rand();
   p->timer_start = rand();
   p->b_errno = rand();
   p->blocking_ = rand();
   p->errors = rand();
   p->suppress_error_msgs_ = true;
   p->sleep_time_after_authentication_error = rand();
   p->local_daemon_type_ = BareosDaemonType::kConsole;
   p->remote_daemon_type_ = BareosDaemonType::kDirector;

/* protected BAREOS_SOCKET: */
   p->jcr_ = (JobControlRecord *)0x2222;
//   p->pthread_mutex_t mutex_; --> MUTEX INITIALIZER
   p->who_ = (char *)0x3333;
   p->host_ = (char *)0x4444;
   p->port_ = rand();
   p->tid_ = (btimer_t*)0x5555;
   p->data_end_ = rand();
   p->FileIndex_ = rand();
   p->timed_out_ = true;
   p->terminated_ = true;
   p->cloned_ = true;
   p->spool_ = true;
   p->use_locking_ = true;
   p->use_bursting_ = true;
   p->use_keepalive_ = true;
   p->bwlimit_ = rand();
   p->nb_bytes_= rand();
   p->last_tick_ = rand();
   p->tls_established_ = true;

   /* copy p --> q */
   std::shared_ptr<BareosSocketTCP> q = std::make_shared<BareosSocketTCP>(*p);

   EXPECT_EQ(p->fd_, q->fd_);
   EXPECT_EQ(p->read_seqno, q->read_seqno);
   EXPECT_EQ(p->msg, q->msg);
   EXPECT_EQ(p->errmsg, q->errmsg);
//   POOLMEM *msg; --> already initialized
//   POOLMEM *errmsg;  --> already initialized
   EXPECT_EQ(p->spool_fd_, q->spool_fd_);
   EXPECT_EQ(p->src_addr, q->src_addr);
   EXPECT_EQ(p->in_msg_no, q->in_msg_no);
   EXPECT_EQ(p->out_msg_no, q->out_msg_no);
   EXPECT_EQ(p->message_length, q->message_length);
   EXPECT_EQ(p->timer_start, q->timer_start);
   EXPECT_EQ(p->b_errno, q->b_errno);
   EXPECT_EQ(p->blocking_, q->blocking_);
   EXPECT_EQ(p->errors, q->errors);
   EXPECT_EQ(p->suppress_error_msgs_, q->suppress_error_msgs_);
   EXPECT_EQ(p->sleep_time_after_authentication_error, q->sleep_time_after_authentication_error);
   EXPECT_EQ(p->local_daemon_type_, q->local_daemon_type_);
   EXPECT_EQ(p->remote_daemon_type_, q->remote_daemon_type_);

/* protected BAREOS_SOCKET: */
   EXPECT_EQ(p->jcr_, q->jcr_);
//   p->pthread_mutex_t mutex_; --> MUTEX INITIALIZER
   EXPECT_EQ(p->who_, q->who_);
   EXPECT_EQ(p->host_, q->host_);
   EXPECT_EQ(p->port_, q->port_);
   EXPECT_EQ(p->tid_, q->tid_);
   EXPECT_EQ(p->data_end_, q->data_end_);
   EXPECT_EQ(p->FileIndex_, q->FileIndex_);
   EXPECT_EQ(p->timed_out_, q->timed_out_);
   EXPECT_EQ(p->terminated_, q->terminated_);
   EXPECT_EQ(p->cloned_, q->cloned_);
   EXPECT_EQ(p->spool_, q->spool_);
   EXPECT_EQ(p->use_locking_, q->use_locking_);
   EXPECT_EQ(p->use_bursting_, q->use_bursting_);
   EXPECT_EQ(p->use_keepalive_, q->use_keepalive_);
   EXPECT_EQ(p->bwlimit_, q->bwlimit_);
   EXPECT_EQ(p->nb_bytes_, q->nb_bytes_);
   EXPECT_EQ(p->last_tick_, q->last_tick_);
   EXPECT_EQ(p->tls_established_, q->tls_established_);

/* prevent invalid test-adresses from being freed */
   p->src_addr = nullptr;
   p->who_ = nullptr;
   p->host_ = nullptr;
   p->msg = nullptr;
   p->errmsg = nullptr;
   p->tid_ = nullptr;
   p->jcr_ = nullptr;

   q->src_addr = nullptr;
   q->who_ = nullptr;
   q->host_ = nullptr;
   q->msg = nullptr;
   q->errmsg = nullptr;
   q->tid_ = nullptr;
   q->jcr_ = nullptr;
}
