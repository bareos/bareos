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
#ifndef __DROPLET_PRICING_H__
#define __DROPLET_PRICING_H__ 1

enum dpl_tok
  {
    DPL_TOK_ERROR = 256,
    DPL_TOK_CONT,
    DPL_TOK_EOF,
    DPL_TOK_NUMBER,
    DPL_TOK_STRING,
    DPL_TOK_REQUESTS,
    DPL_TOK_REQUEST,
    DPL_TOK_DATA,
    DPL_TOK_DATA_TYPE,
    DPL_TOK_DURATION,
  };

enum dpl_lex_state
  {
    DPL_LEX_STATE_INIT,
    DPL_LEX_STATE_BRACKET_COMMENT_BEGIN,
    DPL_LEX_STATE_BRACKET_COMMENT,
    DPL_LEX_STATE_BRACKET_COMMENT_END,
    DPL_LEX_STATE_COMMENT,
    DPL_LEX_STATE_NUMBER,
    DPL_LEX_STATE_IDENTIFIER,
    DPL_LEX_STATE_QUOTED_STRING,
  };

enum dpl_parse_error
  {
    DPL_PARSE_OK,
  };

enum dpl_parse_state
  {
    DPL_PARSE_STATE_UNDEF,
    DPL_PARSE_STATE_SECTION,
    DPL_PARSE_STATE_REQUESTS,
    DPL_PARSE_STATE_REQUEST,
    DPL_PARSE_STATE_REQUEST_COLON,
    DPL_PARSE_STATE_REQUEST_CURRENCY,
    DPL_PARSE_STATE_REQUEST_CURRENCY_NUMBER,
    DPL_PARSE_STATE_REQUEST_SLASH,
    DPL_PARSE_STATE_REQUEST_QUANTITY_NUMBER,
    DPL_PARSE_STATE_REQUEST_SEMICOLON,
    DPL_PARSE_STATE_DATA,
    DPL_PARSE_STATE_DATA_BLOCK,
    DPL_PARSE_STATE_DATA_LIMIT,
    DPL_PARSE_STATE_DATA_LIMIT_COLON,
    DPL_PARSE_STATE_DATA_CURRENCY,
    DPL_PARSE_STATE_DATA_CURRENCY_NUMBER,
    DPL_PARSE_STATE_DATA_SLASH,
    DPL_PARSE_STATE_DATA_QUANTITY_NUMBER,
    DPL_PARSE_STATE_DATA_SLASH2,
    DPL_PARSE_STATE_DATA_DURATION,
    DPL_PARSE_STATE_DATA_SEMICOLON,
  };

/**/

enum dpl_currency
  {
    DPL_CURRENCY_DOLLAR,
  };

enum dpl_request_type
{
  DPL_REQUEST_TYPE_PUT,
  DPL_REQUEST_TYPE_POST,
  DPL_REQUEST_TYPE_GET,
  DPL_REQUEST_TYPE_DELETE,
  DPL_REQUEST_TYPE_HEAD,
  DPL_REQUEST_TYPE_LIST,
  DPL_REQUEST_TYPE_COPY,
  DPL_REQUEST_TYPE_WILDCARD,
};

struct dpl_request_pricing
{
  enum dpl_request_type type;
  unsigned int price;
  enum dpl_currency currency;
  unsigned int quantity;
};

enum dpl_unit
  {
    DPL_UNIT_UNDEFINED,
    DPL_UNIT_KIBI,
    DPL_UNIT_MEBI,
    DPL_UNIT_GIBI,
    DPL_UNIT_TEBI,
    DPL_UNIT_PEBI,
    DPL_UNIT_HEBI,
  };

enum dpl_duration_type
  {
    DPL_DURATION_TYPE_DAY,
    DPL_DURATION_TYPE_WEEK,
    DPL_DURATION_TYPE_MONTH,
    DPL_DURATION_TYPE_QUARTER,
    DPL_DURATION_TYPE_HALF,
    DPL_DURATION_TYPE_YEAR,
  };

struct dpl_data_pricing
{
  size_t limit; /*!< or -1 if '*' */
  double price;
  enum dpl_currency currency;
  size_t quantity;
  enum dpl_duration_type duration;
};

struct dpl_parse_ctx
{
  dpl_ctx_t *ctx;

  int lineno;
  int byteno;

  struct
  {
    enum dpl_lex_state state;
#define DPL_TEXT_SIZE 256
    char text[DPL_TEXT_SIZE];
    int text_pos;
    enum dpl_unit unit;
    double number;
    int string_backslash;
    int unput_byte;
  } lex;

  struct
  {
    enum dpl_parse_state state;
    enum dpl_parse_state return_state;
    struct dpl_request_pricing *cur_request_pricing;
    enum dpl_data_type cur_data_type;
    struct dpl_data_pricing *cur_data_pricing;
  } parse;

};

/* PROTO pricing.c */
/* src/pricing.c */
enum dpl_tok identifier(char *str);
struct dpl_request_pricing *dpl_request_pricing_new(void);
void dpl_request_pricing_free(struct dpl_request_pricing *reqp);
void dpl_vec_request_pricing_free(dpl_vec_t *vec);
struct dpl_data_pricing *dpl_data_pricing_new(void);
void dpl_data_pricing_print(struct dpl_data_pricing *datp);
void dpl_data_pricing_free(struct dpl_data_pricing *datp);
void dpl_vec_data_pricing_free(dpl_vec_t *vec);
enum dpl_request_type dpl_pricing_request_type(char *str);
enum dpl_data_type dpl_pricing_data_type(char *str);
enum dpl_duration_type dpl_pricing_duration_type(char *str);
dpl_status_t dpl_pricing_parse(dpl_ctx_t *ctx, char *path);
dpl_status_t dpl_pricing_load(dpl_ctx_t *ctx);
void dpl_pricing_free(dpl_ctx_t *ctx);
dpl_status_t dpl_log_charged_event(dpl_ctx_t *ctx, char *type, char *subtype, size_t size);
#endif
