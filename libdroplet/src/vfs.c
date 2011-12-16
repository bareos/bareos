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
#include "droplet/cdmi/cdmi.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
dpl_vdir_lookup(dpl_ctx_t *ctx,
                const char *bucket,
                dpl_ino_t parent_ino,
                const char *obj_name,
                dpl_ino_t *obj_inop,
                dpl_ftype_t *obj_typep)
{
  int ret, ret2;
  dpl_vec_t *files = NULL;
  dpl_vec_t *directories = NULL;
  int i;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  int delim_len = strlen(ctx->delim);
  int obj_name_len = strlen(obj_name);

  memset(&obj_ino, 0, sizeof (obj_ino));

  DPL_TRACE(ctx, DPL_TRACE_VFS, "lookup bucket=%s parent_ino=%s obj_name=%s", bucket, parent_ino.key, obj_name);

  if (!strcmp(obj_name, "."))
    {
      if (NULL != obj_inop)
        *obj_inop = parent_ino;

      if (NULL != obj_typep)
        *obj_typep = DPL_FTYPE_DIR;

      ret = DPL_SUCCESS;
      goto end;
    }
  else if (!strcmp(obj_name, ".."))
    {
      char *p, *p2;

      if (!strcmp(parent_ino.key, ""))
        {
          //silent success for root dir
          if (NULL != obj_inop)
            *obj_inop = DPL_ROOT_INO;

          if (NULL != obj_typep)
            *obj_typep = DPL_FTYPE_DIR;

          ret = DPL_SUCCESS;
          goto end;
        }

      obj_ino = parent_ino;

      p = dpl_strrstr(obj_ino.key, ctx->delim);
      if (NULL == p)
        {
          fprintf(stderr, "parent key shall contain delim %s\n", ctx->delim);
          ret = DPL_FAILURE;
          goto end;
        }

      p -= delim_len;

      for (p2 = p;p2 > obj_ino.key;p2--)
        {
          if (!strncmp(p2, ctx->delim, delim_len))
            {
              DPRINTF("found delim\n");

              p2 += delim_len;
              break ;
            }
        }

      *p2 = 0;

      if (NULL != obj_inop)
        *obj_inop = obj_ino;

      if (NULL != obj_typep)
        *obj_typep = DPL_FTYPE_DIR;

      ret = DPL_SUCCESS;
      goto end;
    }

  //AWS do not like "" as a prefix
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(parent_ino.key, "") ? NULL : parent_ino.key, ctx->delim, &files, &directories);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "list_bucket failed %s:%s", bucket, parent_ino.key);
      ret = DPL_FAILURE;
      goto end;
    }

  for (i = 0;i < files->n_items;i++)
    {
      dpl_object_t *obj = (dpl_object_t *) files->array[i];
      int key_len;
      char *p;

      p = dpl_strrstr(obj->key, ctx->delim);
      if (NULL != p)
        p += delim_len;
      else
        p = obj->key;

      DPRINTF("cmp obj_key=%s obj_name=%s\n", p, obj_name);

      if (!strcmp(p, obj_name))
        {
          DPRINTF("ok\n");

          key_len = strlen(obj->key);
          if (key_len >= DPL_MAXNAMLEN)
            {
              DPLERR(0, "key is too long");
              ret = DPL_FAILURE;
              goto end;
            }
          memcpy(obj_ino.key, obj->key, key_len);
          obj_ino.key[key_len] = 0;
          if (key_len >= delim_len && !strcmp(obj->key + key_len - delim_len, ctx->delim))
            obj_type = DPL_FTYPE_DIR;
          else
            obj_type = DPL_FTYPE_REG;

          if (NULL != obj_inop)
            *obj_inop = obj_ino;

          if (NULL != obj_typep)
            *obj_typep = obj_type;

          ret = DPL_SUCCESS;
          goto end;
        }
    }

  for (i = 0;i < directories->n_items;i++)
    {
      dpl_common_prefix_t *prefix = (dpl_common_prefix_t *) directories->array[i];
      int key_len;
      char *p, *p2;

      p = dpl_strrstr(prefix->prefix, ctx->delim);
      if (NULL == p)
        {
          fprintf(stderr, "prefix %s shall contain delim %s\n", prefix->prefix, ctx->delim);
          continue ;
        }

      DPRINTF("p='%s'\n", p);

      p -= delim_len;

      for (p2 = p;p2 > prefix->prefix;p2--)
        {
          DPRINTF("p2='%s'\n", p2);

          if (!strncmp(p2, ctx->delim, delim_len))
            {
              DPRINTF("found delim\n");

              p2 += delim_len;
              break ;
            }
        }

      key_len = p - p2 + 1;

      DPRINTF("cmp (prefix=%s) prefix=%.*s obj_name=%s\n", prefix->prefix, key_len, p2, obj_name);

      if (key_len == obj_name_len && !strncmp(p2, obj_name, obj_name_len))
        {
          DPRINTF("ok\n");

          key_len = strlen(prefix->prefix);
          if (key_len >= DPL_MAXNAMLEN)
            {
              DPLERR(0, "key is too long");
              ret = DPL_FAILURE;
              goto end;
            }
          memcpy(obj_ino.key, prefix->prefix, key_len);
          obj_ino.key[key_len] = 0;
          obj_type = DPL_FTYPE_DIR;

          if (NULL != obj_inop)
            *obj_inop = obj_ino;

          if (NULL != obj_typep)
            *obj_typep = obj_type;

          ret = DPL_SUCCESS;
          goto end;
        }
    }

  ret = DPL_ENOENT;

 end:

  if (NULL != files)
    dpl_vec_objects_free(files);

  if (NULL != directories)
    dpl_vec_common_prefixes_free(directories);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
