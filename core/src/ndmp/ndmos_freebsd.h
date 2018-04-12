/*
 * Copyright (c) 1998,1999,2000
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description:
 *	This establishes the environment and options
 *	for the FreeBSD platform. This, combined with
 *	the O/S generic ndmos.h, are the foundation
 *	for NDMJOBLIB.
 *
 *	This file is #include'd by ndmos.h when
 *	selected by #ifdef's of NDMOS_ID.
 *
 *	Refer to ndmos.h for explanations of the
 *	macros thar are or can be #define'd here.
 */

#define NDMOS_ID_FREEBSD	NDMOS_IDENT('F','B','s','d')

#ifndef NDMOS_ID
#ifdef __FreeBSD__
#define NDMOS_ID	NDMOS_ID_FREEBSD
#endif /* __FreeBSD__ */
#endif /* !NDMOS_ID */




#if NDMOS_ID == NDMOS_ID_FREEBSD	/* probably redundant */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>

#define NDMOS_OPTION_HAVE_SIN_LEN

#define NDMOS_API_STRTOLL(P,PP,BASE) strtoq(P,PP,BASE)

/*
 * #ifndef'ed so they can be set from the Makefile command line
 */
#ifndef NDMOS_CONST_NDMJOBLIB_REVISION
#define NDMOS_CONST_NDMJOBLIB_REVISION "0"
#endif /* !NDMOS_CONST_NDMJOBLIB_REVISION */

#ifndef NDMOS_CONST_VENDOR_NAME
#define NDMOS_CONST_VENDOR_NAME "PublicDomain"
#endif /* !NDMOS_CONST_VENDOR_NAME */

#ifndef NDMOS_CONST_PRODUCT_NAME
#define NDMOS_CONST_PRODUCT_NAME "NDMJOB"
#endif /* !NDMOS_CONST_PRODUCT_NAME */

#ifndef NDMOS_CONST_PRODUCT_REVISION
#define NDMOS_CONST_PRODUCT_REVISION "0.0"
#endif /* !NDMOS_CONST_PRODUCT_REVISION */

#define NDMOS_CONST_NDMOS_REVISION "FreeBSD-04xx"

#define NDMOS_MACRO_ROBOT_AGENT_ADDITIONS \
	struct cam_device *	camdev;

#endif /* NDMOS_ID == NDMOS_ID_FREEBSD */
