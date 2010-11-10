/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
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

#ifndef __DROPLET_REQBUILDER_H__
#define __DROPLET_REQBUILDER_H__ 1

/* PROTO reqbuilder.c */
/* src/reqbuilder.c */
void dpl_req_free(dpl_req_t *req);
dpl_req_t *dpl_req_new(dpl_ctx_t *ctx);
void dpl_req_set_method(dpl_req_t *req, dpl_method_t method);
dpl_status_t dpl_req_set_bucket(dpl_req_t *req, char *bucket);
dpl_status_t dpl_req_set_resource(dpl_req_t *req, char *resource);
dpl_status_t dpl_req_set_subresource(dpl_req_t *req, char *subresource);
void dpl_req_add_behavior(dpl_req_t *req, u_int flags);
void dpl_req_rm_behavior(dpl_req_t *req, u_int flags);
void dpl_req_set_location_constraint(dpl_req_t *req, dpl_location_constraint_t location_constraint);
void dpl_req_set_canned_acl(dpl_req_t *req, dpl_canned_acl_t canned_acl);
void dpl_req_set_storage_class(dpl_req_t *req, dpl_storage_class_t storage_class);
void dpl_req_set_condition(dpl_req_t *req, dpl_condition_t *condition);
dpl_status_t dpl_req_set_cache_control(dpl_req_t *req, char *cache_control);
dpl_status_t dpl_req_set_content_disposition(dpl_req_t *req, char *content_disposition);
dpl_status_t dpl_req_set_content_encoding(dpl_req_t *req, char *content_encoding);
void dpl_req_set_chunk(dpl_req_t *req, dpl_chunk_t *chunk);
dpl_status_t dpl_req_add_metadatum(dpl_req_t *req, char *key, char *value);
dpl_status_t dpl_req_add_metadata(dpl_req_t *req, dpl_dict_t *metadata);
dpl_status_t dpl_req_set_content_type(dpl_req_t *req, char *content_type);
dpl_status_t dpl_req_add_range(dpl_req_t *req, int start, int end);
void dpl_req_set_expires(dpl_req_t *req, time_t expires);
dpl_status_t dpl_req_set_src_bucket(dpl_req_t *req, char *src_bucket);
dpl_status_t dpl_req_set_src_resource(dpl_req_t *req, char *src_resource);
dpl_status_t dpl_req_set_src_subresource(dpl_req_t *req, char *src_subresource);
void dpl_req_set_metadata_directive(dpl_req_t *req, dpl_metadata_directive_t metadata_directive);
void dpl_req_set_copy_source_condition(dpl_req_t *req, dpl_condition_t *condition);
dpl_status_t dpl_req_build(dpl_req_t *req, dpl_dict_t **headersp);
dpl_status_t dpl_req_gen_http_request(dpl_req_t *req, dpl_dict_t *headers, dpl_dict_t *query_params, char *buf, int len, u_int *lenp);
dpl_status_t dpl_req_gen_url(dpl_req_t *req, dpl_dict_t *headers, char *buf, int len, u_int *lenp);
#endif
