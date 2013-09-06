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
#include "dropletp.h"
#include <json.h>
#include <droplet/swift/reqbuilder.h>
#include <droplet/swift/replyparser.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/*
  ret2 = dpl_swift_parse_list_bucket(ctx, data_buf, data_len, prefix, objects, common_prefixes);
*/


dpl_status_t
dpl_swift_parse_list_bucket(dpl_ctx_t *ctx,
                           const char *buf,
                           int len,
                           const char *prefix,
                           dpl_vec_t *objects,
                           dpl_vec_t *common_prefixes)
{
  int ret, ret2;
  int n_children, i;
  dpl_common_prefix_t *common_prefix = NULL;
  dpl_object_t *object = NULL;

  if (buf != NULL)
    {
      ret = write(1, "TEST LIST: \n\t", strlen("TEST LIST: \n\t"));
      ret = write(1, buf, len);
    }
  else
    write(1, "no data\n", strlen("no data\n"));

  ret = DPL_SUCCESS;

 end:

  if (NULL != common_prefix)
    dpl_common_prefix_free(common_prefix);

  if (NULL != object)
    dpl_object_free(object);
  return ret;
}
