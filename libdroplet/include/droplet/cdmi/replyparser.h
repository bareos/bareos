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
#ifndef __DROPLET_CDMI_REPLYPARSER_H__
#define __DROPLET_CDMI_REPLYPARSER_H__ 1

#define DPL_X_OBJECT_META_PREFIX "X-Object-Meta-"
#define DPL_X_CONTAINER_META_PREFIX "X-Container-Meta-"

/* PROTO replyparser.c */
/* src/replyparser.c */
dpl_status_t cb_cdmi_req_add_metadata(dpl_dict_var_t *var, void *cb_arg);
dpl_status_t dpl_cdmi_req_add_metadata(dpl_req_t *req, const dpl_dict_t *metadata, int append);
dpl_status_t dpl_cdmi_get_metadatum_from_value(const char *key, dpl_value_t *val, dpl_metadatum_func_t metadatum_func, void *cb_arg, dpl_dict_t *metadata, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_cdmi_get_metadatum_from_string(const char *key, const char *value, dpl_metadatum_func_t metadatum_func, void *cb_arg, dpl_dict_t *metadata, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_cdmi_get_metadatum_from_header(const char *header, const char *value, dpl_metadatum_func_t metadatum_func, void *cb_arg, dpl_dict_t *metadata, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_cdmi_get_metadata_from_headers(const dpl_dict_t *headers, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_cdmi_get_metadata_from_values(const dpl_dict_t *values, dpl_dict_t **metadatap, dpl_sysmd_t *sysmdp);
dpl_status_t dpl_cdmi_parse_list_bucket(dpl_ctx_t *ctx, const char *buf, int len, const char *prefix, dpl_vec_t *objects, dpl_vec_t *common_prefixes);
dpl_status_t dpl_cdmi_parse_json_buffer(dpl_ctx_t *ctx, const char *buf, int len, dpl_value_t **valp);
dpl_ftype_t dpl_cdmi_content_type_to_ftype(const char *str);
#endif
