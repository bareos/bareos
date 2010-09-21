/*
 * Droplets, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplets
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dropletsp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_vdir_lookup(dpl_ctx_t *ctx,
                char *bucket,
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

  memset(&obj_ino, 0, sizeof (obj_ino));

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "lookup bucket=%s parent_ino=%s obj_name=%s", bucket, parent_ino.key, obj_name);

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

      obj_ino = parent_ino;

      p = rindex(obj_ino.key, '/');
      if (NULL == p)
        {
          //weird prefix
          ret = DPL_FAILURE;
          goto end;
        }

      p--;

      for (p2 = p;p2 > obj_ino.key;p2--)
        {
          if (*p2 == '/')
            {
              p2++;
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
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(parent_ino.key, "") ? NULL : parent_ino.key, "/", &files, &directories);
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

      p = rindex(obj->key, '/');
      if (NULL != p)
        p++;
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
          if ('/' == obj->key[key_len-1])
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

      p = rindex(prefix->prefix, '/');
      if (NULL == p)
        continue ; //weird prefix

      p--;

      for (p2 = p;p2 > prefix->prefix;p2--)
        {
          if (*p2 == '/')
            {
              p2++;
              break ;
            }
        }

      key_len = p - p2 + 1;

      DPRINTF("cmp (prefix=%s) prefix=%.*s obj_name=%.*s\n", prefix->prefix, key_len, p2, key_len, obj_name);

      if (!strncmp(p2, obj_name, key_len))
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

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_vdir_mknod()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vdir_mkdir(dpl_ctx_t *ctx,
               char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name)
{
  int ret, ret2;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "mkdir bucket=%s parent_ino=%s name=%s", bucket, parent_ino.key, obj_name);

  snprintf(resource, sizeof (resource), "%s%s/", parent_ino.key, obj_name);

  ret2 = dpl_put(ctx, bucket, resource, NULL, NULL, DPL_CANNED_ACL_PRIVATE, NULL, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_vdir_unlink()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vdir_rename()
{
  return DPL_FAILURE;
}

dpl_status_t
dpl_vdir_opendir(dpl_ctx_t *ctx,
                 char *bucket,
                 dpl_ino_t ino,
                 void **dir_hdlp)
{
  dpl_dir_t *dir;
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "opendir bucket=%s ino=%s", bucket, ino.key);

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
  ret2 = dpl_list_bucket(ctx, bucket, !strcmp(ino.key, "") ? NULL : ino.key, "/", &dir->files, &dir->directories);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "list_bucket failed %s:%s", bucket, ino.key);
      ret = DPL_FAILURE;
      goto end;
    }

  //printf("%s:%s n_files=%d n_dirs=%d\n", bucket, ino.key, dir->files->n_items, dir->directories->n_items);
  
  if (NULL != dir_hdlp)
    *dir_hdlp = dir;

  DPL_TRACE(dir->ctx, DPL_TRACE_VDIR, "dir_hdl=%p", dir);

  ret = DPL_SUCCESS;

 end:

  if (DPL_SUCCESS != ret)
    {
      if (NULL != dir->files)
        dpl_vec_objects_free(dir->files);

      if (NULL != dir->directories)
        dpl_vec_common_prefixes_free(dir->directories);

      if (NULL != dir)
        free(dir);
    }

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_vdir_readdir(void *dir_hdl,
                 dpl_dirent_t *dirent)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;
  char *name;
  int name_len;
  int key_len;

  DPL_TRACE(dir->ctx, DPL_TRACE_VDIR, "readdir dir_hdl=%p files_cursor=%d directories_cursor=%d", dir_hdl, dir->files_cursor, dir->directories_cursor);

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

      if (!strcmp(name, ""))
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

      if ('/' == obj->key[key_len-1])
        dirent->type = DPL_FTYPE_DIR;
      else
        dirent->type = DPL_FTYPE_REG;
      
      dirent->last_modified = obj->last_modified;
      dirent->size = obj->size;
      
      dir->files_cursor++;
      
      return DPL_SUCCESS;
    }
}

int
dpl_vdir_eof(void *dir_hdl)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  return dir->files_cursor == dir->files->n_items &&
    dir->directories_cursor == dir->directories->n_items;
}

void
dpl_vdir_closedir(void *dir_hdl)
{
  dpl_dir_t *dir = (dpl_dir_t *) dir_hdl;

  DPL_TRACE(dir->ctx, DPL_TRACE_VDIR, "closedir dir_hdl=%p", dir_hdl);
}

dpl_status_t
dpl_vdir_iname(dpl_ctx_t *ctx,
               char *bucket,
               dpl_ino_t ino,
               char *path,
               u_int path_len)
{
  DPL_TRACE(ctx, DPL_TRACE_VDIR, "iname bucket=%s ino=%s", bucket, ino.key);

  return DPL_FAILURE;
}

dpl_status_t
dpl_vdir_namei(dpl_ctx_t *ctx,
               char *path,
               char *bucket,
               dpl_ino_t ino,
               dpl_ino_t *parent_inop,
               dpl_ino_t *obj_inop,
               dpl_ftype_t *obj_typep)
{
  char *p1, *p2;
  char name[DPL_MAXNAMLEN];
  int namelen;
  int ret;
  dpl_ino_t parent_ino, obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "namei path=%s bucket=%s ino=%s", path, bucket, ino.key);

  p1 = path;

  if (!strcmp(p1, "/"))
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
  if ('/' == p1[0])
    {
      parent_ino = DPL_ROOT_INO;
      p1++;
    }
  else
    {
      parent_ino = ino;
    }

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

dpl_status_t
dpl_vdir_count_entries(dpl_ctx_t *ctx,
                       char *bucket,
                       dpl_ino_t ino,
                       u_int *n_entriesp)
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

dpl_status_t
dpl_vdir_rmdir(dpl_ctx_t *ctx,
               char *bucket,
               dpl_ino_t parent_ino,
               const char *obj_name)
{
  u_int n_entries;
  dpl_ino_t ino;
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "rmdir bucket=%s parent_ino=%s name=%s", bucket, parent_ino.key, obj_name);

  if (!strcmp(obj_name, "."))
    {
      ret = DPL_EINVAL;
      goto end;
    }

  ino = parent_ino;
  //XXX check length
  strcat(ino.key, obj_name);
  strcat(ino.key, "/");

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
dpl_opendir(dpl_ctx_t *ctx,
            char *path,
            void **dir_hdlp)
{
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "opendir path=%s", path);

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, NULL, &obj_ino, &obj_type);
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

  ret2 = dpl_vdir_opendir(ctx, ctx->cur_bucket, obj_ino, dir_hdlp);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "unable to open %s:%s", ctx->cur_bucket, obj_ino.key);
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

dpl_status_t
dpl_chdir(dpl_ctx_t *ctx,
          char *path)
{
  int ret, ret2;
  dpl_ino_t obj_ino;
  dpl_ftype_t obj_type;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "chdir path=%s", path);
  
  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, NULL, &obj_ino, &obj_type);
  if (0 != ret2)
    {
      DPLERR(0, "path resolve failed %s", path);
      ret = ret2;
      goto end;
    }

  if (DPL_FTYPE_DIR != obj_type)
    {
      DPLERR(0, "not a directory");
      ret = DPL_EINVAL;
      goto end;
    }

  ctx->cur_ino = obj_ino;

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

dpl_status_t
dpl_mkdir(dpl_ctx_t *ctx,
          char *path)
{
  char *dir_name = NULL;
  dpl_ino_t parent_ino;
  int ret, ret2;
  char *npath;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "mkdir path=%s", path);

  npath = strdup(path);
  if (NULL == npath)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          dir_name = rindex(path, '/');
          if (NULL != dir_name)
            {
              *dir_name++ = 0;
              
              //fetch parent directory                                         
              ret2 = dpl_vdir_namei(ctx, !strcmp(path, "") ? "/" : path, ctx->cur_bucket, ctx->cur_ino, NULL, &parent_ino, NULL);
              if (DPL_SUCCESS != ret2)
                {
                  DPLERR(0, "dst parent dir resolve failed %s: %s\n", path, dpl_status_str(ret));
                  ret = ret2;
                  goto end;
                }
            }
          else
            {
              parent_ino = ctx->cur_ino;
              dir_name = path;
            }
        }
      else
        {
          DPLERR(0, "path resolve failed %s: %s (%d)\n", path, dpl_status_str(ret), ret);
          ret = ret2;
          goto end;
        }
    }
  else
    {
      ret = DPL_EEXIST;
      goto end;
    }

  ret2 = dpl_vdir_mkdir(ctx, ctx->cur_bucket, parent_ino, dir_name);
  if (0 != ret2)
    {
      DPLERR(0, "mkdir failed");
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != npath)
    free(npath);

  return ret;
}

dpl_status_t
dpl_rmdir(dpl_ctx_t *ctx,
          char *path)
{
  int ret, ret2;
  char *dir_name = NULL;
  dpl_ino_t parent_ino;

  DPL_TRACE(ctx, DPL_TRACE_VDIR, "rmdir path=%s", path);
  
  dir_name = rindex(path, '/');
  if (NULL != dir_name)
    dir_name++;
  else
    dir_name = path;

  ret2 = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, &parent_ino, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "path resolved failed");
      ret = ret2;
      goto end;
    }

  ret2 = dpl_vdir_rmdir(ctx, ctx->cur_bucket, parent_ino, dir_name);
  if (DPL_SUCCESS != ret2)
    {
      DPLERR(0, "rmdir failed");
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}
