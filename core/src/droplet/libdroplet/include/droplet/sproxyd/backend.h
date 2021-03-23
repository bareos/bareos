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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SPROXYD_BACKEND_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SPROXYD_BACKEND_H_

DCL_BACKEND_GET_CAPABILITIES_FN(dpl_sproxyd_get_capabilities);
DCL_BACKEND_GET_ID_SCHEME_FN(dpl_sproxyd_get_id_scheme);
DCL_BACKEND_PUT_FN(dpl_sproxyd_put_id);
DCL_BACKEND_GET_FN(dpl_sproxyd_get_id);
DCL_BACKEND_HEAD_FN(dpl_sproxyd_head_id);
DCL_BACKEND_HEAD_RAW_FN(dpl_sproxyd_head_id_raw);
DCL_BACKEND_DELETE_FN(dpl_sproxyd_delete_id);
DCL_BACKEND_DELETE_ALL_ID_FN(dpl_sproxyd_delete_all_id);
DCL_BACKEND_COPY_FN(dpl_sproxyd_copy_id);

DCL_BACKEND_FN(dpl_sproxyd_put_internal, const char *, const char *, const char *, const dpl_option_t *, dpl_ftype_t, const dpl_condition_t *, const dpl_range_t *, const dpl_dict_t *, const dpl_sysmd_t *, const char *, unsigned int, int, char **);

extern dpl_backend_t    dpl_backend_sproxyd;

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_SPROXYD_BACKEND_H_
