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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_sysmd_t *
dpl_sysmd_dup(const dpl_sysmd_t *sysmd)
{
  dpl_sysmd_t *nsysmd;
  
  nsysmd = malloc(sizeof (*nsysmd));
  if (NULL == nsysmd)
    return NULL;
  
  //for now simple ocpy
  memcpy(nsysmd, sysmd, sizeof (*sysmd));

  return nsysmd;
}

void
dpl_sysmd_free(dpl_sysmd_t *sysmd)
{
  free(sysmd);
}

void
dpl_sysmd_print(dpl_sysmd_t *sysmd,
                FILE *f)
{
  int i;

  if (sysmd->mask & DPL_SYSMD_MASK_ID)
    fprintf(f, "id=%s\n", sysmd->id);

  if (sysmd->mask & DPL_SYSMD_MASK_VERSION)
    fprintf(f, "version=%s\n", sysmd->version);

  if (sysmd->mask & DPL_SYSMD_MASK_ENTERPRISE_NUMBER)
    fprintf(f, "enterprise_number=%u\n", sysmd->enterprise_number);

  if (sysmd->mask & DPL_SYSMD_MASK_PARENT_ID)
    fprintf(f, "parent_id=%s\n", sysmd->parent_id);

  if (sysmd->mask & DPL_SYSMD_MASK_FTYPE)
    fprintf(f, "ftype=%s\n", dpl_object_type_str(sysmd->ftype));

  if (sysmd->mask & DPL_SYSMD_MASK_CANNED_ACL)
    fprintf(f, "canned_acl=%s\n", dpl_canned_acl_str(sysmd->canned_acl));

  if (sysmd->mask & DPL_SYSMD_MASK_STORAGE_CLASS)
    fprintf(f, "storage_class=%s\n", dpl_storage_class_str(sysmd->storage_class));

  if (sysmd->mask & DPL_SYSMD_MASK_SIZE)
    fprintf(f, "size=%lu\n", sysmd->size);

  if (sysmd->mask & DPL_SYSMD_MASK_ATIME)
    fprintf(f, "atime=%lu\n", sysmd->atime);

  if (sysmd->mask & DPL_SYSMD_MASK_MTIME)
    fprintf(f, "mtime=%lu\n", sysmd->mtime);

  if (sysmd->mask & DPL_SYSMD_MASK_CTIME)
    fprintf(f, "ctime=%lu\n", sysmd->ctime);
  
  if (sysmd->mask & DPL_SYSMD_MASK_ETAG)
    fprintf(f, "etag=%s\n", sysmd->etag);

  if (sysmd->mask & DPL_SYSMD_MASK_LOCATION_CONSTRAINT)
    fprintf(f, "location_constraint=%s\n", dpl_location_constraint_str(sysmd->location_constraint));

  if (sysmd->mask & DPL_SYSMD_MASK_OWNER)
    fprintf(f, "owner=%s\n", sysmd->owner);

  if (sysmd->mask & DPL_SYSMD_MASK_GROUP)
    fprintf(f, "group=%s\n", sysmd->group);

  if (sysmd->mask & DPL_SYSMD_MASK_ACL)
    {
      for (i = 0;i < sysmd->n_aces;i++)
        {
          printf("ace%d: type=0x%x flag=0x%x access_mask=0x%x who=0x%x\n",
                 i, sysmd->aces[i].type, sysmd->aces[i].flag, sysmd->aces[i].access_mask,
                 sysmd->aces[i].who);
        }
    }

  if (sysmd->mask & DPL_SYSMD_MASK_PATH)
    fprintf(f, "path=%s\n", sysmd->path);
}
