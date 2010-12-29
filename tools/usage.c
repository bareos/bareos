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

/**
 * not reentrant
 *
 * @param defs
 *
 * @return
 */
char *
usage_getoptstr(struct usage_def *defs)
{
  static char buf[256];
  struct usage_def *def;
  int i;

  i = 0;
  for (def = defs;def->opt != 0;def++)
    {
      if (USAGE_NO_OPT == def->opt)
        continue ;

      assert(i < sizeof (buf));
      buf[i++] = def->opt;
      if (def->flags & USAGE_PARAM)
        buf[i++] = ':';
    }
  buf[i] = 0;

  return buf;
}

void
usage_help(struct cmd_def *cmd)
{
  char buf[1024];
  struct usage_def *def;

  sprintf(buf, "purpose: %s\nusage: %s", cmd->purpose, cmd->name);
  for (def = cmd->defs;def->opt != 0;def++)
    {
      char optstr[2];

      if (USAGE_NO_OPT == def->opt)
        continue ;

      strcat(buf, " ");

      if (!(def->flags & USAGE_MANDAT))
        strcat(buf, "[");

      strcat(buf, "-");
      optstr[0] = def->opt;
      optstr[1] = 0;
      strcat(buf, optstr);

      if (def->flags & USAGE_PARAM)
        {
          strcat(buf, " ");
          strcat(buf, def->param);
        }

      if (!(def->flags & USAGE_MANDAT))
        strcat(buf, "]");
    }

  for (def = cmd->defs;def->opt != 0;def++)
    {
      if (USAGE_NO_OPT == def->opt)
        {
          strcat(buf, " ");

          if (!(def->flags & USAGE_MANDAT))
            strcat(buf, "[");

          strcat(buf, def->param);

          if (!(def->flags & USAGE_MANDAT))
            strcat(buf, "]");
        }
    }

  strcat(buf, "\n");

  for (def = cmd->defs;def->opt != 0;def++)
    {
      char optstr[2];

      if (USAGE_NO_OPT == def->opt)
        continue ;

      strcat(buf, "  -");
      optstr[0] = def->opt;
      optstr[1] = 0;
      strcat(buf, optstr);

      if (def->flags & USAGE_PARAM)
        {
          strcat(buf, " ");
          strcat(buf, def->param);
        }
      else
        strcat(buf, "\t");

      strcat(buf, "\t");

      strcat(buf, def->long_descr);

      strcat(buf, "\n");
    }

  for (def = cmd->defs;def->opt != 0;def++)
    {
      if (USAGE_NO_OPT == def->opt)
        {
          strcat(buf, "   ");
          strcat(buf, def->param);

          strcat(buf, "\t");

          strcat(buf, def->long_descr);

          strcat(buf, "\n");
        }
    }

  fprintf(stderr, "%s", buf);
}

