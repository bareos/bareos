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

#ifndef __DROPLET_REQUEST_H__
#define __DROPLET_REQUEST_H__ 1

typedef struct
{
  char *name;
  time_t creation_time;
} dpl_bucket_t;

typedef struct
{
  char *key;
  time_t last_modified;
  size_t size;
  char *etags;
} dpl_object_t;

typedef struct
{
  char *prefix;
} dpl_common_prefix_t;

/* PROTO droplet_listallmybuckets.c droplet_listbucket.c droplet_put.c droplet_get.c droplet_delete.c droplet_makebucket.c droplet_head.c droplet_deletebucket.c */
/* src/droplet_listallmybuckets.c */
void dpl_bucket_free(dpl_bucket_t *bucket);
void dpl_vec_buckets_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_all_my_buckets(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *vec);
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t *ctx, dpl_vec_t **vecp);
/* src/droplet_listbucket.c */
void dpl_object_free(dpl_object_t *object);
void dpl_vec_objects_free(dpl_vec_t *vec);
void dpl_common_prefix_free(dpl_common_prefix_t *common_prefix);
void dpl_vec_common_prefixes_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_bucket(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *objects, dpl_vec_t *common_prefixes);
dpl_status_t dpl_list_bucket(dpl_ctx_t *ctx, char *bucket, char *prefix, char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
/* src/droplet_put.c */
dpl_status_t dpl_put(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, u_int data_len);
dpl_status_t dpl_put_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, u_int data_len, dpl_conn_t **connp);
/* src/droplet_get.c */
dpl_status_t dpl_get(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, char **data_bufp, u_int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
/* src/droplet_delete.c */
dpl_status_t dpl_delete(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource);
/* src/droplet_makebucket.c */
dpl_status_t dpl_make_bucket(dpl_ctx_t *ctx, char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
/* src/droplet_head.c */
dpl_status_t dpl_head(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
/* src/droplet_deletebucket.c */
dpl_status_t dpl_deletebucket(dpl_ctx_t *ctx, char *bucket);
#endif
