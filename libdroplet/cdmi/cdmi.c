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
#include <droplet/cdmi/cdmi.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_backend_t
dpl_backend_cdmi = 
  {
    "cdmi",
    .get_capabilities   = dpl_cdmi_get_capabilities,
    .list_bucket 	= dpl_cdmi_list_bucket,
    .post 		= dpl_cdmi_post,
    .post_buffered      = dpl_cdmi_post_buffered,
    .put 		= dpl_cdmi_put,
    .put_buffered       = dpl_cdmi_put_buffered,
    .get 		= dpl_cdmi_get,
    .get_buffered       = dpl_cdmi_get_buffered,
    .head 		= dpl_cdmi_head,
    .head_raw 		= dpl_cdmi_head_raw,
    .deletef 		= dpl_cdmi_delete,
    .get_id_scheme      = dpl_cdmi_get_id_scheme,
    .post_id 		= dpl_cdmi_post_id,
    .post_id_buffered   = dpl_cdmi_post_id_buffered,
    .put_id 		= dpl_cdmi_put_id,
    .put_id_buffered    = dpl_cdmi_put_id_buffered,
    .get_id 		= dpl_cdmi_get_id,
    .get_id_buffered    = dpl_cdmi_get_id_buffered,
    .head_id 		= dpl_cdmi_head_id,
    .head_id_raw 	= dpl_cdmi_head_id_raw,
    .delete_id 		= dpl_cdmi_delete_id,
    .copy               = dpl_cdmi_copy,
    .copy_id            = dpl_cdmi_copy_id,
  };
