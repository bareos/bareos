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
#include "droplet/vfs.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
dir_lookup(dpl_ctx_t *ctx,
           const char *bucket,
           dpl_fqn_t parent_fqn,
           const char *obj_name,
           dpl_fqn_t *obj_fqnp,
           dpl_ftype_t *obj_typep)
{
  int ret, ret2;
  dpl_vec_t *files = NULL;
  dpl_vec_t *directories = NULL;
  int i;
  dpl_fqn_t obj_fqn;
  dpl_ftype_t obj_type;
  int obj_name_len = strlen(obj_name);

  memset(&obj_fqn, 0, sizeof (obj_fqn));

  DPL_TRACE(ctx, DPL_TRACE_VFS, "lookup bucket=%s parent_fqn=%s obj_name=%s", bucket, parent_fqn.path, obj_name);

  if (!strcmp(obj_name, "."))
    {
      if (NULL != obj_fqnp)
        *obj_fqnp = parent_fqn;

      if (NULL != obj_typep)
        *obj_typep = DPL_FTYPE_DIR;

      ret = DPL_SUCCESS;
      goto end;
    }
  else if (!strcmp(obj_name, ".."))
    {
      char *p, *p2;

      if (!strcmp(parent_fqn.path, ""))
        {
          //silent success for root dir
          if (NULL != obj_fqnp)
            *obj_fqnp = DPL_ROOT_FQN;

          if (NULL != obj_typep)
            *obj_typep = DPL_FTYPE_DIR;

          ret = DPL_SUCCESS;
          goto end;
        }

      obj_fqn = parent_fqn;

      p = rindex(obj_fqn.path, '/');
      if (NULL == p)
        {
          fprintf(stderr, "parent path shall contain delimiter\n");
          ret = DPL_FAILURE;
          goto end;
        }

      p--;

      for (p2 = p;p2 > obj_fqn.path;p2--)
        {
          if (*p2 == '/')
            {
              DPRINTF("found delim\n");

              p2++;
              break ;
            }
        }

      *p2 = 0;

      if (NULL != obj_fqnp)
        *obj_fqnp = obj_fqn;

      if (NULL != obj_typep)
        *obj_typep = DPL_FTYPE_DIR;

      ret = DPL_SUCCESS;
      goto end;
    }

  //AWS do not like "" as a prefix
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(parent_fqn.path, "") ? NULL : parent_fqn.path, "/", -1, &files, &directories);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "list_bucket failed %s:%s", bucket, parent_fqn.path);
      ret = ret2;
      goto end;
    }

  for (i = 0;i < files->n_items;i++)
    {
      dpl_object_t *obj = (dpl_object_t *) dpl_vec_get(files, i);
      int path_len;
      char *p;

      p = rindex(obj->path, '/');
      if (NULL != p)
        p++;
      else
        p = obj->path;

      DPRINTF("cmp obj_path=%s obj_name=%s\n", p, obj_name);

      if (!strcmp(p, obj_name))
        {
          DPRINTF("ok\n");

          path_len = strlen(obj->path);
          if (path_len >= DPL_MAXNAMLEN)
            {
              DPL_TRACE(ctx, DPL_TRACE_ERR, "path is too long");
              ret = DPL_FAILURE;
              goto end;
            }
          memcpy(obj_fqn.path, obj->path, path_len);
          obj_fqn.path[path_len] = 0;
          if (path_len >= 1 && *(obj->path + path_len - 1) == '/')
            obj_type = DPL_FTYPE_DIR;
          else
            obj_type = DPL_FTYPE_REG;

          if (NULL != obj_fqnp)
            *obj_fqnp = obj_fqn;

          if (NULL != obj_typep)
            *obj_typep = obj_type;

          ret = DPL_SUCCESS;
          goto end;
        }
    }

  for (i = 0;i < directories->n_items;i++)
    {
      dpl_common_prefix_t *prefix = (dpl_common_prefix_t *) dpl_vec_get(directories, i);
      int path_len;
      char *p, *p2;

      p = rindex(prefix->prefix, '/');
      if (NULL == p)
        {
          fprintf(stderr, "prefix %s shall contain delimiter\n", prefix->prefix);
          continue ;
        }

      DPRINTF("p='%s'\n", p);

      p--;

      for (p2 = p;p2 > prefix->prefix;p2--)
        {
          DPRINTF("p2='%s'\n", p2);

          if (*p2 == '/')
            {
              DPRINTF("found delim\n");

              p2++;
              break ;
            }
        }

      path_len = p - p2 + 1;

      DPRINTF("cmp (prefix=%s) prefix=%.*s obj_name=%s\n", prefix->prefix, path_len, p2, obj_name);

      if (path_len == obj_name_len && !strncmp(p2, obj_name, obj_name_len))
        {
          DPRINTF("ok\n");

          path_len = strlen(prefix->prefix);
          if (path_len >= DPL_MAXNAMLEN)
            {
              DPL_TRACE(ctx, DPL_TRACE_ERR, "path is too long");
              ret = DPL_FAILURE;
              goto end;
            }
          memcpy(obj_fqn.path, prefix->prefix, path_len);
          obj_fqn.path[path_len] = 0;
          obj_type = DPL_FTYPE_DIR;

          if (NULL != obj_fqnp)
            *obj_fqnp = obj_fqn;

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
dir_open(dpl_ctx_t *ctx,
         const char *bucket,
         dpl_fqn_t fqn,
         void **dir_hdlp)
{
  dpl_dir_t *dir;
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "opendir bucket=%s fqn=%s", bucket, fqn.path);

  dir = malloc(sizeof (*dir));
  if (NULL == dir)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memset(dir, 0, sizeof (*dir));

  dir->ctx = ctx;

  dir->fqn = fqn;

  //AWS prefers NULL for listing the root dir
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(fqn.path, "") ? NULL : fqn.path, "/", -1, &dir->files, &dir->directories);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "list_bucket failed %s:%s", bucket, fqn.path);
      ret = ret2;
      goto end;
    }

  //printf("%s:%s n_files=%d n_dirs=%d\n", bucket, fqn.path, dir->files->n_items, dir->directories->n_items);

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
dir_read(void *dir_hdl,
         dpl_dirent_t *dirent)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;
  char *name;
  int name_len;
  int path_len;

  DPL_TRACE(dir->ctx, DPL_TRACE_VFS, "readdir dir_hdl=%p files_cursor=%d directories_cursor=%d", dir_hdl, dir->files_cursor, dir->directories_cursor);

  memset(dirent, 0, sizeof (*dirent));

  if (dir->files_cursor >= dir->files->n_items)
    {
      if (dir->directories_cursor >= dir->directories->n_items)
        {
          DPL_TRACE(dir->ctx, DPL_TRACE_ERR, "beyond cursors");
          return DPL_ENOENT;
        }
      else
        {
          dpl_common_prefix_t *prefix;

          prefix = (dpl_common_prefix_t *) dpl_vec_get(dir->directories, dir->directories_cursor);

          path_len = strlen(prefix->prefix);
          name = prefix->prefix + strlen(dir->fqn.path);
          name_len = strlen(name);

          if (name_len >= DPL_MAXNAMLEN)
            {
              DPL_TRACE(dir->ctx, DPL_TRACE_ERR, "name is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->name, name, name_len);
          dirent->name[name_len] = 0;

          if (path_len >= DPL_MAXPATHLEN)
            {
              DPL_TRACE(dir->ctx, DPL_TRACE_ERR, "path is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->fqn.path, prefix->prefix, path_len);
          dirent->fqn.path[path_len] = 0;
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

      obj = (dpl_object_t *) dpl_vec_get(dir->files, dir->files_cursor);

      path_len = strlen(obj->path);
      name = obj->path + strlen(dir->fqn.path);
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
              DPL_TRACE(dir->ctx, DPL_TRACE_ERR, "name is too long");
              return DPL_FAILURE;
            }
          memcpy(dirent->name, name, name_len);
          dirent->name[name_len] = 0;
        }

      if (path_len >= DPL_MAXPATHLEN)
        {
          DPL_TRACE(dir->ctx, DPL_TRACE_ERR, "path is too long");
          return DPL_FAILURE;
        }
      memcpy(dirent->fqn.path, obj->path, path_len);
      dirent->fqn.path[path_len] = 0;

      if (path_len >= 1 && *(obj->path + path_len - 1) == '/')
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
dir_eof(void *dir_hdl)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  return dir->files_cursor == dir->files->n_items &&
    dir->directories_cursor == dir->directories->n_items;
}

static void
dir_close(void *dir_hdl)
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

static int
is_rel_path(dpl_ctx_t *ctx,
            const char *path)
{
  //absolute path
  if (*path == '/')
    return 0;
  return 1;
}

static dpl_status_t
make_abs_path(dpl_ctx_t *ctx,
              const char *bucket,
              const char *path,
              dpl_fqn_t *obj_fqnp)
{
  const char *p1, *p2;
  char name[DPL_MAXNAMLEN];
  int namelen;
  dpl_status_t ret, ret2;
  dpl_fqn_t parent_fqn, obj_fqn;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "namei path=%s bucket=%s", path, bucket);

  obj_fqn.path[0] = 0;

  if (!is_rel_path(ctx, path))
    {
      strcpy(obj_fqn.path, path + 1);
      ret = DPL_SUCCESS;
      goto end;
    }

  p1 = path;
  parent_fqn = dpl_cwd(ctx, bucket);

  while (1)
    {
      p2 = index(p1, '/');
      if (NULL == p2)
        {
          namelen = strlen(p1);
        }
      else
        {
          p2++;
          namelen = p2 - p1 - 1;
        }

      if (namelen >= DPL_MAXNAMLEN)
        {
          ret = DPL_ENAMETOOLONG;
          goto end;
        }

      memcpy(name, p1, namelen);
      name[namelen] = 0;

      DPRINTF("lookup '%s'\n", name);

      if (!strcmp(name, ""))
        {
          //this is a dir terminator
          obj_fqn = parent_fqn;
        }
      else
        {
          ret2 = dir_lookup(ctx, bucket, parent_fqn, name, &obj_fqn, &obj_type);
          if (DPL_SUCCESS != ret2)
            {
              size_t plen = strlen(parent_fqn.path);;
              //make a fake path

              obj_fqn.path[0] = 0;
              strcat(obj_fqn.path, parent_fqn.path); //XXX
              if (plen > 1 && '/' != parent_fqn.path[plen - 1])
                {
                  strcat(obj_fqn.path, "/"); //XXX
                }
              strcat(obj_fqn.path, name); //XXX

              ret = DPL_SUCCESS;
              goto end;
            }
        }

      DPRINTF("p2='%s'\n", p2);

      if (NULL == p2)
        {
          ret = DPL_SUCCESS;
          goto end;
        }
      else
        {
          if (DPL_FTYPE_DIR != obj_type)
            {
              ret = DPL_ENOTDIR;
              goto end;
            }

          parent_fqn = obj_fqn;
          p1 = p2;

          DPRINTF("remain '%s'\n", p1);
        }
    }

  ret = DPL_FAILURE;
  
 end:

  if (DPL_SUCCESS == ret)
    {
      if (NULL != obj_fqnp)
        *obj_fqnp = obj_fqn;
    }

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d obj_fqn=%s", ret, obj_fqn.path);

  return ret;
}

static const char *
obj_type_ext(dpl_ftype_t type)
{
  switch (type)
    {
    case DPL_FTYPE_DIR:
      return "/";
    default:
      return "";
    }

  return "";
}

/*
 * path based routines
 */

dpl_fqn_t
dpl_cwd(dpl_ctx_t *ctx,
        const char *bucket)
{
  dpl_dict_var_t *var;
  dpl_fqn_t cwd;

  dpl_ctx_lock(ctx);
  var = dpl_dict_get(ctx->cwds, bucket);
  if (NULL != var)
    {
      assert(var->val->type == DPL_VALUE_STRING);
      strcpy(cwd.path, var->val->string); //XXX check overflow
    }
  else
    cwd = DPL_ROOT_FQN;
  dpl_ctx_unlock(ctx);

  return cwd;
}

static dpl_status_t
dir_is_empty(dpl_ctx_t *ctx,
             const char *path)
{
  dpl_status_t ret;
  dpl_status_t ret2;
  dpl_vec_t *objects = NULL;
  dpl_vec_t *common_prefixes = NULL;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "dir_is_empty path=%s", path);

  /* since dir_is_empty(<directory>) returns at least 1 common_prefix (itself), ask for two... */
  ret2 = dpl_list_bucket(ctx, ctx->cur_bucket, path, "/", 10, &objects, &common_prefixes);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (objects->n_items + common_prefixes->n_items >= 2)
    {
      ret = DPL_ENOTEMPTY;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (objects)
    dpl_vec_objects_free(objects);

  if (common_prefixes)
    dpl_vec_common_prefixes_free(common_prefixes);

  return ret;
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
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dir_open(ctx, bucket, obj_fqn, dir_hdlp);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "unable to open %s:%s", bucket, obj_fqn.path);
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_readdir(void *dir_hdl,
            dpl_dirent_t *dirent)
{
  return dir_read(dir_hdl, dirent);
}

int
dpl_eof(void *dir_hdl)
{
  return dir_eof(dir_hdl);
}

void
dpl_closedir(void *dir_hdl)
{
  dir_close(dir_hdl);
}

dpl_status_t
dpl_iterate(dpl_ctx_t *ctx,
            const char *locator,
            int (* cb)(dpl_dirent_t *dirent, void *ctx),
            void *user_data)
{
  dpl_status_t ret, ret2;
  void *dir_hdl = NULL;
  dpl_dirent_t dirent;

  ret2 = dpl_opendir(ctx, locator, &dir_hdl);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  while (DPL_SUCCESS == dpl_readdir(dir_hdl, &dirent))
    {
      if (-1 == cb(&dirent, user_data))
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  ret = DPL_SUCCESS;
 end:

  if (dir_hdl)
    dpl_closedir(dir_hdl);

  return ret;
}

dpl_status_t
dpl_chdir(dpl_ctx_t *ctx,
          const char *locator)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn, tmp_fqn;
  char *nlocator = NULL;
  char *nbucket;
  char *path;
  char *bucket = NULL;
  dpl_sysmd_t sysmd;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  dpl_ctx_lock(ctx);
  if (strcmp(bucket, ctx->cur_bucket))
    {
      nbucket = strdup(bucket);
      if (NULL == nbucket)
        {
          dpl_ctx_unlock(ctx);
          ret = DPL_ENOMEM;
          goto end;
        }
      free(ctx->cur_bucket);
      ctx->cur_bucket = nbucket;
    }
  dpl_ctx_unlock(ctx);

  //recheck obj_type: append delim at the end of object to be compatible with all backend
  tmp_fqn = obj_fqn;
  
  if (strcmp(tmp_fqn.path, ""))
    {
      int len = strlen(tmp_fqn.path);
      
      //append delim if not present
      if (len >= 1 && *(tmp_fqn.path + len - 1) != '/')
        strcat(tmp_fqn.path, "/");
    }

  ret2 = dpl_head(ctx, ctx->cur_bucket, tmp_fqn.path, NULL, DPL_FTYPE_UNDEF, NULL, NULL, &sysmd);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (sysmd.mask & DPL_SYSMD_MASK_FTYPE)
    {
      if (DPL_FTYPE_DIR != sysmd.ftype)
        {
          ret = DPL_ENOTDIR;
          goto end;
        }
    }

  ret2 = dpl_dict_add(ctx->cwds, ctx->cur_bucket, obj_fqn.path, 0);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

/* 
 * vfile
 */

dpl_status_t
dpl_close_ex(dpl_vfile_t *vfile,
             dpl_sysmd_t *returned_sysmdp,
             char **locationp)
{
  int ret, ret2;
  dpl_dict_t *headers_returned = NULL;
  int connection_close = 0;
  dpl_dict_var_t *var;

  DPL_TRACE(vfile->ctx, DPL_TRACE_VFS, "close vfile=%p", vfile);

  ret = DPL_SUCCESS;

  if (NULL != vfile->conn)
    {
      if (vfile->flags & DPL_VFILE_FLAG_ONESHOT)
        {
          //XXX ?
        }
      else
        {
          if (vfile->conn->type == DPL_CONN_TYPE_HTTP)
            {
              ret2 = dpl_read_http_reply(vfile->conn, 1, NULL, NULL, &headers_returned, &connection_close);
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
                          assert(var->val->type == DPL_VALUE_STRING);
                          if (strncmp(var->val->string + 1, bcd_digest, DPL_BCD_LENGTH(MD5_DIGEST_LENGTH)))
                            {
                              fprintf(stderr, "MD5 checksum dont match\n");
                            }
                        }
                    }
                }
            }
          
          if (1 == connection_close)
            dpl_conn_terminate(vfile->conn);
          else
            dpl_conn_release(vfile->conn);
        }
    }

  if (vfile->flags & DPL_VFILE_FLAG_ENCRYPT)
    {
      if (NULL != vfile->cipher_ctx)
        EVP_CIPHER_CTX_free(vfile->cipher_ctx);
    }

  if (NULL != vfile->bucket)
    free(vfile->bucket);

  if (NULL != vfile->resource)
    free(vfile->resource);

  if (NULL != vfile->metadata)
    dpl_dict_free(vfile->metadata);

  if (NULL != vfile->sysmd)
    dpl_sysmd_free(vfile->sysmd);

  if (NULL != vfile->query_params)
    dpl_dict_free(vfile->query_params);

  free(vfile);

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  return ret;
}

