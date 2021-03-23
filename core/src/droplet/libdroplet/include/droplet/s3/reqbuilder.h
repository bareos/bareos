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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_REQBUILDER_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_REQBUILDER_H_

typedef enum
  {
    DPL_S3_REQ_COPY = (1u<<0),
  } dpl_s3_req_mask_t;

/* PROTO reqbuilder.c */
/* src/reqbuilder.c */
dpl_status_t dpl_s3_req_build(const dpl_req_t *req, dpl_s3_req_mask_t req_mask, dpl_dict_t **headersp);
dpl_status_t dpl_s3_req_gen_url(const dpl_req_t *req, dpl_dict_t *headers, char *buf, int len, unsigned int *lenp);
dpl_status_t dpl_s3_add_authorization_to_headers(const dpl_req_t *, dpl_dict_t *,
                                                 const dpl_dict_t *, struct tm *);

/* src/s3/auth/v2.c */
dpl_status_t dpl_s3_add_authorization_v2_to_headers(const dpl_req_t *, dpl_dict_t *,
                                                    const dpl_dict_t *, struct tm *);
dpl_status_t dpl_s3_get_authorization_v2_params(const dpl_req_t *, dpl_dict_t *, struct tm *);

/* src/s3/auth/v4.c */
dpl_status_t dpl_s3_add_authorization_v4_to_headers(const dpl_req_t *, dpl_dict_t *,
                                                    const dpl_dict_t *, struct tm *);
dpl_status_t dpl_s3_get_authorization_v4_params(const dpl_req_t *, dpl_dict_t *, struct tm *);

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_S3_REQBUILDER_H_
