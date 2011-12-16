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
#ifndef __DROPLET_CDMI_REQBUILDER_H__
#define __DROPLET_CDMI_REQBUILDER_H__ 1

#define DPL_CDMI_CONTENT_TYPE_CONTAINER  "application/cdmi-container"
#define DPL_CDMI_CONTENT_TYPE_OBJECT     "application/cdmi-object"
#define DPL_CDMI_CONTENT_TYPE_CAPABILITY "application/cdmi-capability"
#define DPL_CDMI_CONTENT_TYPE_DOMAIN     "application/cdmi-domain"
#define DPL_CDMI_CONTENT_TYPE_ANY        "application/*"

/* PROTO reqbuilder.c */
/* src/reqbuilder.c */
dpl_status_t dpl_cdmi_req_build(const dpl_req_t *req, dpl_dict_t **headersp, char **body_strp, int *lenp);
dpl_status_t dpl_cdmi_req_gen_url(const dpl_req_t *req, dpl_dict_t *headers, char *buf, int len, unsigned int *lenp);
#endif
