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

//#define DPRINTF(fmt,...) do { fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)
#define DPRINTF(fmt,...)

#define D_NAMLEN(d)   (strlen ((d)->name))
#define FREE free
#define savestring(x) strcpy ((char *)xmalloc (1 + strlen (x)), (x))
int _rl_match_hidden_files = 1;
#define HIDDEN_FILE(file) 0
int rl_complete_with_tilde_expansion = 0;

void *
do_opendir(char *path)
{
  int ret;
  dpl_ino_t obj_ino;
  void *dir_hdl = NULL;

  ret = dpl_vdir_namei(ctx, path, ctx->cur_bucket, ctx->cur_ino, NULL, &obj_ino, NULL);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "path resolve error\n");
      return NULL;
    }
  
  ret = dpl_vdir_opendir(ctx, ctx->cur_bucket, obj_ino, &dir_hdl);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "dir open error\n");
      return NULL;
    }

  return dir_hdl;
}

dpl_dirent_t *
do_readdir(void *dir_hdl)
{
  static dpl_dirent_t entry;
  int ret;

  if (1 == dpl_vdir_eof(dir_hdl))
    return NULL;
  
  ret = dpl_readdir(dir_hdl, &entry);
  if (DPL_SUCCESS != ret)
    {
      fprintf(stderr, "readdir failed\n");
      return NULL;
    }

  return &entry;
}

void
do_closedir(void *dir_hdl)
{
  dpl_closedir(dir_hdl);
}

