/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
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

#ifndef __DROPLETS_REQUEST_H__
#define __DROPLETS_REQUEST_H__ 1

typedef enum
  {
    DPL_LOCATION_CONSTRAINT_US_STANDARD,
    DPL_LOCATION_CONSTRAINT_EU,
    DPL_LOCATION_CONSTRAINT_US_WEST_1,
    DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1
  } dpl_location_constraint_t;
#define DPL_N_LOCATION_CONSTRAINT (DPL_LOCATION_CONSTRAINT_AP_SOUTHEAST_1+1)

typedef enum
  {
    DPL_CANNED_ACL_PRIVATE,
    DPL_CANNED_ACL_PUBLIC_READ,
    DPL_CANNED_ACL_PUBLIC_READ_WRITE,
    DPL_CANNED_ACL_AUTHENTICATED_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_READ,
    DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL,
  } dpl_canned_acl_t;
#define DPL_N_CANNED_ACL (DPL_CANNED_ACL_BUCKET_OWNER_FULL_CONTROL+1)

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

typedef struct
{
#define DPL_CONDITION_IF_MODIFIED_SINCE   (1u<<0)
#define DPL_CONDITION_IF_UNMODIFIED_SINCE (1u<<1)
#define DPL_CONDITION_IF_MATCH            (1u<<2)
#define DPL_CONDITION_IF_NONE_MATCH       (1u<<3)
  u_int mask;
  time_t time;
  char *etag;
} dpl_condition_t;

/* PROTO droplets_listallmybuckets.c droplets_listbucket.c droplets_put.c droplets_get.c droplets_delete.c droplets_makebucket.c droplets_head.c */
/* src/droplets_listallmybuckets.c */
void dpl_bucket_free(dpl_bucket_t *bucket);
void dpl_vec_buckets_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_all_my_buckets(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *vec);
dpl_status_t dpl_list_all_my_buckets(dpl_ctx_t *ctx, dpl_vec_t **vecp);
/* src/droplets_listbucket.c */
void dpl_object_free(dpl_object_t *object);
void dpl_vec_objects_free(dpl_vec_t *vec);
void dpl_common_prefix_free(dpl_common_prefix_t *common_prefix);
void dpl_vec_common_prefixes_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_bucket(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *objects, dpl_vec_t *common_prefixes);
dpl_status_t dpl_list_bucket(dpl_ctx_t *ctx, char *bucket, char *prefix, char *delimiter, dpl_vec_t **objectsp, dpl_vec_t **common_prefixesp);
/* src/droplets_put.c */
dpl_canned_acl_t dpl_canned_acl(char *str);
char *dpl_canned_acl_str(dpl_canned_acl_t canned_acl);
dpl_dict_t *dpl_parse_metadata(char *metadata);
dpl_status_t dpl_add_metadata_to_headers(dpl_dict_t *metadata, dpl_dict_t *headers);
dpl_status_t dpl_put(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, char *data_buf, u_int data_len);
dpl_status_t dpl_put_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, u_int data_len, dpl_conn_t **connp);
/* src/droplets_get.c */
dpl_status_t dpl_get_metadata_from_headers(dpl_dict_t *headers, dpl_dict_t *metadata);
dpl_status_t dpl_add_condition_to_headers(dpl_condition_t *condition, dpl_dict_t *headers);
dpl_status_t dpl_get(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, char **data_bufp, u_int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_get_buffered(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
/* src/droplets_delete.c */
dpl_status_t dpl_delete(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource);
/* src/droplets_makebucket.c */
dpl_location_constraint_t dpl_location_constraint(char *str);
char *dpl_location_constraint_str(dpl_location_constraint_t location_constraint);
dpl_status_t dpl_make_bucket(dpl_ctx_t *ctx, char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
/* src/droplets_head.c */
dpl_status_t dpl_head(dpl_ctx_t *ctx, char *bucket, char *resource, char *subresource, dpl_condition_t *condition, dpl_dict_t **metadatap);
#endif
