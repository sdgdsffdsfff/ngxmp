/**
 * random $name start end 
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
  ngx_uint_t start;
  ngx_uint_t end;
} 
ngx_http_rand_vctx_t;


static char      *ngx_http_rand_command      (ngx_conf_t *cf, ngx_command_t *cmd, void *conf); /* deprecated */


static ngx_command_t
ngx_http_rand_commands[] = {

  { ngx_string("random"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE3,
    ngx_http_rand_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_rand_module_ctx = {
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
ngx_http_rand_module = {
  NGX_MODULE_V1,
  &ngx_http_rand_module_ctx,                    /* module context */
  ngx_http_rand_commands,                       /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  NULL,                                         /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  NULL,                                         /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};


  static ngx_int_t
ngx_http_rand_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  fSTEP;

  ngx_http_rand_vctx_t *ctx = (ngx_http_rand_vctx_t *)data;
  long int              ran;


  ran = random();
  ran = ran % (1 + ctx->end - ctx->start) + ctx->start;

  v->data = ngx_palloc(r->pool, 32);
  v->len = sprintf((char*)v->data, "%ld", ran);

  v->valid = 1;

  return NGX_OK;
}

  static char *
ngx_http_rand_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;

  ngx_str_t                *argvalues;
  ngx_str_t                 name;
  ngx_int_t                 startn;
  ngx_int_t                 endn;

  ngx_http_rand_vctx_t *ctx;

  ngx_http_variable_t      *v;
  char*  res;


  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  argvalues = cf->args->elts;

  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[1], &name))) {
    return res;
  }

  if (NGX_ERROR == 
    (startn = ngx_atoi(argvalues[2].data, argvalues[2].len))) {
    return "arg 2 is not a number";
  }

  if (NGX_ERROR == 
    (endn = ngx_atoi(argvalues[3].data, argvalues[3].len))) {
    return "arg 3 is not a number";
  }

  if (startn > endn) {
    ngx_int_t t;
    t = startn;
    startn = endn;
    endn = t;
  }

  v = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
  v->get_handler = ngx_http_rand_get_handler;
  ctx = ngx_palloc(cf->pool, sizeof(ngx_http_rand_vctx_t));
  if (NULL == ctx){
    return NGX_CONF_ERROR;
  }
  ctx->start = startn;
  ctx->end   = endn;

  v->data = (uintptr_t)ctx;

  ngx_conf_log_error(NGX_LOG_INFO,  cf,  0,  "rand num:%V:%d-%d", &name, startn, endn);

  return NGX_CONF_OK;
}

