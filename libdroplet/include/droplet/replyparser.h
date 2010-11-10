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

#ifndef __DROPLET_REPLYPARSER_H__
#define __DROPLET_REPLYPARSER_H__ 1

/* PROTO replyparser.c */
/* src/replyparser.c */
dpl_status_t dpl_get_metadata_from_headers(dpl_dict_t *headers, dpl_dict_t *metadata);
void dpl_bucket_free(dpl_bucket_t *bucket);
void dpl_vec_buckets_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_all_my_buckets(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *vec);
void dpl_object_free(dpl_object_t *object);
void dpl_vec_objects_free(dpl_vec_t *vec);
void dpl_common_prefix_free(dpl_common_prefix_t *common_prefix);
void dpl_vec_common_prefixes_free(dpl_vec_t *vec);
dpl_status_t dpl_parse_list_bucket(dpl_ctx_t *ctx, char *buf, int len, dpl_vec_t *objects, dpl_vec_t *common_prefixes);
#endif