dpl_vdir_mkgen(dpl_ctx_t *ctx,
               const char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name,
               dpl_ftype_t object_type,
               const char *delim)
{
  int ret, ret2;
  char resource[DPL_MAXPATHLEN];
  dpl_sysmd_t sysmd;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "mkdir bucket=%s parent_ino=%s name=%s", bucket, parent_ino.key, obj_name);

  snprintf(resource, sizeof (resource), "%s%s%s", parent_ino.key, obj_name, delim);

  memset(&sysmd, 0, sizeof (sysmd));
  sysmd.mask = DPL_SYSMD_MASK_CANNED_ACL;
  sysmd.canned_acl = DPL_CANNED_ACL_PRIVATE;

  ret2 = dpl_put(ctx, bucket, resource, NULL, object_type, NULL, &sysmd, NULL, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
dpl_vdir_mkdir(dpl_ctx_t *ctx,
               const char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name)
{
  return dpl_vdir_mkgen(ctx, bucket, parent_ino, obj_name, DPL_FTYPE_DIR, ctx->delim);
}


static dpl_status_t
dpl_vdir_mknod(dpl_ctx_t *ctx,
               const char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name)
{
  return dpl_vdir_mkgen(ctx, bucket, parent_ino, obj_name, DPL_FTYPE_REG, "");
}

static dpl_status_t
dpl_vdir_opendir(dpl_ctx_t *ctx,
                 const char *bucket,
                 dpl_ino_t ino,
                 void **dir_hdlp)
{
  dpl_dir_t *dir;
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "opendir bucket=%s ino=%s", bucket, ino.key);

  dir = malloc(sizeof (*dir));
  if (NULL == dir)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memset(dir, 0, sizeof (*dir));

  dir->ctx = ctx;

  dir->ino = ino;

  //AWS prefers NULL for listing the root dir
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(ino.key, "") ? NULL : ino.key, ctx->delim, &dir->files, &dir->directories);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "list_bucket failed %s:%s", bucket, ino.key);
      ret = DPL_FAILURE;
      goto end;
    }

  //printf("%s:%s n_files=%d n_dirs=%d\n", bucket, ino.key, dir->files->n_items, dir->directories->n_items);

  if (NULL != dir_hdlp)
    *dir_hdlp = dir;

  DPL_TRACE(dir->ctx, DPL_TRACE_VFS, "dir_hdl=%p", dir);

  ret = DPL_SUCCESS;

 end:

  if (DPL_SUCCESS != ret)
    {
      if (NULL != dir->files)
        dpl_vec_objects_free(dir->files);

      if (NULL != dir->directories)
        dpl_vec_common_prefixes_free(dir->directories);

      free(dir);
    }

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
dpl_vdir_readdir(void *dir_hdl,
                 dpl_dirent_t *dirent)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;
  char *name;
  int name_len;
  int key_len;
  int delim_len = strlen(dir->ctx->delim);

  DPL_TRACE(dir->ctx, DPL_TRACE_VFS, "readdir dir_hdl=%p files_cursor=%d directories_cursor=%d", dir_hdl, dir->files_cursor, dir->directories_cursor);

  memset(dirent, 0, sizeof (*dirent));

  if (dir->files_cursor >= dir->files->n_items)
    {
      if (dir->directories_cursor >= dir->directories->n_items)
        {
          DPLERR(0, "beyond cursors");
          return DPL_ENOENT;
        }
      else
        {
          dpl_common_prefix_t *prefix;

          prefix = (dpl_common_prefix_t *) dir->directories->array[dir->directories_cursor];

          key_len = strlen(prefix->prefix);
          name = prefix->prefix + strlen(dir->ino.key);
          name_len = strlen(name);

          if (name_len >= DPL_MAXNAMLEN)
            {
              DPLERR(0, "name is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->name, name, name_len);
          dirent->name[name_len] = 0;

          if (key_len >= DPL_MAXPATHLEN)
            {
              DPLERR(0, "key is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->ino.key, prefix->prefix, key_len);
          dirent->ino.key[key_len] = 0;
          dirent->type = DPL_FTYPE_DIR;

          dirent->last_modified = 0; //?
          dirent->size = 0;

          dir->directories_cursor++;

          return DPL_SUCCESS;
        }
    }
  else
    {
      dpl_object_t *obj;

      obj = (dpl_object_t *) dir->files->array[dir->files_cursor];

      key_len = strlen(obj->key);
      name = obj->key + strlen(dir->ino.key);
      name_len = strlen(name);

      if (!strcmp(name, "/") || !strcmp(name, ""))
        {
          memcpy(dirent->name, ".", 1);
          dirent->name[1] = 0;
        }
      else
        {
          if (name_len >= DPL_MAXNAMLEN)
            {
              DPLERR(0, "name is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->name, name, name_len);
          dirent->name[name_len] = 0;
        }

      if (key_len >= DPL_MAXPATHLEN)
        {
          DPLERR(0, "key is too long");
          return DPL_FAILURE;
        }
      memcpy(dirent->ino.key, obj->key, key_len);
      dirent->ino.key[key_len] = 0;

      if (key_len >= delim_len && !strcmp(obj->key + key_len - delim_len, dir->ctx->delim))
        dirent->type = DPL_FTYPE_DIR;
      else
        dirent->type = DPL_FTYPE_REG;

      dirent->last_modified = obj->last_modified;
      dirent->size = obj->size;

      dir->files_cursor++;

      return DPL_SUCCESS;
    }
}

static int
dpl_vdir_eof(void *dir_hdl)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  return dir->files_cursor == dir->files->n_items &&
    dir->directories_cursor == dir->directories->n_items;
}

static void
dpl_vdir_closedir(void *dir_hdl)
{
  dpl_dir_t *dir = dir_hdl;

  if (dir)
    {
      DPL_TRACE(dir->ctx, DPL_TRACE_VFS, "closedir dir_hdl=%p", dir_hdl);

      if (dir->files)
        dpl_vec_objects_free(dir->files);

      if (dir->directories)
        dpl_vec_common_prefixes_free(dir->directories);

      free(dir);
    }
}

static dpl_status_t
dpl_vdir_count_entries(dpl_ctx_t *ctx,
                       const char *bucket,
                       dpl_ino_t ino,
                       unsigned int *n_entriesp)
{
  void *dir_hdl = NULL;
  int ret, ret2;
  u_int n_entries = 0;
  dpl_dirent_t dirent;

  ret2 = dpl_vdir_opendir(ctx, bucket, ino, &dir_hdl);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  while (1 != dpl_vdir_eof(dir_hdl))
    {
      ret2 = dpl_vdir_readdir(dir_hdl, &dirent);
      if (DPL_SUCCESS != ret2)
        {
          if (DPL_ENOENT == ret2)
            break ;
          ret = ret2;
          goto end;
        }

      if (strcmp(dirent.name, "."))
        n_entries++;
    }

  if (NULL != n_entriesp)
    *n_entriesp = n_entries;

  ret = DPL_SUCCESS;

 end:

  if (NULL != dir_hdl)
    dpl_vdir_closedir(dir_hdl);

  return ret;
}

static dpl_status_t
dpl_vdir_rmdir(dpl_ctx_t *ctx,
               const char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name)
{
  u_int n_entries = 0;
  dpl_ino_t ino;
  int ret, ret2;
  int obj_name_len = strlen(obj_name);
  int delim_len = strlen(ctx->delim);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "rmdir bucket=%s parent_ino=%s name=%s", bucket, parent_ino.key, obj_name);

  if (!strcmp(obj_name, "."))
    {
      ret = DPL_EINVAL;
      goto end;
    }

  ino = parent_ino;
  //XXX check length
  strcat(ino.key, obj_name);
  //append delim to key if not already
  if (obj_name_len >= delim_len && strcmp(obj_name + obj_name_len - delim_len, ctx->delim))
    strcat(ino.key, ctx->delim);

  ret2 = dpl_vdir_count_entries(ctx, bucket, ino, &n_entries);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //printf("n_entries=%d\n", n_entries);

  if (0 != n_entries)
    {
      ret = DPL_ENOTEMPTY;
      goto end;
    }

  ret2 = dpl_delete(ctx, bucket, ino.key, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

/*
 * path based routines
 */

dpl_status_t
dpl_iname(dpl_ctx_t *ctx,
             const char *bucket,
             dpl_ino_t ino,
             char *path,
             unsigned int path_len)
{
  DPL_TRACE(ctx, DPL_TRACE_VFS, "iname bucket=%s ino=%s", bucket, ino.key);

  return DPL_FAILURE;
}

dpl_status_t
dpl_namei(dpl_ctx_t *ctx,
             const char *path,
             const char *bucket,
             dpl_ino_t ino,
             dpl_ino_t *parent_inop,
             dpl_ino_t *obj_inop,
             dpl_ftype_t *obj_typep)
{
  const char *p1, *p2;
  char name[DPL_MAXNAMLEN];
  int namelen;
  int ret;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  int delim_len = strlen(ctx->delim);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "namei path=%s bucket=%s ino=%s", path, bucket, ino.key);

  p1 = path;

  if (!strcmp(p1, ctx->delim))
    {
      if (NULL != parent_inop)
        *parent_inop = DPL_ROOT_INO;
      if (NULL != obj_inop)
        *obj_inop = DPL_ROOT_INO;
      if (NULL != obj_typep)
        *obj_typep = DPL_FTYPE_DIR;
      return DPL_SUCCESS;
    }

  //absolute path
  if (!strncmp(p1, ctx->delim, delim_len))
    {
      if (ctx->light_mode)
        {
          char save_path[DPL_MAXPATHLEN];
          int len;
          char *p;

          len = strlen(path);
          strcpy(save_path, path); //XXX check length

          //dont try to resolve path
          if (NULL != obj_inop)
            strcpy(obj_inop->key, save_path); //XXX check length

          obj_type = DPL_FTYPE_REG; //default

          if (len > delim_len)
            {
              p = dpl_strrstr(save_path, ctx->delim);
              if (NULL == p)
                goto fallback;

              //last bytes are delim
              if (!strcmp(p, ctx->delim))
                obj_type = DPL_FTYPE_DIR;
              else
                obj_type = DPL_FTYPE_REG;
              
              *p = 0;

              //again for parent
              p = dpl_strrstr(save_path, ctx->delim);
              if (NULL == p)
                goto fallback;

              *p = 0;
            }
          else
            {
            fallback:
              save_path[0] = 0;
            }

          if (save_path[0] == 0)
            strcpy(save_path, ctx->delim);

          if (NULL != parent_inop)
            strcpy(parent_inop->key, save_path); //XXX check length

          if (NULL != obj_typep)
            *obj_typep = obj_type;
          return DPL_SUCCESS;
        }

      parent_ino = DPL_ROOT_INO;
      p1 += delim_len;
    }
  else
    {
      parent_ino = ino;
    }

  while (1)
    {
      p2 = strstr(p1, ctx->delim);
      if (NULL == p2)
        {
          namelen = strlen(p1);
        }
      else
        {
          p2 += delim_len;
          namelen = p2 - p1 - 1;
        }

      if (namelen >= DPL_MAXNAMLEN)
        return DPL_ENAMETOOLONG;

      memcpy(name, p1, namelen);
      name[namelen] = 0;

      DPRINTF("lookup '%s'\n", name);

      if (!strcmp(name, ""))
        {
          obj_ino = parent_ino;
          obj_type = DPL_FTYPE_DIR;
        }
      else
        {
          ret = dpl_vdir_lookup(ctx, bucket, parent_ino, name, &obj_ino, &obj_type);
          if (DPL_SUCCESS != ret)
            return ret;
        }

      DPRINTF("p2='%s'\n", p2);

      if (NULL == p2)
        {
          if (NULL != parent_inop)
            *parent_inop = parent_ino;
          if (NULL != obj_inop)
            *obj_inop = obj_ino;
          if (NULL != obj_typep)
            *obj_typep = obj_type;

          return DPL_SUCCESS;
        }
      else
        {
          if (DPL_FTYPE_DIR != obj_type)
            return DPL_ENOTDIR;

          parent_ino = obj_ino;
          p1 = p2;

          DPRINTF("remain '%s'\n", p1);
        }
    }

  return DPL_FAILURE;
}

dpl_ino_t
dpl_cwd(dpl_ctx_t *ctx,
           const char *bucket)
{
  dpl_var_t *var;
  dpl_ino_t cwd;

  pthread_mutex_lock(&ctx->lock);
  var = dpl_dict_get(ctx->cwds, bucket);
  if (NULL != var)
    strcpy(cwd.key, var->value); //XXX check overflow
  else
    cwd = DPL_ROOT_INO;
  pthread_mutex_unlock(&ctx->lock);

  return cwd;
}

/**
 * open a directory
 *
 * @param ctx
 * @param locator [bucket:]path
 * @param dir_hdlp
 *
 * @return
 */
dpl_status_t
dpl_opendir(dpl_ctx_t *ctx,
               const char *locator,
               void **dir_hdlp)
{
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "opendir locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, NULL, &obj_ino, &obj_type);
  if (0 != ret2)
    {
      DPLERR(0, "path resolve failed %s", path);
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG == obj_type)
    {
      DPLERR(0, "cannot list a file");
      ret = DPL_EINVAL;
      goto end;
    }

  ret2 = dpl_vdir_opendir(ctx, bucket, obj_ino, dir_hdlp);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "unable to open %s:%s", bucket, obj_ino.key);
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_readdir(void *dir_hdl,
               dpl_dirent_t *dirent)
{
  return dpl_vdir_readdir(dir_hdl, dirent);
}

int
dpl_eof(void *dir_hdl)
{
  return dpl_vdir_eof(dir_hdl);
}

void
dpl_closedir(void *dir_hdl)
{
  dpl_vdir_closedir(dir_hdl);
}

dpl_status_t
dpl_chdir(dpl_ctx_t *ctx,
             const char *locator)
{
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  dpl_ino_t cur_ino;
  char *nbucket;
  char *path;
  char *bucket = NULL;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "chdir locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, NULL, &obj_ino, &obj_type);
  if (0 != ret2)
    {
      DPLERR(0, "path resolve failed %s: %s (%d)", path, dpl_status_str(ret2), ret2);
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_DIR != obj_type)
    {
      DPLERR(0, "not a directory");
      ret = DPL_EINVAL;
      goto end;
    }

  pthread_mutex_lock(&ctx->lock);
  if (strcmp(bucket, ctx->cur_bucket))
    {
      nbucket = strdup(bucket);
      if (NULL == nbucket)
        {
          pthread_mutex_unlock(&ctx->lock);
          ret = DPL_ENOMEM;
          goto end;
        }
      free(ctx->cur_bucket);
      ctx->cur_bucket = nbucket;
    }

  ret2 = dpl_dict_add(ctx->cwds, ctx->cur_bucket, obj_ino.key, 0);
  pthread_mutex_unlock(&ctx->lock);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}


static dpl_status_t
dpl_mkgen(dpl_ctx_t *ctx,
          const char *locator,
          dpl_status_t (*cb)(dpl_ctx_t *, const char *, dpl_ino_t, const char *))
{
  char *dir_name = NULL;
  dpl_ino_t parent_ino;
  int ret, ret2;
  char *nlocator = NULL;
  int delim_len = strlen(ctx->delim);
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "mkdir locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          dir_name = dpl_strrstr(path, ctx->delim);
          if (NULL != dir_name)
            {
              *dir_name = 0;
              dir_name += delim_len;

              //fetch parent directory
              ret2 = dpl_namei(ctx, !strcmp(path, "") ? ctx->delim : path, bucket, cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", path, dpl_status_str(ret2));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = cur_ino;
              dir_name = path;
            }
        }
      else
        {
          DPLERR(0, "path resolve failed %s: %s (%d)\n", path, dpl_status_str(ret2), ret2);
          ret = ret2;
          goto end;
        }
    }
  else
    {
      ret = DPL_EEXIST;
      goto end;
    }

  ret2 = cb(ctx, bucket, parent_ino, dir_name);
  if (0 != ret2)
    {
      DPLERR(0, "mkdir failed");
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_mkdir(dpl_ctx_t *ctx,
             const char *locator)
{
  return dpl_mkgen(ctx, locator, dpl_vdir_mkdir);
}


dpl_status_t
dpl_mknod(dpl_ctx_t *ctx,
             const char *locator)
{
  return dpl_mkgen(ctx, locator, dpl_vdir_mknod);
}


dpl_status_t
dpl_rmdir(dpl_ctx_t *ctx,
             const char *locator)
{
  int ret, ret2;
  char *dir_name = NULL;
  dpl_ino_t parent_ino;
  int delim_len = strlen(ctx->delim);
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "rmdir locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  dir_name = dpl_strrstr(path, ctx->delim);
  if (NULL != dir_name)
    dir_name += delim_len;
  else
    dir_name = path;

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "path resolved failed");
      ret = ret2;
      goto end;
    }

  ret2 = dpl_vdir_rmdir(ctx, bucket, parent_ino, dir_name);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "rmdir failed");
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

/* 
 * vfile
 */

dpl_status_t
dpl_close_ex(dpl_vfile_t *vfile,
             char **resource_idp)
{
  int ret, ret2;
  dpl_dict_t *headers_returned = NULL;
  int connection_close = 0;
  dpl_var_t *var;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFS, "close vfile=%p", vfile);

  ret = DPL_SUCCESS;

  if (NULL != vfile->conn)
    {
      ret2 = dpl_read_http_reply(vfile->conn, 1, NULL, NULL, &headers_returned);
      if (DPL_SUCCESS != ret2)
        {
          //too common to print
          //fprintf(stderr, "read http_reply failed %s (%d)\n", dpl_status_str(ret2), ret2);
          ret = ret2;
        }
      else
        {
          if (vfile->flags & DPL_VFILE_FLAG_MD5)
            {
              ret2 = dpl_dict_get_lowered(headers_returned, "Content-MD5", &var);
              if (DPL_SUCCESS != ret2 || NULL == var)
                {
                  fprintf(stderr, "missing 'Content-MD5' in answer\n");
                  //XXX ret = DPL_FAILURE;
                }
              else
                {
                  //compare MD5's
                  u_char digest[MD5_DIGEST_LENGTH];
                  char bcd_digest[DPL_BCD_LENGTH(MD5_DIGEST_LENGTH) + 1];
                  u_int bcd_digest_len;

                  MD5_Final(digest, &vfile->md5_ctx);

                  bcd_digest_len = dpl_bcd_encode(digest, MD5_DIGEST_LENGTH, bcd_digest);
                  bcd_digest[bcd_digest_len] = 0;

                  //skip quotes
                  if (strncmp(var->value + 1, bcd_digest, DPL_BCD_LENGTH(MD5_DIGEST_LENGTH)))
                    {
                      fprintf(stderr, "MD5 checksum dont match\n");
                    }
                }
            }
        }

      connection_close = dpl_connection_close(vfile->ctx, headers_returned);

      if (1 == connection_close)
        dpl_conn_terminate(vfile->conn);
      else
        dpl_conn_release(vfile->conn);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (NULL != vfile->cipher_ctx)
        EVP_CIPHER_CTX_free(vfile->cipher_ctx);
    }

  if (DPL_SUCCESS == ret)
    {
      if (!strcmp(vfile->ctx->backend->name, "cdmi") && vfile->ctx->cdmi_have_metadata)
        {
          //require separate metadata update
          if (NULL != vfile->bucket && 
              NULL != vfile->resource &&
              NULL != vfile->metadata)
            {
              ret2 = dpl_cdmi_put(vfile->ctx, vfile->bucket, vfile->resource, "metadata",
                                  DPL_FTYPE_REG, vfile->metadata, DPL_CANNED_ACL_UNDEF,
                                  NULL, 0);
              if (DPL_SUCCESS != ret2)
                {
                  ret = ret2;
                }
            }
        }

      //XXX get resource id
    }

  if (NULL != vfile->headers_reply)
    dpl_dict_free(vfile->headers_reply);

  if (NULL != vfile->bucket)
    free(vfile->bucket);

  if (NULL != vfile->resource)
    free(vfile->resource);

  if (NULL != vfile->metadata)
    dpl_dict_free(vfile->metadata);

  free(vfile);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  return ret;
}

dpl_status_t
dpl_close(dpl_vfile_t *vfile)
{
  return dpl_close_ex(vfile, NULL);
}

static dpl_status_t
encrypt_init(dpl_vfile_t *vfile,
             int enc)
{
  int ret;
  const EVP_CIPHER *cipher;
  const EVP_MD *md;
  u_char key[EVP_MAX_KEY_LENGTH];
  u_char iv[EVP_MAX_IV_LENGTH];

  if (NULL == vfile->ctx->encrypt_key)
    {
      fprintf(stderr, "missing encrypt_key in conf\n");
      ret = DPL_EINVAL;
      goto end;
    }

  cipher = EVP_get_cipherbyname("aes-256-cfb");
  if (NULL == cipher)
    {
      fprintf(stderr, "unsupported cipher\n");
      ret = DPL_EINVAL;
      goto end;
    }

  vfile->cipher_ctx = EVP_CIPHER_CTX_new();
  if (NULL == vfile->cipher_ctx)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  md = EVP_md5();
  if (NULL == md)
    {
      fprintf(stderr, "unsupported md\n");
      ret = DPL_FAILURE;
      goto end;
    }

  EVP_BytesToKey(cipher, md, vfile->salt, (u_char *) vfile->ctx->encrypt_key, strlen(vfile->ctx->encrypt_key), 1, key, iv);

  EVP_CipherInit(vfile->cipher_ctx, cipher, key, iv, enc);

  ret = DPL_SUCCESS;

 end:

  return ret;
}

dpl_status_t
dpl_openwrite_ex(dpl_ctx_t *ctx,
                 const char *locator,
                 dpl_ftype_t object_type,
                 dpl_vfile_flag_t flags,
                 dpl_dict_t *metadata,
                 dpl_canned_acl_t canned_acl,
                 unsigned int data_len,
                 dpl_dict_t *query_params,
                 dpl_vfile_t **vfilep)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  int delim_len = strlen(ctx->delim);
  dpl_ino_t cur_ino;
  int own_metadata = 0;
  dpl_sysmd_t sysmd;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "openwrite locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          char *remote_name;
          
          if (!(vfile->flags & DPL_VFILE_FLAG_CREAT))
            {
              ret = DPL_FAILURE;
              goto end;
            }
          
          remote_name = dpl_strrstr(path, ctx->delim);
          if (NULL != remote_name)
            {
              remote_name += delim_len;
              *remote_name = 0;
              
              //fetch parent directory
              ret2 = dpl_namei(ctx, !strcmp(path, "") ? ctx->delim : path, bucket, cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", path, dpl_status_str(ret2));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = cur_ino;
              remote_name = path;
            }
          
          // If the remote_name string length is 0,
          // that means that the parent ino was searched with dpl_namei
          // and that we should concat the rest of the locator instead.
          obj_ino = parent_ino;
          if (remote_name[0] == '\0')
            strcat(obj_ino.key, &locator[path - nlocator + strlen(path)]);
          else
            strcat(obj_ino.key, remote_name); //XXX check length
          obj_type = object_type;
        }
      else
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }
  else
    {
      if (vfile->flags & DPL_VFILE_FLAG_EXCL)
        {
          ret = DPL_EEXIST;
          goto end;
        }
    }

  if (vfile->flags & DPL_VFILE_FLAG_MD5)
    {
      MD5_Init(&vfile->md5_ctx);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      ret2 = RAND_pseudo_bytes(vfile->salt, sizeof (vfile->salt));
      if (0 == ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }

      ret2 = encrypt_init(vfile, 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      if (NULL == metadata)
        {
          metadata = dpl_dict_new(13);
          if (NULL == metadata)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          own_metadata = 1;
        }

      ret2 = dpl_dict_add(metadata, "cipher", "yes", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_dict_add(metadata, "cipher-type", "aes-256-cfb", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  vfile->bucket = strdup(bucket);
  if (NULL == vfile->bucket)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  vfile->resource = strdup(obj_ino.key);
  if (NULL == vfile->resource)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != metadata)
    {
      vfile->metadata = dpl_dict_new(13);
      if (NULL == vfile->metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      
      ret2 = dpl_dict_copy(vfile->metadata, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  memset(&sysmd, 0, sizeof (sysmd));
  sysmd.mask = DPL_SYSMD_MASK_CANNED_ACL;
  sysmd.canned_acl = canned_acl;

  if (flags & DPL_VFILE_FLAG_POST)
    ret2 = dpl_post_buffered(ctx, bucket, obj_ino.key, NULL, obj_type, metadata, &sysmd,
                             data_len, query_params, &vfile->conn);
  else
    ret2 = dpl_put_buffered(ctx, bucket, obj_ino.key, NULL, obj_type, metadata, &sysmd,
                            data_len, &vfile->conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != vfilep)
    {
      *vfilep = vfile;
      vfile = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != vfile)
    dpl_close(vfile);

  if (NULL != nlocator)
    free(nlocator);

  if (1 == own_metadata)
    dpl_dict_free(metadata);

  return ret;
}

dpl_status_t
dpl_openwrite(dpl_ctx_t *ctx,
              const char *locator,
              dpl_vfile_flag_t flags,
              dpl_dict_t *metadata,
              dpl_canned_acl_t canned_acl,
              unsigned int data_len,
              dpl_vfile_t **vfilep)
{
  return dpl_openwrite_ex(ctx, locator, DPL_FTYPE_REG, flags, metadata, canned_acl, data_len, NULL, vfilep);
}

/**
 * write buffer
 *
 * @param vfile
 * @param buf
 * @param len
 *
 * @note ensure that all the buffer is written
 *
 * @return
 */
dpl_status_t
dpl_write(dpl_vfile_t *vfile,
             char *buf,
             unsigned int len)
{
  int ret, ret2;
  struct iovec iov[10];
  int n_iov = 0;
  char *obuf = NULL;
  int olen;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFS, "write_all vfile=%p", vfile);

  if (vfile->flags & DPL_VFILE_FLAG_MD5)
    {
      MD5_Update(&vfile->md5_ctx, buf, len);
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (0 == vfile->header_done)
        {
          iov[n_iov].iov_base = DPL_ENCRYPT_MAGIC;
          iov[n_iov].iov_len = strlen(DPL_ENCRYPT_MAGIC);

          n_iov++;

          iov[n_iov].iov_base = vfile->salt;
          iov[n_iov].iov_len = sizeof (vfile->salt);

          n_iov++;

          vfile->header_done = 1;
        }

      obuf = malloc(len);
      if (NULL == obuf)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret = EVP_CipherUpdate(vfile->cipher_ctx, (u_char *) obuf, &olen, (u_char *) buf, len);
      if (0 == ret)
        {
          DPLERR(0, "CipherUpdate failed\n");
          ret = DPL_FAILURE;
          goto end;
        }

      iov[n_iov].iov_base = obuf;
      iov[n_iov].iov_len = olen;

      n_iov++;
    }
  else
    {
      iov[n_iov].iov_base = buf;
      iov[n_iov].iov_len = len;

      n_iov++;
    }

  ret2 = dpl_conn_writev_all(vfile->conn, iov, n_iov, DPL_DEFAULT_WRITE_TIMEOUT);

  ret = ret2;

 end:

  if (NULL != obuf)
    free(obuf);

  return ret;
}

/**/

static dpl_status_t
cb_vfile_header(void *cb_arg,
                const char *header,
                char *value)
{
  dpl_vfile_t *vfile = (dpl_vfile_t *) cb_arg;
  int ret, ret2;

  ret2 = dpl_dict_add(vfile->headers_reply, header, value, 1);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (!strcmp(header, "x-amz-meta-cipher"))
    {
      if (!strcmp(value, "yes"))
        {
          vfile->flags |= DPL_VFILE_FLAG_ENCRYPT;
        }
    }

  //XXX check cipher-type

  ret = DPL_SUCCESS;

 end:

  return ret;
}

static dpl_status_t
cb_vfile_buffer(void *cb_arg,
                char *buf,
                u_int len)
{
  dpl_vfile_t *vfile = (dpl_vfile_t *) cb_arg;
  char *obuf = NULL;
  int olen;
  int ret, ret2;

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (0 == vfile->header_done)
        {
          u_int magic_len;
          u_int header_len;

          magic_len = strlen(DPL_ENCRYPT_MAGIC);

          header_len = magic_len + sizeof (vfile->salt);
          if (len < header_len)
            {
              DPLERR(0, "not enough bytes for decrypting");
              ret = DPL_EINVAL;
              goto end;
            }

          memcpy(vfile->salt, buf + magic_len, sizeof (vfile->salt));
          buf += header_len;
          len -= header_len;

          ret2 = encrypt_init(vfile, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          vfile->header_done = 1;
        }

      obuf = malloc(len);
      if (NULL == obuf)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret2 = EVP_CipherUpdate(vfile->cipher_ctx, (u_char *) obuf, &olen, (u_char *) buf, len);
      if (0 == ret2)
        {
          DPLERR(0, "CipherUpdate failed\n");
          ret = DPL_FAILURE;
          goto end;
        }

      buf = obuf;
      len = olen;
    }

  if (NULL != vfile->buffer_func)
    {
      ret2 = vfile->buffer_func(vfile->cb_arg, buf, len);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != obuf)
    free(obuf);

  return ret;
}

dpl_status_t
dpl_openread(dpl_ctx_t *ctx,
             const char *locator,
             dpl_vfile_flag_t flags,
             dpl_condition_t *condition,
             dpl_buffer_func_t buffer_func,
             void *cb_arg,
             dpl_dict_t **metadatap)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;
  dpl_dict_t *metadata = NULL;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "openread locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->headers_reply = dpl_dict_new(13);
  if (NULL == vfile->headers_reply)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  vfile->ctx = ctx;
  vfile->flags = flags;
  vfile->buffer_func = buffer_func;
  vfile->cb_arg = cb_arg;

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_get_buffered(ctx, bucket, obj_ino.key, NULL, DPL_FTYPE_ANY,
                          condition, cb_vfile_header, cb_vfile_buffer, vfile);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (!strcmp(ctx->backend->name, "cdmi") && ctx->cdmi_have_metadata)
    {
      ret2 = dpl_cdmi_head(ctx, bucket, obj_ino.key, NULL, DPL_FTYPE_ANY, NULL, &metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }
  else
    {
      metadata = dpl_dict_new(13);
      if (NULL == metadata)
        {
          ret = DPL_ENOMEM;
          goto end;
        }

      ret2 = dpl_get_metadata_from_headers(ctx, vfile->headers_reply, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != vfile)
    dpl_close(vfile);

  if (NULL != nlocator)
    free(nlocator);

  if (NULL != metadata)
    dpl_dict_free(metadata);

  return ret;
}

dpl_status_t
dpl_openread_range(dpl_ctx_t *ctx,
                   const char *locator,
                   dpl_vfile_flag_t flags,
                   dpl_condition_t *condition,
                   int start,
                   int end,
                   char **data_bufp,
                   unsigned int *data_lenp,
                   dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "openread locator=%s flags=0x%x", locator, flags);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_get_range(ctx, bucket, obj_ino.key, NULL, DPL_FTYPE_REG,
                       condition, start, end, data_bufp, data_lenp, metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_unlink(dpl_ctx_t *ctx,
              const char *locator)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "unlink locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_delete(ctx, bucket, obj_ino.key, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_getattr(dpl_ctx_t *ctx,
               const char *locator,
               dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "getattr locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_head(ctx, bucket, obj_ino.key, NULL, NULL, metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_getattr_raw(dpl_ctx_t *ctx,
                const char *locator,
                dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "getattr locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_head_all(ctx, bucket, obj_ino.key, NULL, NULL, metadatap);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_setattr(dpl_ctx_t *ctx,
               const char *locator,
               dpl_dict_t *metadata)
{
  int ret, ret2;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "setattr locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, &parent_ino, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_REG != obj_type)
    {
      ret = DPL_EISDIR;
      goto end;
    }

  ret2 = dpl_copy(ctx, bucket, obj_ino.key, NULL, bucket, obj_ino.key, NULL, DPL_FTYPE_REG, DPL_METADATA_DIRECTIVE_REPLACE, metadata, DPL_CANNED_ACL_UNDEF, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  return ret;
}

dpl_status_t
dpl_fgenurl(dpl_ctx_t *ctx,
               const char *locator,
               time_t expires,
               char *buf,
               unsigned int len,
               unsigned int *lenp)
{
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  dpl_ino_t cur_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "fgenurl locator=%s", locator);

  nlocator = strdup(locator);
  if (NULL == nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  path = index(nlocator, ':');
  if (NULL != path)
    {
      *path++ = 0;
      bucket = strdup(nlocator);
      if (NULL == bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  cur_ino = dpl_cwd(ctx, bucket);

  ret2 = dpl_namei(ctx, path, bucket, cur_ino, NULL, &obj_ino, &obj_type);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "namei failed %s (%d)\n", dpl_status_str(ret2), ret2);
      ret = ret2;
      goto end;
    }


  ret2 = dpl_genurl(ctx, bucket, obj_ino.key, NULL, expires, buf, len, lenp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

dpl_status_t
dpl_fcopy(dpl_ctx_t *ctx,
             const char *src_locator,
             const char *dst_locator)
{
  int ret, ret2;
  char *src_nlocator = NULL;
  char *src_bucket = NULL;
  char *src_path;
  dpl_ino_t src_cur_ino;
  dpl_ino_t src_obj_ino;
  dpl_ftype_t src_obj_type;
  char *dst_nlocator = NULL;
  char *dst_bucket = NULL;
  char *dst_path;
  dpl_ino_t dst_cur_ino;
  dpl_ino_t dst_obj_ino;
  dpl_ftype_t dst_obj_type;
  int delim_len = strlen(ctx->delim);
  dpl_ino_t parent_ino;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "fcopy src_locator=%s dst_locator=%s", src_locator, dst_locator);

  src_nlocator = strdup(src_locator);
  if (NULL == src_nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  src_path = index(src_nlocator, ':');
  if (NULL != src_path)
    {
      *src_path++ = 0;
      src_bucket = strdup(src_nlocator);
      if (NULL == src_bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      src_bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == src_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      src_path = src_nlocator;
    }

  src_cur_ino = dpl_cwd(ctx, src_bucket);

  dst_nlocator = strdup(dst_locator);
  if (NULL == dst_nlocator)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dst_path = index(dst_nlocator, ':');
  if (NULL != dst_path)
    {
      *dst_path++ = 0;
      dst_bucket = strdup(dst_nlocator);
      if (NULL == dst_bucket)
        {
          ret = ENOMEM;
          goto end;
        }
    }
  else
    {
      pthread_mutex_lock(&ctx->lock);
      dst_bucket = strdup(ctx->cur_bucket);
      pthread_mutex_unlock(&ctx->lock);
      if (NULL == dst_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      dst_path = dst_nlocator;
    }

  dst_cur_ino = dpl_cwd(ctx, dst_bucket);

  ret2 = dpl_namei(ctx, src_path, src_bucket, src_cur_ino, NULL, &src_obj_ino, &src_obj_type);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "namei failed %s (%d)\n", dpl_status_str(ret2), ret2);
      ret = ret2;
      goto end;
    }

  ret2 = dpl_namei(ctx, dst_path, dst_bucket, dst_cur_ino, NULL, &dst_obj_ino, &dst_obj_type);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          char *remote_name;
          
          remote_name = dpl_strrstr(dst_path, ctx->delim);
          if (NULL != remote_name)
            {
              *remote_name = 0;
              remote_name += delim_len;
              
              //fetch parent directory
              ret2 = dpl_namei(ctx, !strcmp(dst_path, "") ? ctx->delim : dst_path, dst_bucket, dst_cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", dst_path, dpl_status_str(ret2));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = dst_cur_ino;
              remote_name = dst_path;
            }
          
          dst_obj_ino = parent_ino;
          strcat(dst_obj_ino.key, remote_name); //XXX check length
          dst_obj_type = DPL_FTYPE_REG;
        }
    }

  ret2 = dpl_copy(ctx, src_bucket, src_obj_ino.key, NULL, dst_bucket, dst_obj_ino.key, NULL, DPL_FTYPE_REG, DPL_METADATA_DIRECTIVE_COPY, NULL, DPL_CANNED_ACL_UNDEF, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != dst_bucket)
    free(dst_bucket);

  if (NULL != src_bucket)
    free(src_bucket);

  if (NULL != dst_nlocator)
    free(dst_nlocator);

  if (NULL != src_nlocator)
    free(src_nlocator);

  return ret;
}
