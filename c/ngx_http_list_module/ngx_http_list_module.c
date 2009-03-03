/**
 *
 * setlist $list $raw delim;
 *
 * getitem $v $list $i;
 *
 *
 * default delim is " "
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define IS_FATAL 1
#define IS_ERROR 1
#define IS_DEBUG 1
#define IS_STEP  1
#define IS_TRACE 1

#include  "debug.h"
#include  "ngx_util.h"


typedef struct {
  ngx_int_t idx;
  u_char delim;
  ngx_str_t raw;
}
ngx_http_list_ctx_t;

typedef struct {
  ngx_int_t lvi;
  ngx_int_t vi;
  ngx_int_t ii;
  ngx_http_variable_t *list;
  ngx_int_t is_rand;
} 
ngx_http_listitem_ctx_t;

static char *ngx_http_set_list_command    (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_list_getitem_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

#define GET_RAND_ITEM "getranditem"

static ngx_str_t cmd_get_rand_itme = ngx_string(GET_RAND_ITEM);

static ngx_command_t
ngx_http_list_commands[] = {

  { ngx_string("setlist"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE2,
    ngx_http_set_list_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("getitem"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE3,
    ngx_http_list_getitem_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string(GET_RAND_ITEM),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE3,
    ngx_http_list_getitem_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_list_module_ctx = {
  NULL,                                         /* preconfiguration */
  NULL,                                         /* postconfiguration */
  NULL,                                         /* create main configuration */
  NULL,                                         /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};

ngx_module_t
ngx_http_list_module = {
  NGX_MODULE_V1,
  &ngx_http_list_module_ctx,                      /* module context */
  ngx_http_list_commands,                         /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  NULL,                        /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  NULL,                             /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};


  static ngx_int_t
ngx_http_list_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  fSTEP;


  return NGX_OK;
}

  static void
ngx_http_list_set_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  fSTEP;

  ngx_http_list_ctx_t *ctx;



  ctx = (ngx_http_list_ctx_t*)data;

  ctx->raw.data = v->data;
  ctx->raw.len = v->len;


  r->variables[ctx->idx].data = v->data;
  r->variables[ctx->idx].len  = v->len;
  r->variables[ctx->idx].valid  = 1;
  r->variables[ctx->idx].not_found  = 0;

  return;
}

  static ngx_int_t
ngx_http_listitem_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  ngx_http_listitem_ctx_t   *ctx;
  ngx_http_list_ctx_t       *lctx;

  ngx_http_variable_t       *list;
  ngx_str_t                 *raw;

  ngx_http_variable_value_t *iiv;
  ngx_int_t                  ii;

  u_char                     delim = 0;
  ngx_uint_t                  i;
  ngx_uint_t                  j;



  ctx = (ngx_http_listitem_ctx_t*)data;
  iiv = ngx_http_get_indexed_variable(r, ctx->ii);
  if (NULL == iiv) {
    goto invalid;
  }

  ii = ngx_atoi(iiv->data, iiv->len);
  if (NGX_ERROR == ii) {
    ii = 0;
  }

  list = ctx->list;
  lctx = (ngx_http_list_ctx_t*)list->data;

  delim = lctx->delim;

  raw = &lctx->raw;
  if (NULL == raw){
    goto invalid;
  }
  
  if (0 == raw->len) {
    goto empty;
  }

  if (ctx->is_rand) {
    TRACE("is rand");
    long int ran;
    int a, b;
    do {
      ran = random();
      ran = ran % raw->len;
      for (a= ran-1; raw->data[a] != delim && a >= 0; --a);
      ++a;
      for (b= ran; raw->data[b] != delim && b < (int)raw->len; ++b);
    } while (a == b);

    TRACE("[%d-%d]", a, b);

    v->data = &raw->data[a];
    v->len = b-a;
    v->valid = 1;
    v->not_found = 0;
    return NGX_OK;
  }


  TRACE("not rand");


  for (i= 0; i < raw->len && delim == raw->data[i]; ++i);
  if (i == raw->len) {
    goto empty;
  }

  TRACE("start at [%ld]/[%ld]", i, raw->len);
  TRACE("wanted:[%ld]", ii);
  for (j=0; i<raw->len ;++j)
  {
    v->data = &raw->data[i];
    next_tok(raw->data, raw->len, delim, i);
    if ((ngx_uint_t)ii == j) {
      v->len = &raw->data[i] - v->data;
      v->valid = 1;
      v->not_found = 0;
      return NGX_OK;
    }

    while (delim == raw->data[i] && i<raw->len) ++i;
  }

  goto empty;

  return NGX_OK;

invalid:
  TRACE("invalid");
    v->data = NULL;
    v->len = 0;
    v->not_found = 0;
    v->valid = 0;

    return NGX_OK;

empty:
  TRACE("empty");
    v->data = (u_char*)"";
    v->len = 0;
    v->not_found = 0;
    v->valid = 1;

    return NGX_OK;

}

  static char *
ngx_http_set_list_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;

  ngx_str_t                *args;
  ngx_str_t                 listname;
  u_char                    delim;
  ngx_http_variable_t      *v;

  char                     *res;

  ngx_http_list_ctx_t *ctx = NULL;


  if (2 > cf->args->nelts) {
    return "not enough list arguments";
  }

  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  args = cf->args->elts;


  if (NULL != 
    (res = ngx_http_get_var_name_str(&args[1], &listname))) {
    return res;
  }


  if (3 == cf->args->nelts || 0 == args[2].len) {
    delim = ' ';
  }
  else {
    delim = args[2].data[0]; /* first char */
    if (1 != args[2].len) {
      logce("delimiter must be single char but :[%V]", &args[3]);
      return "delimiter too long";
    }
  }


  v = ngx_http_add_variable(cf, &listname, NGX_HTTP_VAR_CHANGEABLE);
  v->get_handler = ngx_http_list_get_handler;
  v->set_handler = ngx_http_list_set_handler;


  ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_list_ctx_t));
  if (NULL == ctx){
    return "alloc ctx failed";
  }
  ctx->delim = delim;

  v->data = (uintptr_t)ctx;


  return NGX_CONF_OK;
}

  static char *
ngx_http_list_getitem_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;

  ngx_str_t                *argvalues;
  ngx_str_t                vname;
  ngx_str_t                lname;
  ngx_str_t                iname;
  ngx_http_variable_t      *v;

  char *res;

  ngx_http_listitem_ctx_t *ctx;

  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  argvalues = cf->args->elts;


  if (4 != cf->args->nelts) {
    return "not enough arguments, need 3";
  }

  if (NULL != (res = ngx_http_get_var_name_str(&argvalues[1], &vname))) {
    return res;
  }

  if (NULL != (res = ngx_http_get_var_name_str(&argvalues[2], &lname))) {
    return res;
  }

  if (NULL != (res = ngx_http_get_var_name_str(&argvalues[3], &iname))) {
    return res;
  }


  v = ngx_http_add_variable(cf, &vname, NGX_HTTP_VAR_CHANGEABLE);
  v->get_handler = ngx_http_listitem_get_handler;


  ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_listitem_ctx_t));
  ctx->vi = ngx_http_get_variable_index(cf, &vname);
  ctx->list = ngx_http_add_variable(cf, &lname, NGX_HTTP_VAR_CHANGEABLE);
  ctx->ii = ngx_http_get_variable_index(cf, &iname);

  if (0 == ngx_strncmp(argvalues[0].data, cmd_get_rand_itme.data, cmd_get_rand_itme.len)) {
    ctx->is_rand = 1;
  }


  v->data = (uintptr_t)ctx;

  return NGX_CONF_OK;
}

