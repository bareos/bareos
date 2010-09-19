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

#include "dpl_sh.h"

static char sargmem[SHELL_MAX_ARGV][SHELL_MAX_ARG_LEN];
static char *sargv[SHELL_MAX_ARGV + 1];
static int  sargc;

/**/

const char *shell_err_strings[] =
  {
    "No error",
    "too many args",
    "argument is too long",
    "double quote error",
    NULL,
  };

const char *
shell_error_str(enum shell_error err)
{
  return (shell_err_strings[err]);
}

static struct cmd_def **g_cmd_defs = NULL;

/** 
 * for completion
 * 
 * @param defs 
 */
void 
shell_install_cmd_defs(struct cmd_def **defs)
{
  g_cmd_defs = defs;
}

static char *
command_generator(const char *text,
                  int state)
{
  static int list_index, len;
  char *name;
  struct cmd_def *def;

  if (!state)
    {
      list_index = 0;
      len = strlen (text);
    }

  while (g_cmd_defs[list_index])
    {
      def = g_cmd_defs[list_index];
      name = def->name;

      list_index++;

      if (strncmp(name, text, len) == 0)
        return xstrdup(name);
    }

  /* If no names matched, then return NULL. */
  return ((char *) NULL);
}

char **
shell_completion(const char *text,
                 int start,
                 int end)
{
  char **matches;
  
  if (NULL == g_cmd_defs)
    return NULL;

  matches = (char **)NULL;

  if (start == 0)
    matches = rl_completion_matches(text, command_generator);

  return (matches);
}

int
shell_do_cmd(struct cmd_def **defs,
             int argc,
             char **argv)
{
  struct cmd_def *def, *tmp;
  int ret, len, found;
  char *p;
  int i;

  if (argc == 0)
    return SHELL_CONT;

  if ('!' == argv[0][0])
    {
      pid_t pid;

      pid = fork();
      if (-1 == pid)
        {
          perror("fork");
          return SHELL_CONT;
        }
      if (0 == pid)
        {
          argv[0]++;

          ret = execvp(argv[0], argv);
          if (-1 == ret)
            {
              perror("execvp");
              exit(1);
            }
        }
      else
        {
        retry:
          ret = wait(0);
          if (-1 == ret)
            {
              if (EINTR == errno)
                goto retry;
              else
                {
                  perror("wait");
                  exit(1);
                }
            }
        }
      return SHELL_CONT;
    }
  else if (NULL != (p = index(argv[0], '=')))
    {
      *p++ = 0;
      var_set(argv[0], p, VAR_CMD_SET, NULL);
      return SHELL_CONT;
    }
  else if (argc == 1 && (NULL != (p = index(argv[0], ':'))))
    {
      *p = 0;
      free(ctx->cur_bucket);
      ctx->cur_bucket = xstrdup(argv[0]);
      ctx->cur_ino = DPL_ROOT_INO;
      return SHELL_CONT;
    }

  len = strlen(argv[0]);

  def = NULL;
  found = 0;
  for (i = 0;defs[i];i++)
    {
      tmp = defs[i];
      //printf("'%s' '%s'\n", argv[0], tmp->name);
      if (!strcmp(argv[0], tmp->name))
        {
          found = 1;
          def = tmp;
          break ;
        }
      else if (!strncmp(argv[0], tmp->name, len))
	{
          if (1 == found)
            {
              fprintf(stderr, "ambiguous command: %s\n", argv[0]);
              return SHELL_CONT;
            }
          found = 1;
          def = tmp;
	}
    }

  if (0 == found)
    {
      fprintf(stderr, "cmd %s: not found\n", argv[0]);
      return SHELL_CONT;
    }

  ret = def->func(argc, argv);
  return ret;
}

/**
 * parse a string into multiple commands. everything is done
 * static. understands comments (sharp sign), double quotes, semicolon.
 * ignore whitspaces
 *
 * @param str the input string
 * @param cmd the callback
 * @param errp the error code
 *
 * @return 0 if OK, -1 on failure
 */
int	
shell_parse(struct cmd_def **defs,
            char *str, 
            enum shell_error *errp)
{
  int i, pos, skip_ws, comment, dblquote, ret;
  char c;

  i = sargc = pos = 0;
  comment = dblquote = 0;
  skip_ws = 1;

  while (1)
    {
      c = *str;

      switch (c)
        {
        case '"':
          if (dblquote)
            {
              dblquote = 0;
              if (0 == pos)
                {
                  skip_ws = 0;
                  c = 0;
                  goto store_it;
                }
            }
          else
            dblquote = 1;
          break ;

        case '#':
          comment = 1;
          break ;

	case '\0':
        case ';':
        case '\n':

          if (c == ';' && 1 == dblquote)
            {
              skip_ws = 0;
              goto store_it;
            }

	  sargv[sargc] = &sargmem[sargc][0];
	  
	  if (!skip_ws) 
	    sargc++;
	  
	  sargv[sargc] = NULL;
	  
	  if (dblquote)
	    {
	      if (errp)
		*errp = SHELL_ERROR_DBL_QUOTE;
	      return -1;
	    }
	  
	  if ((ret = shell_do_cmd(defs, sargc, sargv)) != SHELL_CONT)
	    return ret;

          memset(sargmem, 0, sizeof (sargmem));

	  sargc = pos = 0;
	  comment = dblquote = 0;
	  skip_ws = 1;

	  if (c == '\0')
	    return 0;

          break ;

        case ' ':
        case '\t':

          if (comment)
            break ;

          if (dblquote)
            {
              skip_ws = 0;
              goto store_it;
            }

          if (!skip_ws)
            {
              if ((sargc + 1) < SHELL_MAX_ARGV)
                {
                  sargv[sargc] = &sargmem[sargc][0];
                  sargc++;
                  sargv[sargc] = NULL;
                  pos = 0;
                }
              else
                {
                  if (errp)
		    *errp = SHELL_ERROR_TOO_MANY_ARGS;
		  return -1;
                }
              skip_ws = 1;
            }
          break ;
          
        default:

          if (comment)
            break ;
          skip_ws = 0;

        store_it:
          if ((pos + 1) < SHELL_MAX_ARG_LEN)
            {
              sargmem[sargc][pos++] = c;
              sargmem[sargc][pos] = 0;
            }
          else
            {
              if (errp)
		*errp = SHELL_ERROR_ARG_TOO_LONG;
	      return -1;
            }
        }
      str++;
    }
  
  return 0;
}

void
shell_do(struct cmd_def **defs)
{
  using_history();

  while (1)
    {
      char      *line;
      char prompt[256];

      snprintf(prompt, sizeof (prompt), "%s:/%s> ", ctx->cur_bucket ? ctx->cur_bucket : "", ctx->cur_ino.key);
      
      if ((line = readline(prompt)))
        {
          enum shell_error shell_err;
          int ret;
          
          ret = shell_parse(defs, line, &shell_err);
          if (ret == SHELL_EPARSE)
            {
              fprintf(stderr, 
                      "parsing: %s\n", shell_error_str(shell_err));
            }
          else if (ret == SHELL_RETURN)
            {
              return ;
            }
          if (strcmp(line, ""))
            add_history(line);
          free(line);
        }
      else
        {
          return ;
        }
    }
}
