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

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/*
 * misc
 */

static const char *
currency_str(enum dpl_currency currency)
{
  switch (currency)
    {
    case DPL_CURRENCY_DOLLAR:
      return "$";
    }

  return "?";
}

static const char *
duration_str(enum dpl_duration_type duration)
{
  switch (duration)
    {
    case DPL_DURATION_TYPE_DAY:
      return "day";
    case DPL_DURATION_TYPE_WEEK:
      return "week";
    case DPL_DURATION_TYPE_MONTH:
      return "month";
    case DPL_DURATION_TYPE_QUARTER:
      return "quarter";
    case DPL_DURATION_TYPE_HALF:
      return "half";
    case DPL_DURATION_TYPE_YEAR:
      return "year";
    }

  return "unknown";
}

/*
 * lexer
 */

static void
lex_reset(struct dpl_parse_ctx *parse_ctx)
{
  parse_ctx->lex.unit = DPL_UNIT_UNDEFINED;
  parse_ctx->lex.number = 0u;
  parse_ctx->lex.text[0] = 0;
  parse_ctx->lex.text_pos = 0;
  parse_ctx->lex.string_backslash = 0;
}

static dpl_status_t
text_add(struct dpl_parse_ctx *parse_ctx,
         int c)
{
  if (parse_ctx->lex.text_pos < (DPL_TEXT_SIZE-1))
    {
      parse_ctx->lex.text[parse_ctx->lex.text_pos++] = c;
      parse_ctx->lex.text[parse_ctx->lex.text_pos] = 0;
      return DPL_SUCCESS;
    }

  return DPL_FAILURE;
}

enum dpl_tok
dpl_pricing_identifier(const char *str)
{
  if (!strcasecmp(str, "requests"))
    return DPL_TOK_REQUESTS;
  else if (!strcasecmp(str, "PUT"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "POST"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "GET"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "DELETE"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "HEAD"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "LIST"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "COPY"))
    return DPL_TOK_REQUEST;
  else if (!strcasecmp(str, "data"))
    return DPL_TOK_DATA;
  else if (!strcasecmp(str, "IN"))
    return DPL_TOK_DATA_TYPE;
  else if (!strcasecmp(str, "OUT"))
    return DPL_TOK_DATA_TYPE;
  else if (!strcasecmp(str, "STORAGE"))
    return DPL_TOK_DATA_TYPE;
  else if (!strcasecmp(str, "day"))
    return DPL_TOK_DURATION;
  else if (!strcasecmp(str, "week"))
    return DPL_TOK_DURATION;
  else if (!strcasecmp(str, "month"))
    return DPL_TOK_DURATION;
  else if (!strcasecmp(str, "quarter"))
    return DPL_TOK_DURATION;
  else if (!strcasecmp(str, "half"))
    return DPL_TOK_DURATION;
  else if (!strcasecmp(str, "year"))
    return DPL_TOK_DURATION;

  return DPL_TOK_ERROR;
}

