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
#include <droplet/posix/posix.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <linux/xattr.h>
#include <attr/xattr.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
cb_posix_setattr(dpl_dict_var_t *var,
                 void *cb_arg)
{
  char *path = (char *) cb_arg;
  int iret;
  char buf[256];

  assert(var->val->type == DPL_VALUE_STRING);

  snprintf(buf, sizeof (buf), "%s%s", DPL_POSIX_XATTR_PREFIX, var->key);
 
  iret = lsetxattr(path, buf, dpl_sbuf_get_str(var->val->string),
                   strlen(dpl_sbuf_get_str(var->val->string)), 0);
  if (-1 == iret)
    {
      perror("lsetxattr");
      return DPL_FAILURE;
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_posix_setattr(const char *path,
                  const dpl_dict_t *metadata,
                  const dpl_sysmd_t *sysmd)
{
  dpl_status_t ret, ret2;
  int iret;

  if (sysmd)
    {
      switch (sysmd->mask)
        {
        case DPL_SYSMD_MASK_SIZE:
          iret = truncate(path, sysmd->size);
          if (-1 == iret)
            {
              perror("truncate");
              ret = DPL_FAILURE;
              goto end;
            }
          break ;
        case DPL_SYSMD_MASK_CANNED_ACL:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_STORAGE_CLASS:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ATIME:
        case DPL_SYSMD_MASK_MTIME:
          {
            struct utimbuf times;
            
            times.actime = sysmd->atime;
            times.modtime = sysmd->mtime;
            iret = utime(path, &times);
            if (-1 == iret)
              {
                perror("utime");
                ret = DPL_FAILURE;
                goto end;
              }
          }
          break ;
        case DPL_SYSMD_MASK_CTIME:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ETAG:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_LOCATION_CONSTRAINT:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_OWNER:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_GROUP:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ACL:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ID:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_PARENT_ID:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_FTYPE:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_ENTERPRISE_NUMBER:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_PATH:
          ret = DPL_ENOTSUPP;
          goto end;
        case DPL_SYSMD_MASK_VERSION:
          ret = DPL_ENOTSUPP;
          goto end;
        }
    }

  if (metadata)
    {
      ret2 = dpl_dict_iterate(metadata, cb_posix_setattr, (char *) path);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
     
  ret = DPL_SUCCESS;
  
 end:

  return ret;
}
