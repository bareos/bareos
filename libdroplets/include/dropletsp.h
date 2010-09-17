/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DROPLETSP_H__
#define __DROPLETSP_H__ 1

/*
 * dependencies
 */
#include <droplets.h>
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
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>
#include <droplets_utils.h>
#include <droplets_profile.h>
#include <droplets_httpreply.h>
#include <droplets_httpreq.h>
#include <droplets_s3req.h>
#include <droplets_pricing.h>

#endif