char *
file_completion(const char *text,
                int state)
{
  static void *directory = NULL;
  static char *filename = (char *)NULL;
  static char *dirname = (char *)NULL;
  static char *users_dirname = (char *)NULL;
  static int filename_len;
  char *temp;
  int dirlen;
  dpl_dirent_t *entry;

  DPRINTF("text '%s' state %d dirname='%s' dir=%p\n", text, state, dirname, directory);

  /* If we don't have any state, then do some initialization. */
  if (state == 0)
    {
      /* If we were interrupted before closing the directory or reading
	 all of its contents, close it. */
      if (directory)
	{
	  do_closedir (directory);
	  directory = NULL;
	}
      FREE (dirname);
      FREE (filename);
      FREE (users_dirname);

      filename = savestring (text);
      if (*text == 0)
	text = ".";
      dirname = savestring (text);

      temp = strrchr (dirname, '/');

#if defined (__MSDOS__)
      /* special hack for //X/... */
      if (dirname[0] == '/' && dirname[1] == '/' && ISALPHA ((unsigned char)dirname[2]) && dirname[3] == '/')
        temp = strrchr (dirname + 3, '/');
#endif

      if (temp)
	{
	  strcpy (filename, ++temp);
	  *temp = '\0';
	}
#if defined (__MSDOS__)
      /* searches from current directory on the drive */
      else if (ISALPHA ((unsigned char)dirname[0]) && dirname[1] == ':')
        {
          strcpy (filename, dirname + 2);
          dirname[2] = '\0';
        }
#endif
      else
	{
	  dirname[0] = '.';
	  dirname[1] = '\0';
	}

      /* We aren't done yet.  We also support the "~user" syntax. */

      /* Save the version of the directory that the user typed. */
      users_dirname = savestring (dirname);

      if (*dirname == '~')
	{
	  temp = tilde_expand (dirname);
	  free (dirname);
	  dirname = temp;
	}

      if (rl_directory_rewrite_hook)
	(*rl_directory_rewrite_hook) (&dirname);

      /* The directory completion hook should perform any necessary
	 dequoting. */
      if (rl_directory_completion_hook && (*rl_directory_completion_hook) (&dirname))
	{
	  free (users_dirname);
	  users_dirname = savestring (dirname);
	}
      else if (rl_completion_found_quote && rl_filename_dequoting_function)
	{
	  /* delete single and double quotes */
	  temp = (*rl_filename_dequoting_function) (users_dirname, rl_completion_quote_character);
	  free (users_dirname);
	  users_dirname = temp;
	}

      directory = do_opendir (dirname);

      /* Now dequote a non-null filename. */
      if (filename && *filename && rl_completion_found_quote && rl_filename_dequoting_function)
	{
	  /* delete single and double quotes */
	  temp = (*rl_filename_dequoting_function) (filename, rl_completion_quote_character);
	  free (filename);
	  filename = temp;
	}
      filename_len = strlen (filename);

      rl_filename_completion_desired = 1;
    }

  /* At this point we should entertain the possibility of hacking wildcarded
     filenames, like /usr/man/man<WILD>/te<TAB>.  If the directory name
     contains globbing characters, then build an array of directories, and
     then map over that list while completing. */
  /* *** UNIMPLEMENTED *** */

  /* Now that we have some state, we can read the directory. */

  entry = (dpl_dirent_t *)NULL;
  while (directory && (entry = do_readdir (directory)))
    {
      /* Special case for no filename.  If the user has disabled the
         `match-hidden-files' variable, skip filenames beginning with `.'.
	 All other entries except "." and ".." match. */
      if (filename_len == 0)
	{
	  if (_rl_match_hidden_files == 0 && HIDDEN_FILE (entry->name))
	    continue;

	  if (entry->name[0] != '.' ||
	       (entry->name[1] &&
		 (entry->name[1] != '.' || entry->name[2])))
	    break;
	}
      else
	{
	  /* Otherwise, if these match up to the length of filename, then
	     it is a match. */
#if 0
	  if (_rl_completion_case_fold)
	    {
	      if ((_rl_to_lower (entry->name[0]) == _rl_to_lower (filename[0])) &&
		  (((int)D_NAMLEN (entry)) >= filename_len) &&
		  (_rl_strnicmp (filename, entry->name, filename_len) == 0))
		break;
	    }
	  else
#endif
	    {
	      if ((entry->name[0] == filename[0]) &&
		  (((int)D_NAMLEN (entry)) >= filename_len) &&
		  (strncmp (filename, entry->name, filename_len) == 0))
		break;
	    }
	}
    }

  if (entry == 0)
    {
      if (directory)
	{
	  do_closedir (directory);
	  directory = NULL;
	}
      if (dirname)
	{
	  free (dirname);
	  dirname = (char *)NULL;
	}
      if (filename)
	{
	  free (filename);
	  filename = (char *)NULL;
	}
      if (users_dirname)
	{
	  free (users_dirname);
	  users_dirname = (char *)NULL;
	}

      DPRINTF("return null\n");

      return (char *)NULL;
    }
  else
    {

      /* dirname && (strcmp (dirname, ".") != 0) */
      if (dirname && (dirname[0] != '.' || dirname[1]))
	{
	  if (rl_complete_with_tilde_expansion && *users_dirname == '~')
	    {
	      dirlen = strlen (dirname);
	      temp = (char *)xmalloc (2 + dirlen + D_NAMLEN (entry));
	      strcpy (temp, dirname);
	      /* Canonicalization cuts off any final slash present.  We
		 may need to add it back. */
	      if (dirname[dirlen - 1] != '/')
	        {
	          temp[dirlen++] = '/';
	          temp[dirlen] = '\0';
	        }
	    }
	  else
	    {
	      dirlen = strlen (users_dirname);
	      temp = (char *)xmalloc (2 + dirlen + D_NAMLEN (entry));
	      strcpy (temp, users_dirname);
	      /* Make sure that temp has a trailing slash here. */
	      if (users_dirname[dirlen - 1] != '/')
		temp[dirlen++] = '/';
	    }

	  strcpy (temp + dirlen, entry->name);
	}
      else
        {
          temp = savestring (entry->name);
          DPRINTF("return temp='%s'\n", temp);
        }

      return (temp);
    }
}