dpl_status_t
dpl_close(dpl_vfile_t *vfile)
{
  return dpl_close_ex(vfile, NULL, NULL);
}

static dpl_status_t
encrypt_init(dpl_vfile_t *vfile,
             int enc)
{
  int ret;
  const EVP_CIPHER *cipher;
  const EVP_MD *md;
  u_char path[EVP_MAX_KEY_LENGTH];
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

  EVP_BytesToKey(cipher, md, vfile->salt, (u_char *) vfile->ctx->encrypt_key, strlen(vfile->ctx->encrypt_key), 1, path, iv);

  EVP_CipherInit(vfile->cipher_ctx, cipher, path, iv, enc);

  ret = DPL_SUCCESS;

 end:

  return ret;
}

/** 
 * open a file for writing
 * 
 * @param ctx 
 * @param locator 
 * @param obj_type 
 * @param flags DPL_VFILE_FLAG_CREAT create file if it doesnt exist
 * @param flags DPL_VFILE_FLAG_EXCL exclusive creation
 * @param flags DPL_VFILE_FLAG_MD5 check MD5
 * @param flags DPL_VFILE_FLAG_ENCRYPT encrypt on the fly
 * @param flags DPL_VFILE_FLAG_POST use POST to write/creat file
 * @param flags DPL_VFILE_FLAG_ONESHOT put is done in one shot
 * @param flags DPL_VFILE_FLAG_RANGE put range
 * @param range
 * @param metadata 
 * @param sysmd 
 * @param data_len 
 * @param query_params 
 * @param vfilep 
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_EEXIST if DPL_VFILE_FLAG_EXCL was specified
 */
