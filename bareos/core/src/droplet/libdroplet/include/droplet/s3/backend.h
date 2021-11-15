/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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

#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_BACKEND_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_BACKEND_H_

DCL_BACKEND_GET_CAPABILITIES_FN(dpl_s3_get_capabilities);
DCL_BACKEND_LIST_ALL_MY_BUCKETS_FN(dpl_s3_list_all_my_buckets);
DCL_BACKEND_LIST_BUCKET_FN(dpl_s3_list_bucket);
DCL_BACKEND_LIST_BUCKET_ATTRS_FN(dpl_s3_list_bucket_attrs);
DCL_BACKEND_MAKE_BUCKET_FN(dpl_s3_make_bucket);
DCL_BACKEND_DELETE_BUCKET_FN(dpl_s3_delete_bucket);
DCL_BACKEND_PUT_FN(dpl_s3_put);
DCL_BACKEND_GET_FN(dpl_s3_get);
DCL_BACKEND_HEAD_FN(dpl_s3_head);
DCL_BACKEND_HEAD_RAW_FN(dpl_s3_head_raw);
DCL_BACKEND_DELETE_FN(dpl_s3_delete);
DCL_BACKEND_DELETE_ALL_FN(dpl_s3_delete_all);
DCL_BACKEND_GENURL_FN(dpl_s3_genurl);
DCL_BACKEND_COPY_FN(dpl_s3_copy);
DCL_BACKEND_STREAM_RESUME_FN(dpl_s3_stream_resume);
DCL_BACKEND_STREAM_GET_FN(dpl_s3_stream_get);
DCL_BACKEND_STREAM_PUT_FN(dpl_s3_stream_put);
DCL_BACKEND_STREAM_FLUSH_FN(dpl_s3_stream_flush);

DCL_BACKEND_STREAM_RESUME_FN(dpl_s3_stream_resume);
DCL_BACKEND_STREAM_GETMD_FN(dpl_s3_stream_getmd);
DCL_BACKEND_STREAM_GET_FN(dpl_s3_stream_get);
DCL_BACKEND_STREAM_PUTMD_FN(dpl_s3_stream_putmd);
DCL_BACKEND_STREAM_PUT_FN(dpl_s3_stream_put);
DCL_BACKEND_STREAM_FLUSH_FN(dpl_s3_stream_flush);

extern dpl_backend_t dpl_backend_s3;

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_BACKEND_H_