static int
lex(struct dpl_parse_ctx *parse_ctx,
    int c)
{
  int ret;
  char *p;

#define CHECK_RETURN(Cond, Tok) if (!(Cond)) return (Tok)

  switch (parse_ctx->lex.state)
    {
    case DPL_LEX_STATE_INIT:

      if (EOF == c)
        return DPL_TOK_EOF;
      else if (c == '/')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_BRACKET_COMMENT_BEGIN;
	  return DPL_TOK_CONT;
	}
      else if (c == '#')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_COMMENT;
	  return DPL_TOK_CONT;
	}
      else if (isdigit(c) || '.' == c)
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_NUMBER;
	  ret = text_add(parse_ctx, c);
	  CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	  return DPL_TOK_CONT;
	}
      else if (isalpha(c))
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_IDENTIFIER;
	  ret = text_add(parse_ctx, c);
	  CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	  return DPL_TOK_CONT;
	}
      else if (c == '"')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_QUOTED_STRING;
	  return DPL_TOK_CONT;
	}
      else if (c == '{')
        return '{';
      else if (c == '}')
        return '}';
      else if (c == ';')
        return ';';
      else if (c == ':')
        return ':';
      else if (c == '*')
        return '*';
      else if (c == '$')
        return '$';
      else if (isspace(c))
        return DPL_TOK_CONT;
      else
        return DPL_TOK_ERROR;

    case DPL_LEX_STATE_COMMENT:

      if (c == '\n')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
	}
      else if (c == EOF)
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
	  parse_ctx->lex.unput_byte = 1;
	  return DPL_TOK_CONT;
	}

      return DPL_TOK_CONT;

    case DPL_LEX_STATE_BRACKET_COMMENT_BEGIN:

      if (c == '*')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_BRACKET_COMMENT;
	  return DPL_TOK_CONT;
	}
      else
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
          parse_ctx->lex.unput_byte = 1;
	  return '/';
	}

    case DPL_LEX_STATE_BRACKET_COMMENT:

      if (c == '*')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_BRACKET_COMMENT_END;
	  return DPL_TOK_CONT;
	}
      else if (c == EOF)
	{
	  return DPL_TOK_ERROR;
	}
      else
	{
	  return DPL_TOK_CONT;
	}

    case DPL_LEX_STATE_BRACKET_COMMENT_END:

      if (c == '/')
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
	  return DPL_TOK_CONT;
	}
      else if (c == EOF)
	{
	  return DPL_TOK_ERROR;
	}
      else
	{
	  parse_ctx->lex.state = DPL_LEX_STATE_BRACKET_COMMENT;
	  parse_ctx->lex.unput_byte = 1;
	  return DPL_TOK_CONT;
	}

    case DPL_LEX_STATE_NUMBER:

      if (isdigit(c) || '.' == c)
	{
	  ret = text_add(parse_ctx, c);
	  CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	  return DPL_TOK_CONT;
	}
      else if ((p = index("KMGTPH", c)))
	{
	  if (*p == 'K')
	    parse_ctx->lex.unit = DPL_UNIT_KIBI;
	  else if (*p == 'M')
	    parse_ctx->lex.unit = DPL_UNIT_MEBI;
	  else if (*p == 'G')
	    parse_ctx->lex.unit = DPL_UNIT_GIBI;
	  else if (*p == 'T')
	    parse_ctx->lex.unit = DPL_UNIT_TEBI;
	  else if (*p == 'P')
	    parse_ctx->lex.unit = DPL_UNIT_PEBI;
	  else if (*p == 'H')
	    parse_ctx->lex.unit = DPL_UNIT_HEBI;
	  else
	    return DPL_TOK_ERROR;
	}
      else
	{
	  parse_ctx->lex.unit = DPL_UNIT_UNDEFINED;
	  parse_ctx->lex.unput_byte = 1;
	}

      parse_ctx->lex.number = atof(parse_ctx->lex.text);
      parse_ctx->lex.state = DPL_LEX_STATE_INIT;

      return DPL_TOK_NUMBER;

    case DPL_LEX_STATE_IDENTIFIER:

      if (isalnum(c) || c == '_' || c == '.')
	{
	  ret = text_add(parse_ctx, c);
	  CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	  return DPL_TOK_CONT;
	}
      else
	{
	  enum dpl_tok tok;

	  tok = dpl_pricing_identifier(parse_ctx->lex.text);

	  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
	  parse_ctx->lex.unput_byte = 1;

	  return tok;
	}

    case DPL_LEX_STATE_QUOTED_STRING:

      if (parse_ctx->lex.string_backslash)
	{
	  if (c == EOF)
	    {
	      return DPL_TOK_ERROR;
	    }

	  parse_ctx->lex.string_backslash = 0;
	  ret = text_add(parse_ctx, c);
	  CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	  return DPL_TOK_CONT;
	}
      else
	{
	  if (c == '\\')
	    {
	      parse_ctx->lex.string_backslash = 1;
	      return DPL_TOK_CONT;
	    }
	  else if (c == '"')
	    {
              parse_ctx->lex.state = DPL_LEX_STATE_INIT;
              return DPL_TOK_STRING;
	    }
	  else if (c == EOF)
	    {
	      return DPL_TOK_ERROR;
	    }
	  else
	    {
	      ret = text_add(parse_ctx, c);
	      CHECK_RETURN(DPL_SUCCESS == ret, DPL_TOK_ERROR);
	      return DPL_TOK_CONT;
	    }
	}

    default:
      // Error!
      break;
    }

  CHECK_RETURN(0, DPL_TOK_ERROR);

  return DPL_TOK_ERROR;
}

