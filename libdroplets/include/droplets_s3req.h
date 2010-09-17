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

#ifndef __DROPLETS_S3REQ_H__
#define __DROPLETS_S3REQ_H__ 1

#define DPL_BASE64_LENGTH(len) (4 * (((len) + 2) / 3))
#define DPL_URL_LENGTH(len) ((len)*3+1)

/* PROTO droplets_s3req.c */
/* src/droplets_s3req.c */
u_int dpl_hmac_sha1(char *key_buf, u_int key_len, char *data_buf, u_int data_len, char *digest_buf);
u_int dpl_base64_encode(const unsigned char *in_buf, u_int in_len, char *out_buf);
void dpl_url_encode(char *str, char *str_ue);
dpl_status_t dpl_build_s3_signature(dpl_ctx_t *ctx, char *method, char *bucket, char *resource_ue, char *subresource_ue, dpl_dict_t *headers, char *buf, u_int len, u_int *lenp);
dpl_status_t dpl_build_s3_request(dpl_ctx_t *ctx, char *method, char *bucket, char *resource, char *subresource, dpl_dict_t *query_params, dpl_dict_t *headers, char *buf, u_int len, u_int *lenp);
#endif
