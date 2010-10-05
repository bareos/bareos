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

#ifndef __DROPLET_CONVERTERS_H__
#define __DROPLET_CONVERTERS_H__ 1

/* PROTO droplet_converters.c */
/* src/droplet_converters.c */
dpl_method_t dpl_method(char *str);
char *dpl_method_str(dpl_method_t method);
dpl_location_constraint_t dpl_location_constraint(char *str);
char *dpl_location_constraint_str(dpl_location_constraint_t location_constraint);
dpl_canned_acl_t dpl_canned_acl(char *str);
char *dpl_canned_acl_str(dpl_canned_acl_t canned_acl);
dpl_storage_class_t dpl_storage_class(char *str);
char *dpl_storage_class_str(dpl_storage_class_t storage_class);
dpl_dict_t *dpl_parse_metadata(char *metadata);
dpl_dict_t *dpl_parse_query_params(char *query_params);
#endif
