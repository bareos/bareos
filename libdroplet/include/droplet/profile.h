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

#ifndef __DROPLET_PROFILE_H__
#define __DROPLET_PROFILE_H__ 1

/* PROTO profile.c */
/* src/profile.c */
struct dpl_conf_ctx *dpl_conf_new(dpl_conf_cb_func_t cb_func, void *cb_arg);
void dpl_conf_free(struct dpl_conf_ctx *ctx);
dpl_status_t dpl_conf_parse(struct dpl_conf_ctx *ctx, char *buf, int len);
dpl_status_t dpl_conf_finish(struct dpl_conf_ctx *ctx);
dpl_status_t dpl_profile_parse(dpl_ctx_t *ctx, char *path);
dpl_status_t dpl_profile_default(dpl_ctx_t *ctx);
dpl_status_t dpl_open_event_log(dpl_ctx_t *ctx);
void dpl_close_event_log(dpl_ctx_t *ctx);
dpl_status_t dpl_profile_post(dpl_ctx_t *ctx);
dpl_status_t dpl_profile_load(dpl_ctx_t *ctx, char *droplet_dir, char *profile_name);
void dpl_profile_free(dpl_ctx_t *ctx);
#endif
