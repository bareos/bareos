/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CONVERTERS_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CONVERTERS_H_

/* PROTO converters.c */
/* src/converters.c */
dpl_method_t dpl_method(char* str);
char* dpl_method_str(dpl_method_t method);
dpl_location_constraint_t dpl_location_constraint(char* str);
char* dpl_location_constraint_str(
    dpl_location_constraint_t location_constraint);
dpl_canned_acl_t dpl_canned_acl(char* str);
char* dpl_canned_acl_str(dpl_canned_acl_t canned_acl);
dpl_storage_class_t dpl_storage_class(char* str);
char* dpl_storage_class_str(dpl_storage_class_t storage_class);
dpl_copy_directive_t dpl_copy_directive(char* str);
char* dpl_copy_directive_str(dpl_copy_directive_t copy_directive);
dpl_ftype_t dpl_object_type(char* str);
char* dpl_object_type_str(dpl_ftype_t object_type);
dpl_dict_t* dpl_parse_metadata(char* metadata);
dpl_dict_t* dpl_parse_query_params(char* query_params);
dpl_status_t dpl_parse_condition(const char* str, dpl_condition_t* condp);
dpl_status_t dpl_parse_option(const char* str, dpl_option_t* optp);
#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_CONVERTERS_H_
