/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
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

#include "dplsh.h"

int cmd_ls(int argc, char **argv);

struct usage_def ls_usage[] =
  {
    {'l', 0u, NULL, "long display"},
    {'R', 0u, NULL, "recurse subdirectories"},
    {'a', 0u, NULL, "list all files in the bucket (do not use vdir interface)"},
    {USAGE_NO_OPT, 0u, "path or bucket", "remote directory"},
    {0, 0u, NULL, NULL},
  };

struct cmd_def ls_cmd = {"ls", "list directory", ls_usage, cmd_ls};

dpl_status_t
ls_recurse(struct ls_data *ls_data,
           char *dir,
           int level)
{
  int ret;

  if (1 == ls_data->aflag)
    {
      dpl_vec_t *objects = NULL;
      int i;
      
      //raw listing
      ret = dpl_list_bucket(ctx, ctx->cur_bucket, NULL, NULL, &objects, NULL);
      if (DPL_SUCCESS != ret)
        {
          fprintf(stderr, "listbucket failure %s (%d)\n", dpl_status_str(ret), ret);
          return ret;
        }      

      for (i = 0;i < objects->n_items;i++)
        {
          dpl_object_t *obj = (dpl_object_t *) objects->array[i];

          if (0 == ls_data->pflag)
            {
              if (ls_data->lflag)
                {
                  struct tm *stm;
                  
                  stm = localtime(&obj->last_modified);
                  printf("%12llu %04d-%02d-%02d %02d:%02d %s\n", (unsigned long long) obj->size, 1900 + stm->tm_year, 1 + stm->tm_mon, stm->tm_mday, stm->tm_hour, stm->tm_min, obj->key);
                }
              else
                {
                  printf("%s\n", obj->key);
                }
            }
          
          ls_data->total_size += obj->size;
        }

      if (NULL != objects)
        dpl_vec_objects_free(objects);
    }
  else
    {
      void *dir_hdl;
      dpl_dirent_t entry;
      dpl_ino_t cur_ino;

      if (1 == ls_data->Rflag)
        {
          ret = dpl_chdir(ctx, dir);
          if (DPL_SUCCESS != ret)
            return ret;
          
          cur_ino = dpl_cwd(ctx, ctx->cur_bucket);

          printf("%s%s%s:\n", 0 == level ? "" : "\n", ctx->delim, cur_ino.key);
          
          ret = dpl_opendir(ctx, ".", &dir_hdl);
          if (DPL_SUCCESS != ret)
            return ret;
        }
      else
        {
          ret = dpl_opendir(ctx, dir, &dir_hdl);
          if (DPL_SUCCESS != ret)
            return ret;
        }
      
      while (!dpl_eof(dir_hdl))
        {
          ret = dpl_readdir(dir_hdl, &entry);
          if (DPL_SUCCESS != ret)
            return ret;

          if (0 == ls_data->pflag)
            {
              if (ls_data->lflag)
                {
                  struct tm *stm;
                  
                  stm = localtime(&entry.last_modified);
                  printf("%12llu %04d-%02d-%02d %02d:%02d %s\n", (unsigned long long) entry.size, 1900 + stm->tm_year, 1 + stm->tm_mon, stm->tm_mday, stm->tm_hour, stm->tm_min, entry.name);
                }
              else
                {
                  printf("%s\n", entry.name);
                }
            }
          
          ls_data->total_size += entry.size;
          
          if (1 == ls_data->Rflag &&
              strcmp(entry.name, ".") && 
              (DPL_FTYPE_DIR == entry.type))
            {
              ret = ls_recurse(ls_data, entry.name, level + 1);
              if (DPL_SUCCESS != ret)
                return ret;
            }
        }
      
      dpl_closedir(dir_hdl);
      
      if (1 == ls_data->Rflag && level > 0)
        {
          ret = dpl_chdir(ctx, "..");
          if (DPL_SUCCESS != ret)
            return ret;
        }
    }

  return DPL_SUCCESS;
}

int
cmd_ls(int argc,
       char **argv)
{
  char opt;
  int ret;
  int lflag = 0;
  int aflag = 0;
  int Rflag = 0;
  size_t total_size = 0;
  char *path;
  struct ls_data ls_data;

  var_set("status", "1", VAR_CMD_SET, NULL);

  optind = 0;

  while ((opt = getopt(argc, argv, usage_getoptstr(ls_usage))) != -1)
    switch (opt)
      {
      case 'R':
        Rflag = 1;
        break ;
      case 'a':
        aflag = 1;
        break ;
      case 'l':
        lflag = 1;
        break ;
      case '?':
      default:
        usage_help(&ls_cmd);
        return SHELL_CONT;
      }
  argc -= optind;
  argv += optind;

  if (0 == argc)
    path = ".";
  else if (1 == argc)
    path = argv[0];
  else
    {
      usage_help(&ls_cmd);
      return SHELL_CONT;
    }

  memset(&ls_data, 0, sizeof (ls_data));
  ls_data.ctx = ctx;
  ls_data.lflag = lflag;
  ls_data.Rflag = Rflag;
  ls_data.aflag = aflag;
  
  ret = ls_recurse(&ls_data, path, 0);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "ls failure %s (%d)\n", dpl_status_str(ret), ret);
      goto end;
    }
  
  total_size = ls_data.total_size;

  if (1 == lflag)
    {
      if (NULL != ctx->pricing)
        printf("Total %s Price %s\n", dpl_size_str(total_size), dpl_price_storage_str(ctx, total_size));
    }

  var_set("status", "0", VAR_CMD_SET, NULL);

 end:

  return SHELL_CONT;
}