/*
 * parser
 */

struct dpl_request_pricing *
dpl_request_pricing_new()
{
  struct dpl_request_pricing *reqp;

  reqp = malloc(sizeof (*reqp));
  if (NULL == reqp)
    return NULL;

  memset(reqp, 0, sizeof (*reqp));

  return reqp;
}

void
dpl_request_pricing_free(struct dpl_request_pricing *reqp)
{
  free(reqp);
}

void
dpl_vec_request_pricing_free(dpl_vec_t *vec)
{
  int i;

  if (! vec)
    return;

  for (i = 0;i < vec->n_items;i++)
    dpl_request_pricing_free((struct dpl_request_pricing *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

struct dpl_data_pricing *
dpl_data_pricing_new()
{
  struct dpl_data_pricing *datp;

  datp = malloc(sizeof (*datp));
  if (NULL == datp)
    return NULL;

  memset(datp, 0, sizeof (*datp));

  return datp;
}

void
dpl_data_pricing_print(struct dpl_data_pricing *datp)
{
  printf("%llu: %s%.03f/%llu/%s\n",
         (long long unsigned) datp->limit, currency_str(datp->currency),
         datp->price, (long long unsigned) datp->quantity, duration_str(datp->duration));
}

void
dpl_data_pricing_free(struct dpl_data_pricing *datp)
{
  free(datp);
}

void
dpl_vec_data_pricing_free(dpl_vec_t *vec)
{
  int i;

  if (! vec)
    return;

  for (i = 0;i < vec->n_items;i++)
    dpl_data_pricing_free((struct dpl_data_pricing *) dpl_vec_get(vec, i));
  dpl_vec_free(vec);
}

enum dpl_request_type
dpl_pricing_request_type(const char *str)
{
  if (!strcasecmp(str, "PUT"))
    return DPL_REQUEST_TYPE_PUT;
  else if (!strcasecmp(str, "POST"))
    return DPL_REQUEST_TYPE_POST;
  else if (!strcasecmp(str, "GET"))
    return DPL_REQUEST_TYPE_GET;
  else if (!strcasecmp(str, "DELETE"))
    return DPL_REQUEST_TYPE_DELETE;
  else if (!strcasecmp(str, "HEAD"))
    return DPL_REQUEST_TYPE_HEAD;
  else if (!strcasecmp(str, "LIST"))
    return DPL_REQUEST_TYPE_LIST;
  else if (!strcasecmp(str, "COPY"))
    return DPL_REQUEST_TYPE_COPY;

  assert(0);
}

enum dpl_data_type
dpl_pricing_data_type(const char *str)
{
  if (!strcasecmp(str, "IN"))
    return DPL_DATA_TYPE_IN;
  else if (!strcasecmp(str, "OUT"))
    return DPL_DATA_TYPE_OUT;
  else if (!strcasecmp(str, "STORAGE"))
    return DPL_DATA_TYPE_STORAGE;

  assert(0);
}

enum dpl_duration_type
dpl_pricing_duration_type(const char *str)
{
  if (!strcasecmp(str, "DAY"))
    return DPL_DURATION_TYPE_DAY;
  else if (!strcasecmp(str, "WEEK"))
    return DPL_DURATION_TYPE_WEEK;
  else if (!strcasecmp(str, "MONTH"))
    return DPL_DURATION_TYPE_MONTH;
  else if (!strcasecmp(str, "QUARTER"))
    return DPL_DURATION_TYPE_QUARTER;
  else if (!strcasecmp(str, "HALF"))
    return DPL_DURATION_TYPE_HALF;
  else if (!strcasecmp(str, "YEAR"))
    return DPL_DURATION_TYPE_YEAR;

  assert(0);
}

static unsigned long long
multiplicator(enum dpl_unit unit)
{
  switch (unit)
    {
    case DPL_UNIT_UNDEFINED:
      return 1;
    case DPL_UNIT_KIBI:
      return 1000LL;
    case DPL_UNIT_MEBI:
      return 1000LL*1000LL;
    case DPL_UNIT_GIBI:
      return 1000LL*1000LL*1000LL;
    case DPL_UNIT_TEBI:
      return 1000LL*1000LL*1000LL*1000LL;
    case DPL_UNIT_PEBI:
      return 1000LL*1000LL*1000LL*1000LL*1000LL;
    case DPL_UNIT_HEBI:
      return 1000LL*1000LL*1000LL*1000LL*1000LL*1000LL;
    }

  return 0LL;
}

static dpl_status_t
parse_buf(struct dpl_parse_ctx *parse_ctx,
          const char *buf,
          int len,
          int eof)
{
  int i;
  int ret;

  i = 0;
  while (1)
    {
      int c;
      enum dpl_tok tok;

      if (len == i)
        {
          if (eof)
            c = EOF;
          else
            return DPL_SUCCESS;
        }
      else
        c = buf[i];

      tok = lex(parse_ctx, c);

      DPRINTF("%c tok=%d %s\n", c, tok, parse_ctx->lex.text);

      if (DPL_TOK_ERROR == tok)
        return DPL_EINVAL;

      if (DPL_TOK_CONT == tok)
        goto next_byte;

      //unput_tok:

      switch (parse_ctx->parse.state)
        {
        case DPL_PARSE_STATE_UNDEF:
          assert(0);

        case DPL_PARSE_STATE_SECTION:

          if (DPL_TOK_EOF == tok)
            return DPL_SUCCESS;
          else if (DPL_TOK_REQUESTS == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUESTS;
              break ;
            }
          else if (DPL_TOK_DATA == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA;
              break ;
            }
          else
            return DPL_EINVAL;

          break ;

        case DPL_PARSE_STATE_REQUESTS:

          if ('{' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST:

          if (DPL_TOK_REQUEST == tok || '*' == tok)
            {
              parse_ctx->parse.cur_request_pricing = dpl_request_pricing_new();
              if (NULL == parse_ctx->parse.cur_request_pricing)
                return DPL_ENOMEM;
              if ('*' == tok)
                parse_ctx->parse.cur_request_pricing->type = DPL_REQUEST_TYPE_WILDCARD;
              else
                parse_ctx->parse.cur_request_pricing->type = dpl_pricing_request_type(parse_ctx->lex.text);

              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_COLON;
              break ;
            }
          else if ('}' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_SECTION;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_COLON:

          if (':' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_CURRENCY;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_CURRENCY:

          if ('$' == tok)
            {
              parse_ctx->parse.cur_request_pricing->currency = DPL_CURRENCY_DOLLAR;
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_CURRENCY_NUMBER;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_CURRENCY_NUMBER:

          if (DPL_TOK_NUMBER == tok)
            {
              parse_ctx->parse.cur_request_pricing->price = parse_ctx->lex.number;
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_SLASH;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_SLASH:

          if ('/' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_QUANTITY_NUMBER;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_QUANTITY_NUMBER:

          if (DPL_TOK_NUMBER == tok)
            {
              parse_ctx->parse.cur_request_pricing->quantity = parse_ctx->lex.number;
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST_SEMICOLON;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_REQUEST_SEMICOLON:

          if (';' == tok)
            {
              ret = dpl_vec_add(parse_ctx->ctx->request_pricing, parse_ctx->parse.cur_request_pricing);
              if (DPL_SUCCESS != ret)
                return DPL_ENOMEM;
              parse_ctx->parse.cur_request_pricing = NULL;
              parse_ctx->parse.state = DPL_PARSE_STATE_REQUEST;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA:

          if (DPL_TOK_DATA_TYPE == tok)
            {
              parse_ctx->parse.cur_data_type = dpl_pricing_data_type(parse_ctx->lex.text);
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_BLOCK;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_BLOCK:

          if ('{' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_LIMIT;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_LIMIT:

          if (DPL_TOK_NUMBER == tok || '*' == tok)
            {
              parse_ctx->parse.cur_data_pricing = dpl_data_pricing_new();
              if (NULL == parse_ctx->parse.cur_data_pricing)
                return DPL_ENOMEM;
              if ('*' == tok)
                {
                  parse_ctx->parse.cur_data_pricing->limit = (size_t) -1;
                }
              else
                {
                  parse_ctx->parse.cur_data_pricing->limit = parse_ctx->lex.number * multiplicator(parse_ctx->lex.unit);
                }
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_LIMIT_COLON;
              break ;
            }
          else if ('}' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_SECTION;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_LIMIT_COLON:

          if (':' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_CURRENCY;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_CURRENCY:

          if ('$' == tok)
            {
              parse_ctx->parse.cur_data_pricing->currency = DPL_CURRENCY_DOLLAR;
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_CURRENCY_NUMBER;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_CURRENCY_NUMBER:

          if (DPL_TOK_NUMBER == tok)
            {
              parse_ctx->parse.cur_data_pricing->price = parse_ctx->lex.number;
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_SLASH;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_SLASH:

          if ('/' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_QUANTITY_NUMBER;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_QUANTITY_NUMBER:

          if (DPL_TOK_NUMBER == tok)
            {
              parse_ctx->parse.cur_data_pricing->quantity = parse_ctx->lex.number * multiplicator(parse_ctx->lex.unit);
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_SLASH2;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_SLASH2:

          if ('/' == tok)
            {
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_DURATION;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_DURATION:

          if (DPL_TOK_DURATION == tok)
            {
              parse_ctx->parse.cur_data_pricing->duration = dpl_pricing_duration_type(parse_ctx->lex.text);
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_SEMICOLON;
              break ;
            }
          else
            return DPL_EINVAL;

        case DPL_PARSE_STATE_DATA_SEMICOLON:

          if (';' == tok)
            {
              ret = dpl_vec_add(parse_ctx->ctx->data_pricing[parse_ctx->parse.cur_data_type], parse_ctx->parse.cur_data_pricing);
              if (DPL_SUCCESS != ret)
                return DPL_ENOMEM;
              parse_ctx->parse.cur_data_pricing = NULL;
              parse_ctx->parse.state = DPL_PARSE_STATE_DATA_LIMIT;
              break ;
            }
          else
            return DPL_EINVAL;

        }

      lex_reset(parse_ctx);

    next_byte:
      if (parse_ctx->lex.unput_byte)
        parse_ctx->lex.unput_byte = 0;
      else
        {
          if ('\n' == buf[i])
            {
              parse_ctx->lineno++;
              parse_ctx->byteno = 1;
            }
          else
            {
              parse_ctx->byteno++;
            }
          i++;
        }
    }

  return DPL_SUCCESS;
}

static void
parse_ctx_init(struct dpl_parse_ctx *parse_ctx,
               dpl_ctx_t *ctx)
{
  memset(parse_ctx, 0, sizeof (*parse_ctx));

  parse_ctx->ctx = ctx;

  parse_ctx->lineno = parse_ctx->byteno = 1;

  parse_ctx->lex.state = DPL_LEX_STATE_INIT;
  parse_ctx->lex.unput_byte = 0;
  lex_reset(parse_ctx);

  parse_ctx->parse.state = DPL_PARSE_STATE_SECTION;
  parse_ctx->parse.return_state = DPL_PARSE_STATE_UNDEF;
}

static void
buf_point_error(char *buf,
                int len,
                int lineno,
                int byteno)
{
  int nl, i;

  fprintf(stderr, "error line %d:\n", lineno);

  nl = 1;
  for (i = 0;i < len;i++)
    {
      if (nl == lineno)
        putchar(buf[i]);

      if (buf[i] == '\n')
        nl++;
    }

  printf("\n");
  for (i = 1; i < byteno;i++)
    {
      putchar(' ');
    }
  printf("^\n");
}

static struct dpl_parse_ctx *
parse_ctx_new(dpl_ctx_t *ctx)
{
  struct dpl_parse_ctx *parse_ctx;

  parse_ctx = malloc(sizeof (*parse_ctx));
  if (NULL == parse_ctx)
    return NULL;

  parse_ctx_init(parse_ctx, ctx);

  return parse_ctx;
}

static void
parse_ctx_free(struct dpl_parse_ctx *parse_ctx)
{
  free(parse_ctx);
}

/**/

dpl_status_t
dpl_pricing_parse(dpl_ctx_t *ctx,
                  const char *path)
{
  struct dpl_parse_ctx *parse_ctx = NULL;
  char buf[4096];
  ssize_t cc;
  int fd = -1;
  int ret, ret2;

  parse_ctx = parse_ctx_new(ctx);
  if (NULL == parse_ctx)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  fd = open(path, O_RDONLY);
  if (-1 == fd)
    {
      fprintf(stderr, "droplet: error opening %s\n", path);
      ret = DPL_FAILURE;
      goto end;
    }

  while (1)
    {
      cc = read(fd, buf, sizeof (buf));
      if (0 == cc)
        {
          ret2 = parse_buf(parse_ctx, NULL, 0, 1);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_FAILURE;
              goto end;
            }
          break ;
        }

      if (-1 == cc)
        {
          ret = DPL_FAILURE;
          goto end;
        }

      ret2 = parse_buf(parse_ctx, buf, cc, 0);
      if (DPL_SUCCESS != ret2)
        {
          buf_point_error(buf, cc, parse_ctx->lineno, parse_ctx->byteno);
          ret = DPL_FAILURE;
          goto end;
        }
    }

  ret = DPL_SUCCESS;

 end:

  if (parse_ctx)
    parse_ctx_free(parse_ctx);

  if (-1 != fd)
    (void) close(fd);

  return ret;
}

dpl_status_t
dpl_pricing_load(dpl_ctx_t *ctx)
{
  int ret, ret2;
  char path[1024];
  int i;

  ctx->request_pricing = dpl_vec_new(2, 2);
  if (NULL == ctx->request_pricing)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  for (i = 0;i < DPL_N_DATA_TYPE;i++)
    {
      ctx->data_pricing[i] = dpl_vec_new(2, 2);
      if (NULL == ctx->data_pricing[i])
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  snprintf(path, sizeof (path), "%s/%s.pricing", ctx->droplet_dir, ctx->pricing);

  ret2 = dpl_pricing_parse(ctx, path);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

void
dpl_pricing_free(dpl_ctx_t *ctx)
{
  int i;

  for (i = 0;i < DPL_N_DATA_TYPE;i++)
    dpl_vec_data_pricing_free(ctx->data_pricing[i]);

  dpl_vec_request_pricing_free(ctx->request_pricing);
}

/**
 *
 *
 * @param type e.g. "request", or "data"
 * @param subtype e.g. "LIST" or "IN", "OUT", "STORAGE"
 * @param size data size
 *
 * @return
 */
dpl_status_t
dpl_log_request(dpl_ctx_t *ctx,
                const char *type,
                const char *subtype,
                size_t size)
{
  time_t t;

  if (NULL != ctx->event_log)
    {
      t = time(0);
      fprintf(ctx->event_log, "%ld;%s;%s;%llu\n", t, type, subtype, (unsigned long long) size);
    }
  return DPL_SUCCESS;
}

