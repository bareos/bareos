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
#ifndef __DROPLET_VFILE_H__
#define __DROPLET_VFILE_H__ 1

#define DPL_VFILE_FLAG_CREAT   (1u<<0)
#define DPL_VFILE_FLAG_EXCL    (1u<<1)
#define DPL_VFILE_FLAG_MD5     (1u<<2) /*!< check MD5 */
#define DPL_VFILE_FLAG_ENCRYPT (1u<<3) /*!< encrypt on the fly */

typedef struct
{
  dpl_ctx_t *ctx;

  unsigned int flags;

  dpl_conn_t *conn;

  /*
   * MD5
   */
  MD5_CTX md5_ctx;

  /*
   * encrypt
   */
  unsigned char salt[PKCS5_SALT_LEN];
  EVP_CIPHER_CTX *cipher_ctx;
  int header_done;

  /*
   * read
   */
  dpl_dict_t *headers_reply;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;

} dpl_vfile_t;

#define DPL_ENCRYPT_MAGIC "Salted__"

/* PROTO vfile.c */
/* src/vfile.c */
dpl_status_t dpl_close(dpl_vfile_t *vfile);
dpl_status_t dpl_openwrite(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_dict_t *metadata, dpl_canned_acl_t canned_acl, unsigned int data_len, dpl_vfile_t **vfilep);
dpl_status_t dpl_write(dpl_vfile_t *vfile, char *buf, unsigned int len);
dpl_status_t dpl_openread(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, dpl_buffer_func_t buffer_func, void *cb_arg, dpl_dict_t **metadatap);
dpl_status_t dpl_openread_range(dpl_ctx_t *ctx, char *locator, unsigned int flags, dpl_condition_t *condition, int start, int end, char **data_bufp, unsigned int *data_lenp, dpl_dict_t **metadatap);
dpl_status_t dpl_unlink(dpl_ctx_t *ctx, char *locator);
dpl_status_t dpl_getattr(dpl_ctx_t *ctx, char *locator, dpl_dict_t **metadatap);
dpl_status_t dpl_setattr(dpl_ctx_t *ctx, char *locator, dpl_dict_t *metadata);
dpl_status_t dpl_fgenurl(dpl_ctx_t *ctx, char *locator, time_t expires, char *buf, unsigned int len, unsigned int *lenp);
dpl_status_t dpl_fcopy(dpl_ctx_t *ctx, char *src_locator, char *dst_locator);
#endif
