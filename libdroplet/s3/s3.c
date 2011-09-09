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
#include "dropletp.h"
#include <droplet/s3/s3.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_backend_t
dpl_backend_s3 = 
  {
    "s3",
    /* general */
    .list_all_my_buckets = dpl_s3_list_all_my_buckets,
    .list_bucket 	= dpl_s3_list_bucket,
    .make_bucket 	= dpl_s3_make_bucket,
    .delete_bucket 	= dpl_s3_deletebucket,
    .put 		= dpl_s3_put,
    .get 		= dpl_s3_get,
    .head 		= dpl_s3_head,
    .head_all 		= dpl_s3_head_all,
    .get_metadata_from_headers = dpl_s3_get_metadata_from_headers,
    .delete 		= dpl_s3_delete,
    /* vdir */
    .namei		= dpl_s3_namei,
    .cwd		= dpl_s3_cwd,
    .opendir		= dpl_s3_opendir,
    .readdir		= dpl_s3_readdir,
    .eof		= dpl_s3_eof,
    .closedir		= dpl_s3_closedir,
    .chdir		= dpl_s3_chdir,
    .mkdir		= dpl_s3_mkdir,
    .mknod		= dpl_s3_mknod,
    .rmdir		= dpl_s3_rmdir,
    /* vfile */
    .close		= dpl_s3_close,
    .openwrite		= dpl_s3_openwrite,
    .write		= dpl_s3_write,
    .openread		= dpl_s3_openread,
    .openread_range	= dpl_s3_openread_range,
    .unlink		= dpl_s3_unlink,
    .getattr		= dpl_s3_getattr,
    .setattr		= dpl_s3_setattr,
    .fgenurl		= dpl_s3_fgenurl,
    .fcopy		= dpl_s3_fcopy,
  };
