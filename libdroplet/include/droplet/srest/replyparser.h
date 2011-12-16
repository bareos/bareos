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
#ifndef __DROPLET_SREST_REPLYPARSER_H__
#define __DROPLET_SREST_REPLYPARSER_H__ 1

#define SCAL_SREST_X_SCALITY_ID            "x-scality-id"
#define SCAL_SREST_X_SCALITY_WRITE_CACHE   "x-scality-write-cache"
#define SCAL_SREST_X_SCALITY_WRITE_QUEUE   "x-scality-write-queue"
#define SCAL_SREST_X_SCALITY_STORED        "x-scality-stored"
#define SCAL_SREST_X_SCALITY_STORED_STATUS "x-scality-stored-status"
#define SCAL_SREST_X_SCALITY_SIZE          "x-scality-size"
#define SCAL_SREST_X_SCALITY_ATIME         "x-scality-atime"
#define SCAL_SREST_X_SCALITY_MTIME         "x-scality-mtime"
#define SCAL_SREST_X_SCALITY_CTIME         "x-scality-ctime"
#define SCAL_SREST_X_SCALITY_VERSION       "x-scality-version"
#define SCAL_SREST_X_SCALITY_CRC32         "x-scality-crc32"
#define SCAL_SREST_X_SCALITY_DELETED       "x-scality-deleted"

#define SCAL_SREST_YES                     "Yes"
#define SCAL_SREST_NO                      "No"

/* PROTO replyparser.c */
/* src/replyparser.c */
dpl_status_t dpl_srest_get_metadata_from_headers(const dpl_dict_t *headers, dpl_dict_t *metadata);
dpl_status_t dpl_srest_parse_list_bucket(const dpl_ctx_t *ctx, const char *buf, int len, const char *prefix, dpl_vec_t *objects, dpl_vec_t *common_prefixes);
dpl_status_t dpl_srest_get_metadata_from_json_metadata(const dpl_dict_t *json_metadata, dpl_dict_t *metadata);
dpl_status_t dpl_srest_parse_metadata(const dpl_ctx_t *ctx, const char *buf, int len, dpl_dict_t *metadata);
#endif
