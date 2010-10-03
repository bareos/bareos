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

#ifndef __DROPLET_REQDO_H__
#define __DROPLET_REQDO_H__ 1

/* PROTO droplet_reqdo.c */
/* src/droplet_reqdo.c */
dpl_dict_t *dpl_parse_metadata(char *metadata);
dpl_status_t dpl_add_metadata_to_headers(dpl_dict_t *metadata, dpl_dict_t *headers);
dpl_status_t dpl_get_metadata_from_headers(dpl_dict_t *headers, dpl_dict_t *metadata);
dpl_location_constraint_t dpl_location_constraint(char *str);
char *dpl_location_constraint_str(dpl_location_constraint_t location_constraint);
dpl_canned_acl_t dpl_canned_acl(char *str);
char *dpl_canned_acl_str(dpl_canned_acl_t canned_acl);
dpl_status_t dpl_add_condition_to_headers(dpl_condition_t *condition, dpl_dict_t *headers);
dpl_status_t dpl_req_do(dpl_req_t *req);
#endif
