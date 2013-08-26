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
#include <utime.h>
#include <pwd.h>
#include <grp.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_posix_get_metadatum_from_value(const char *key,
                                   dpl_value_t *val,
                                   dpl_metadatum_func_t metadatum_func,
                                   void *cb_arg,
                                   dpl_dict_t *metadata,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_status_t ret, ret2;
  int iret;
  char buf[256];

  if (sysmdp)
    {
      if (!strcmp(key, "atime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->atime = strtoul(dpl_sbuf_get_str(val->string), NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_ATIME;
        }
      else if (!strcmp(key, "mtime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->mtime = strtoul(dpl_sbuf_get_str(val->string), NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_MTIME;
        }
      else if (!strcmp(key, "ctime"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->ctime = strtoul(dpl_sbuf_get_str(val->string), NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_CTIME;
        }
      else if (!strcmp(key, "size"))
        {
          assert(val->type == DPL_VALUE_STRING);
          sysmdp->size = strtoul(dpl_sbuf_get_str(val->string), NULL, 0);
          sysmdp->mask |= DPL_SYSMD_MASK_SIZE;
        }
      else if (!strcmp(key, "uid"))
        {
          uid_t uid;
          struct passwd pwd, *pwdp;

          assert(val->type == DPL_VALUE_STRING);
          uid = atoi(dpl_sbuf_get_str(val->string));
          iret = getpwuid_r(uid, &pwd, buf, sizeof (buf), &pwdp);
          if (iret == -1)
            {
              perror("getpwuid");
              ret = DPL_FAILURE;
              goto end;
            }
          snprintf(sysmdp->owner, sizeof (sysmdp->owner), "%s", pwdp->pw_name);
          sysmdp->mask |= DPL_SYSMD_MASK_OWNER;
        }
      else if (!strcmp(key, "gid"))
        {
          gid_t gid;
          struct group grp, *grpp;

          assert(val->type == DPL_VALUE_STRING);
          gid = atoi(dpl_sbuf_get_str(val->string));
          iret = getgrgid_r(gid, &grp, buf, sizeof (buf), &grpp);
          if (iret == -1)
            {
              perror("getgrgid");
              ret = DPL_FAILURE;
              goto end;
            }
          snprintf(sysmdp->group, sizeof (sysmdp->group), "%s", grpp->gr_name);
          sysmdp->mask |= DPL_SYSMD_MASK_GROUP;
        }
      else if (!strcmp(key, "ino"))
        {
          assert(val->type == DPL_VALUE_STRING);
          snprintf(sysmdp->id, sizeof (sysmdp->id), "%s",
                   dpl_sbuf_get_str(val->string));
          sysmdp->mask |= DPL_SYSMD_MASK_ID;
        }
    }

  if (!strcmp(key, "xattr"))
    {
      //this is the metadata object
      if (DPL_VALUE_SUBDICT != val->type)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      if (metadata)
        {
          if (metadatum_func)
            {
              ret2 = metadatum_func(cb_arg, key, val);
              if (DPL_SUCCESS != ret2)
                {
                  ret = ret2;
                  goto end;
                }
            }

          //add md into metadata
          ret2 = dpl_dict_add_value(metadata, key, val, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }
  
  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

struct metadata_conven
{
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
};

static dpl_status_t
cb_values_iterate(dpl_dict_var_t *var,
                  void *cb_arg)
{
  struct metadata_conven *mc = (struct metadata_conven *) cb_arg;
  
  return dpl_posix_get_metadatum_from_value(var->key,
                                            var->val,
                                            NULL,
                                            NULL,
                                            mc->metadata,
                                            mc->sysmdp);
}

/** 
 * get metadata from values
 * 
 * @param values 
 * @param metadatap 
 * @param sysmdp 
 * 
 * @return 
 */
dpl_status_t
dpl_posix_get_metadata_from_values(const dpl_dict_t *values,
                                   dpl_dict_t **metadatap,
                                   dpl_sysmd_t *sysmdp)
{
  dpl_dict_t *metadata = NULL;
  dpl_status_t ret, ret2;
  struct metadata_conven mc;

  if (metadatap)
    {
      metadata = dpl_dict_new(13);
      if (NULL == metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  memset(&mc, 0, sizeof (mc));
  mc.metadata = metadata;
  mc.sysmdp = sysmdp;

  if (sysmdp)
    sysmdp->mask = 0;

  ret2 = dpl_dict_iterate(values, cb_values_iterate, &mc);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
}

