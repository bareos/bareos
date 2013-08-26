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
#ifndef __DROPLET_SPROXYD_REPLYPARSER_H__
#define __DROPLET_SPROXYD_REPLYPARSER_H__ 1

#define DPL_SPROXYD_X_SCAL_SIZE           "X-Scal-Size"
#define DPL_SPROXYD_X_SCAL_ATIME          "X-Scal-Atime"
#define DPL_SPROXYD_X_SCAL_MTIME          "X-Scal-Mtime"
#define DPL_SPROXYD_X_SCAL_CTIME          "X-Scal-Ctime"
#define DPL_SPROXYD_X_SCAL_VERSION        "X-Scal-Version"
#define DPL_SPROXYD_X_SCAL_CRC32          "X-Scal-Crc32"

#define DPL_SPROXYD_X_SCAL_USERMD         "X-Scal-Usermd"

#define DPL_SPROXYD_X_SCAL_CMD            "X-Scal-Cmd"
#define DPL_SPROXYD_UPDATE_USERMD         "update-usermd"

#define DPL_SPROXYD_X_SCAL_REPLICA_POLICY "X-Scal-Replica-Policy"
#define DPL_SPROXYD_CONSISTENT            "consistent"

#define DPL_SPROXYD_X_SCAL_GET_USERMD     "X-Scal-Get-Usermd"

#define DPL_SPROXYD_X_SCAL_FORCE_VERSION  "X-Scal-Force-Version"

#define DPL_SPROXYD_X_SCAL_RING_STATUS    "X-Scal-Ring-Status"

#define DPL_SPROXYD_X_SCAL_COND           "X-Scal-Cond"

#define DPL_SPROXYD_YES                   "Yes"
#define DPL_SPROXYD_NO                    "No"

/* PROTO replyparser.c */
/* src/replyparser.c */
dpl_status_t dpl_sproxyd_get_metadatum_from_header(const char *header, const char *value, dpl_metadatum_func_t metadatum_func, void *cb_arg, dpl_dict_t *metadata, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_sproxyd_get_metadata_from_headers(const dpl_dict_t *headers, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
#endif
