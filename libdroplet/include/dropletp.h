/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#ifndef __DROPLETP_H__
#define __DROPLETP_H__ 1

/*
 * dependencies
 */
#include <droplet.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <poll.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/hmac.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <json.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <droplet/utils.h>
#include <droplet/profile.h>
#include <droplet/httpreply.h>
#include <droplet/pricing.h>
#include <droplet/backend.h>
#include <droplet/sbuf.h>
#include <droplet/ntinydb.h>
#include <droplet/task.h>

#endif
