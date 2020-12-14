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
#ifndef __DROPLET_PROFILE_H__
#define __DROPLET_PROFILE_H__ 1

/* PROTO profile.c */
/* src/profile.c */
struct dpl_conf_ctx *dpl_conf_new(dpl_conf_cb_func_t cb_func, void *cb_arg);
void dpl_conf_free(struct dpl_conf_ctx *ctx);
dpl_status_t dpl_conf_parse(struct dpl_conf_ctx *ctx, const char *buf, int len);
dpl_status_t dpl_conf_finish(struct dpl_conf_ctx *ctx);
dpl_status_t dpl_profile_parse(dpl_ctx_t *ctx, const char *path);
dpl_status_t dpl_profile_default(dpl_ctx_t *ctx);
dpl_status_t dpl_open_event_log(dpl_ctx_t *ctx);
void dpl_close_event_log(dpl_ctx_t *ctx);
dpl_status_t dpl_profile_post(dpl_ctx_t *ctx);
dpl_status_t dpl_profile_load(dpl_ctx_t *ctx, const char *droplet_dir, const char *profile_name);
dpl_status_t dpl_profile_set_from_dict(dpl_ctx_t *ctx, const dpl_dict_t *profile);
void dpl_profile_free(dpl_ctx_t *ctx);
#endif
