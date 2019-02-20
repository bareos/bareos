/*
 * Copyright (c) 1998,2001
 *      Traakan, Inc., Los Altos, CA
 *      All rights reserved.
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
 *      This contains the O/S (Operating System) specific
 *      portions of NDMJOBLIB for the Solaris platform.
 *
 *      This file is #include'd by ndmos.c when
 *      selected by #ifdef's of NDMOS_ID.
 *
 *      There are four major portions:
 *      1) Misc support routines: password check, local info, etc
 *      2) Non-blocking I/O support routines
 *      3) Tape interfacs ndmos_tape_xxx()
 *      4) OS Specific NDMP request dispatcher which intercepts
 *         requests implemented here, such as SCSI operations
 *         and system configuration queries.
 */


/*
 * #include "ndmagents.h" already done in ndmos.c
 * Additional #include's, not needed in ndmos_xxx.h, yet needed here.
 */
#include <sys/utsname.h>


/*
 * Select common code fragments from ndmos_common.c
 */
#define NDMOS_COMMON_SYNC_CONFIG_INFO /* from config file (ndmjob.conf) */
#define NDMOS_COMMON_OK_NAME_PASSWORD
#define NDMOS_COMMON_MD5
#define NDMOS_COMMON_NONBLOCKING_IO_SUPPORT
#define NDMOS_COMMON_TAPE_INTERFACE   /* uses tape simulator */
#define NDMOS_COMMON_SCSI_INTERFACE   /* stub-out */
#define NDMOS_COMMON_DISPATCH_REQUEST /* no-op */


#include "ndmos_common.c"
