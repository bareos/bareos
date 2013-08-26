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
#ifndef __DROPLET_REQ_H__
#define __DROPLET_REQ_H__ 1

/* PROTO req.c */
/* src/req.c */
void dpl_req_free(dpl_req_t *req);
dpl_req_t *dpl_req_new(dpl_ctx_t *ctx);
dpl_status_t dpl_req_set_host(dpl_req_t *req, const char *host);
dpl_status_t dpl_req_set_port(dpl_req_t *req, const char *port);
void dpl_req_set_method(dpl_req_t *req, dpl_method_t method);
dpl_status_t dpl_req_set_bucket(dpl_req_t *req, const char *bucket);
dpl_status_t dpl_req_set_src_resource_ext(dpl_req_t *req, const char *src_resource, int add_base_path);
dpl_status_t dpl_req_set_resource(dpl_req_t *req, const char *resource);
dpl_status_t dpl_req_set_subresource(dpl_req_t *req, const char *subresource);
dpl_status_t dpl_req_add_subresource(dpl_req_t *req, const char *subresource);
void dpl_req_add_behavior(dpl_req_t *req, unsigned int flags);
void dpl_req_rm_behavior(dpl_req_t *req, unsigned int flags);
void dpl_req_set_location_constraint(dpl_req_t *req, dpl_location_constraint_t location_constraint);
void dpl_req_set_canned_acl(dpl_req_t *req, dpl_canned_acl_t canned_acl);
void dpl_req_set_storage_class(dpl_req_t *req, dpl_storage_class_t storage_class);
void dpl_req_set_condition(dpl_req_t *req, const dpl_condition_t *condition);
dpl_status_t dpl_req_set_cache_control(dpl_req_t *req, const char *cache_control);
dpl_status_t dpl_req_set_content_disposition(dpl_req_t *req, const char *content_disposition);
dpl_status_t dpl_req_set_content_encoding(dpl_req_t *req, const char *content_encoding);
void dpl_req_set_data(dpl_req_t *req, const char *data_buf, u_int data_len);
dpl_status_t dpl_req_add_metadatum(dpl_req_t *req, const char *key, const char *value);
dpl_status_t dpl_req_add_metadata(dpl_req_t *req, const dpl_dict_t *metadata);
dpl_status_t dpl_req_set_content_type(dpl_req_t *req, const char *content_type);
dpl_status_t dpl_req_set_object_type(dpl_req_t *req, dpl_ftype_t object_type);
dpl_status_t dpl_req_add_range(dpl_req_t *req, int start, int end);
void dpl_req_set_expires(dpl_req_t *req, time_t expires);
dpl_status_t dpl_req_set_src_bucket(dpl_req_t *req, const char *src_bucket);
dpl_status_t dpl_req_set_src_resource(dpl_req_t *req, const char *src_resource);
dpl_status_t dpl_req_set_src_subresource(dpl_req_t *req, const char *src_subresource);
void dpl_req_set_copy_directive(dpl_req_t *req, dpl_copy_directive_t copy_directive);
void dpl_req_set_copy_source_condition(dpl_req_t *req, const dpl_condition_t *condition);
#endif