dpl_status_t
dpl_openwrite(dpl_ctx_t *ctx,
              const char *locator,
              dpl_ftype_t obj_type,
              dpl_vfile_flag_t flags,
              dpl_range_t *range,
              dpl_dict_t *metadata,
              dpl_sysmd_t *sysmd,
              unsigned int data_len,
              dpl_dict_t *query_params,
              dpl_vfile_t **vfilep)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  int own_metadata = 0;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "openwrite locator=%s flags=0x%x", locator, flags);

  if (flags & DPL_VFILE_FLAG_RANGE && !range)
    {
      ret = DPL_EINVAL;
      goto end;
    }

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
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

      ret2 = dpl_dict_add(metadata, DPL_CIPHER, "yes", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_dict_add(metadata, DPL_CIPHER_TYPE, "aes-256-cfb", 1);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (flags & DPL_VFILE_FLAG_ONESHOT)
    {
      vfile->bucket = strdup(bucket);
      if (NULL == vfile->bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      
      vfile->resource = strdup(obj_fqn.path);
      if (NULL == vfile->resource)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      
      vfile->obj_type = obj_type;
      
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
      
      if (NULL != sysmd)
        {
          vfile->sysmd = dpl_sysmd_dup(sysmd);
          if (NULL == vfile->sysmd)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }
      
      if (NULL != query_params)
        {
          vfile->query_params = dpl_dict_new(13);
          if (NULL == vfile->query_params)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
          
          ret2 = dpl_dict_copy(vfile->query_params, query_params);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }
  else
    {
      if (flags & DPL_VFILE_FLAG_POST)
        ret2 = dpl_post_buffered(ctx, bucket, obj_fqn.path, NULL, obj_type, NULL, NULL, metadata, sysmd,
                                 data_len, query_params, &vfile->conn);
      else
        ret2 = dpl_put_buffered(ctx, bucket, obj_fqn.path, NULL, obj_type, NULL, NULL, metadata, sysmd,
                                data_len, &vfile->conn);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
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
          DPL_TRACE(vfile->ctx, DPL_TRACE_ERR, "CipherUpdate failed\n");
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

  if (vfile->flags & DPL_VFILE_FLAG_ONESHOT)
    {
      if (vfile->flags & DPL_VFILE_FLAG_POST)
        ret2 = dpl_post(vfile->ctx, vfile->bucket, vfile->resource, NULL,
                        vfile->obj_type, NULL, NULL, vfile->metadata, vfile->sysmd,
                        buf, len, vfile->query_params, NULL);
      else
        ret2 = dpl_put(vfile->ctx, vfile->bucket, vfile->resource, NULL,
                       vfile->obj_type, NULL, NULL, vfile->metadata, vfile->sysmd,
                       buf, len);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else
    ret2 = dpl_conn_writev_all(vfile->conn, iov, n_iov, DPL_DEFAULT_WRITE_TIMEOUT);

  ret = ret2;

 end:

  if (NULL != obuf)
    free(obuf);

  return ret;
}

/**/

static dpl_status_t
cb_vfile_metadatum(void *cb_arg,
                   const char *key,
                   dpl_value_t *val)
{
  //dpl_vfile_t *vfile = (dpl_vfile_t *) cb_arg;

  //if (!strcmp(key, DPL_CIPHER))
  //{
  //if (val->type == DPL_VALUE_STRING &&
  //!strcmp(val->string, "yes"))
  //{
  //vfile->flags |= DPL_VFILE_FLAG_ENCRYPT;
  //}
  //}

  //XXX check DPL_CIPHER_TYPE

  return DPL_SUCCESS;
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
              DPL_TRACE(vfile->ctx, DPL_TRACE_ERR, "not enough bytes for decrypting");
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
          DPL_TRACE(vfile->ctx, DPL_TRACE_ERR, "CipherUpdate failed\n");
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

/** 
 * open a file for reading
 *
 * @note this function cannot be used to retrieve subresources
 * 
 * @param ctx 
 * @param locator 
 * @param flags DPL_VFILE_FLAG_RANGE
 * @param flags DPL_VFILE_FLAG_ONESHOT get is done in one shot
 * @param condition 
 * @param range_start 
 * @param range_end 
 * @param buffer_func 
 * @param cb_arg 
 * @param metadatap 
 * @param sysmdp 
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT
 */
dpl_status_t
dpl_openread(dpl_ctx_t *ctx,
             const char *locator,
             dpl_vfile_flag_t flags,
             dpl_condition_t *condition,
             dpl_range_t *range,
             dpl_buffer_func_t buffer_func,
             void *cb_arg,
             dpl_dict_t **metadatap,
             dpl_sysmd_t *sysmdp)
{
  dpl_vfile_t *vfile = NULL;
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  char *data_buf = NULL;
  unsigned int data_len;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "openread locator=%s flags=0x%x", locator, flags);

  if (flags & DPL_VFILE_FLAG_RANGE && !range)
    {
      ret = DPL_EINVAL;
      goto end;
    }

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  vfile = malloc(sizeof (*vfile));
  if (NULL == vfile)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memset(vfile, 0, sizeof (*vfile));

  vfile->ctx = ctx;
  vfile->flags = flags;
  vfile->buffer_func = buffer_func;
  vfile->cb_arg = cb_arg;

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (flags & DPL_VFILE_FLAG_ONESHOT)
    {
      if (flags & DPL_VFILE_FLAG_RANGE)
        ret2 = dpl_get(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_ANY,
                       condition, range, &data_buf, &data_len, metadatap, sysmdp);
      else
        ret2 = dpl_get(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_ANY,
                       condition, NULL, &data_buf, &data_len, metadatap, sysmdp);

      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
      else
        {
          buffer_func(cb_arg, data_buf, data_len);
        }
    }
  else
    {
      if (flags & DPL_VFILE_FLAG_RANGE)
        ret2 = dpl_get_buffered(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_ANY,
                                condition, range, cb_vfile_metadatum, metadatap, sysmdp, cb_vfile_buffer, vfile);
      else
        ret2 = dpl_get_buffered(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_ANY,
                                condition, NULL, cb_vfile_metadatum, metadatap, sysmdp, cb_vfile_buffer, vfile);

      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != vfile)
    dpl_close(vfile);

  if (NULL != nlocator)
    free(nlocator);

  if (NULL != data_buf)
    free(data_buf);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
dpl_mkobj(dpl_ctx_t *ctx,
          const char *locator,
          dpl_ftype_t obj_type,
          dpl_dict_t *metadata,
          dpl_sysmd_t *sysmd)
{
  dpl_fqn_t obj_fqn;
  int ret, ret2;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_VFS, "mkobj locator=%s", locator);

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", obj_fqn.path, obj_type_ext(obj_type));
  
  ret2 = dpl_put(ctx, bucket, resource, NULL, obj_type, NULL, NULL, metadata, sysmd, NULL, 0);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_mkdir(dpl_ctx_t *ctx,
          const char *locator,
          dpl_dict_t *metadata,
          dpl_sysmd_t *sysmd)
{
  return dpl_mkobj(ctx, locator, DPL_FTYPE_DIR, metadata, sysmd);
}

dpl_status_t
dpl_mknod(dpl_ctx_t *ctx,
          const char *locator,
          dpl_ftype_t object_type,
          dpl_dict_t *metadata,
          dpl_sysmd_t *sysmd)
{
  return dpl_mkobj(ctx, locator, object_type, metadata, sysmd);
}

dpl_status_t
dpl_rmdir(dpl_ctx_t *ctx,
          const char *locator)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  size_t path_len;
  char *npath = NULL;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
 
  path_len = strlen(obj_fqn.path);
  npath = malloc(path_len + 2);
  if (NULL == npath)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  memcpy(npath, obj_fqn.path, path_len);
  if (path_len == 0 || npath[path_len - 1] != '/')
    {
      npath[path_len] = '/';
      ++path_len;
    }
  npath[path_len] = '\0';

  if (!strcmp((char *) dpl_get_backend_name(ctx), "s3"))
    {
      //AWS does not do it server side
      ret2 = dir_is_empty(ctx, npath);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_delete(ctx, bucket, npath, NULL, DPL_FTYPE_DIR, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != npath)
    free(npath);

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_unlink(dpl_ctx_t *ctx,
           const char *locator)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_delete(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_REG, NULL);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_getattr(dpl_ctx_t *ctx,
            const char *locator,
            dpl_dict_t **metadatap,
            dpl_sysmd_t *sysmdp)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_head(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_UNDEF, NULL, metadatap, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  /* if sysmd.ftype is not set, do it! (s3 does not know the ftype) */
  if (sysmdp)
    {
      if (! (sysmdp->mask & DPL_SYSMD_MASK_FTYPE))
        {
          sysmdp->mask |= DPL_SYSMD_MASK_FTYPE;
          size_t path_len = strlen(obj_fqn.path);

          if (path_len > 1 && '/' == obj_fqn.path[path_len - 1])
            sysmdp->ftype = DPL_FTYPE_DIR;
          else
            sysmdp->ftype = DPL_FTYPE_REG;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != bucket)
    free(bucket);

  if (NULL != nlocator)
    free(nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_getattr_raw(dpl_ctx_t *ctx,
                const char *locator,
                dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_head_raw(ctx, bucket, obj_fqn.path, NULL, DPL_FTYPE_UNDEF, NULL, metadatap);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_setattr(dpl_ctx_t *ctx,
            const char *locator,
            dpl_dict_t *metadata,
            dpl_sysmd_t *sysmd)
{
  int ret, ret2;
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;
  size_t path_len;
  dpl_ftype_t object_type;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  path_len = strlen(path);
  if (path_len > 0u && path[path_len - 1] == '/')
    object_type = DPL_FTYPE_DIR;
  else
    object_type = DPL_FTYPE_REG;

  ret2 = dpl_copy(ctx, bucket, obj_fqn.path, bucket, obj_fqn.path, NULL, object_type, DPL_COPY_DIRECTIVE_METADATA_REPLACE, metadata, sysmd, NULL);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
copy_path_to_path(dpl_ctx_t *ctx,
                  const char *src_locator,
                  const char *dst_locator,
                  dpl_ftype_t object_type,
                  dpl_copy_directive_t copy_directive,
                  dpl_dict_t *metadata,
                  dpl_sysmd_t *sysmd)
{
  int ret, ret2;
  char *src_nlocator = NULL;
  char *src_bucket = NULL;
  char *src_path;
  char *dst_nlocator = NULL;
  char *dst_bucket = NULL;
  char *dst_path;
  dpl_fqn_t dst_obj_fqn, src_obj_fqn;
  size_t path_len;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "copy_path_to_path src_locator=%s dst_locator=%s", src_locator, dst_locator);

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      src_bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == src_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      src_path = src_nlocator;
    }

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      dst_bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == dst_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      dst_path = dst_nlocator;
    }

  ret2 = make_abs_path(ctx, src_bucket, src_path, &src_obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = make_abs_path(ctx, dst_bucket, dst_path, &dst_obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_DIR == object_type)
    {
      path_len = strlen(src_obj_fqn.path);
      if (src_obj_fqn.path[path_len - 1] != '/')
        {
          src_obj_fqn.path[path_len] = '/';
          src_obj_fqn.path[path_len + 1] = '\0'; //XXX
        }
      path_len = strlen(dst_obj_fqn.path);
      if (dst_obj_fqn.path[path_len - 1] != '/')
        {
          dst_obj_fqn.path[path_len] = '/';
          dst_obj_fqn.path[path_len + 1] = '\0'; //XXX
        }
    }
  ret2 = dpl_copy(ctx, src_bucket, src_obj_fqn.path, dst_bucket, dst_obj_fqn.path, NULL, object_type, copy_directive, metadata, sysmd, NULL);
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

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
copy_id_to_path(dpl_ctx_t *ctx,
                const char *src_id,
                uint32_t enterprise_number,
                const char *dst_locator,
                dpl_ftype_t object_type,
                dpl_copy_directive_t copy_directive,
                dpl_dict_t *metadata,
                dpl_sysmd_t *sysmd)
{
  int ret, ret2;
  char *dst_nlocator = NULL;
  char *dst_bucket = NULL;
  char *dst_path;
  dpl_fqn_t dst_obj_fqn;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "copy_id_to_path src_id=%s dst_locator=%s", src_id, dst_locator);

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      dst_bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == dst_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      dst_path = dst_nlocator;
    }

  ret2 = make_abs_path(ctx, dst_bucket, dst_path, &dst_obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_copy_id(ctx, dst_bucket, src_id, dst_bucket, dst_obj_fqn.path, NULL, object_type, copy_directive, metadata, sysmd, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != dst_bucket)
    free(dst_bucket);

  if (NULL != dst_nlocator)
    free(dst_nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

static dpl_status_t
copy_name_to_path(dpl_ctx_t *ctx,
                  const char *src_name,
                  const char *dst_locator,
                  dpl_ftype_t object_type,
                  dpl_copy_directive_t copy_directive,
                  dpl_dict_t *metadata,
                  dpl_sysmd_t *sysmd)
{
  int ret, ret2;
  char *dst_nlocator = NULL;
  char *dst_bucket = NULL;
  char *dst_path;
  dpl_fqn_t dst_obj_fqn;

  DPL_TRACE(ctx, DPL_TRACE_VFS, "copy_name_to_path src_id=%s dst_locator=%s", src_name, dst_locator);

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      dst_bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == dst_bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      dst_path = dst_nlocator;
    }

  ret2 = make_abs_path(ctx, dst_bucket, dst_path, &dst_obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_copy(ctx, dst_bucket, src_name, dst_bucket, dst_obj_fqn.path, NULL, object_type, copy_directive, metadata, sysmd, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != dst_bucket)
    free(dst_bucket);

  if (NULL != dst_nlocator)
    free(dst_nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}

/** 
 * server side copy
 * 
 * @param ctx 
 * @param src_locator 
 * @param dst_locator 
 * 
 * @return 
 */
dpl_status_t
dpl_fcopy(dpl_ctx_t *ctx,
          const char *src_locator,
          const char *dst_locator)
{
  return copy_path_to_path(ctx, src_locator, dst_locator, DPL_FTYPE_REG, DPL_COPY_DIRECTIVE_COPY, NULL, NULL);
}

dpl_status_t
dpl_rename(dpl_ctx_t *ctx,
           const char *src_locator,
           const char *dst_locator,
           dpl_ftype_t object_type)
{
  return copy_path_to_path(ctx, src_locator, dst_locator, object_type, DPL_COPY_DIRECTIVE_MOVE, NULL, NULL);
}

dpl_status_t
dpl_symlink(dpl_ctx_t *ctx,
            const char *src_locator,
            const char *dst_locator)
{
  return copy_path_to_path(ctx, src_locator, dst_locator, DPL_FTYPE_REG, DPL_COPY_DIRECTIVE_SYMLINK, NULL, NULL);
}

dpl_status_t
dpl_link(dpl_ctx_t *ctx,
         const char *src_locator,
         const char *dst_locator)
{
  return copy_path_to_path(ctx, src_locator, dst_locator, DPL_FTYPE_REG, DPL_COPY_DIRECTIVE_LINK, NULL, NULL);
}

dpl_status_t
dpl_mkdent(dpl_ctx_t *ctx,
           const char *src_id,
           const char *dst_locator,
           dpl_ftype_t object_type)
{
  return copy_id_to_path(ctx, src_id, ctx->enterprise_number, dst_locator, object_type, DPL_COPY_DIRECTIVE_MKDENT, NULL, NULL);
}

dpl_status_t
dpl_rmdent(dpl_ctx_t *ctx,
           const char *src_name,
           const char *dst_locator)
{
  return copy_name_to_path(ctx, src_name, dst_locator, DPL_FTYPE_DIR, DPL_COPY_DIRECTIVE_RMDENT, NULL, NULL);
}

dpl_status_t
dpl_mvdent(dpl_ctx_t *ctx,
           const char *src_locator,
           const char *dst_locator)
{
  return copy_path_to_path(ctx, src_locator, dst_locator, DPL_FTYPE_REG, DPL_COPY_DIRECTIVE_MVDENT, NULL, NULL);
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
  dpl_fqn_t obj_fqn;
  char *nlocator = NULL;
  char *bucket = NULL;
  char *path;

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
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      dpl_ctx_lock(ctx);
      bucket = strdup(ctx->cur_bucket);
      dpl_ctx_unlock(ctx);
      if (NULL == bucket)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
      path = nlocator;
    }

  ret2 = make_abs_path(ctx, bucket, path, &obj_fqn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_genurl(ctx, bucket, obj_fqn.path, NULL, expires, buf, len, lenp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (bucket)
    free(bucket);

  if (nlocator)
    free(nlocator);

  DPL_TRACE(ctx, DPL_TRACE_VFS, "ret=%d", ret);

  return ret;
}
