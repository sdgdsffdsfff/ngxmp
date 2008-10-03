/**
 * ctime $var format;
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define IS_FATAL 1
#define IS_ERROR 1
#define IS_DEBUG 1
#define IS_STEP  1
#define IS_TRACE 1

#include "debug.h"
#include "ngx_util.h"

typedef struct {
  ngx_str_t format;
} 
ngx_http_time_vctx_t;


static char      *ngx_http_time_command      (ngx_conf_t *cf, ngx_command_t *cmd, void *conf); /* deprecated */


static ngx_command_t
ngx_http_time_commands[] = {

  { ngx_string("ctime"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE2,
    ngx_http_time_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_time_module_ctx = {
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
ngx_http_time_module = {
  NGX_MODULE_V1,
  &ngx_http_time_module_ctx,                    /* module context */
  ngx_http_time_commands,                       /* module directives */
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


#define MAX_TIME_LEN 32

  static ngx_int_t
ngx_http_time_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  fSTEP;

  ngx_http_time_vctx_t *ctx = (ngx_http_time_vctx_t *)data;
  u_char               *buf;

  time_t timestamp;

  buf = ngx_palloc(r->pool, MAX_TIME_LEN);
  if (NULL == buf){
    v->not_found = 1;
    v->data = NULL;
    v->len = 0;
    v->valid = 0;
    return NGX_OK;
  }
  
  TRACE("to output");
  timestamp = time(NULL);
  v->len = strftime((char*)buf,  MAX_TIME_LEN,  (char*)ctx->format.data,  localtime(&timestamp));
  v->data = buf;
  v->valid = 1;


  sTRACE(ctx->format.data);

  return NGX_OK;
}

  static char *
ngx_http_time_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t *clcf;

  ngx_str_t                *argvalues;
  ngx_str_t                 name;
  ngx_str_t                 fmt;

  ngx_http_time_vctx_t *ctx;

  ngx_http_variable_t      *v;
  u_char *buf;
  char*  res;


  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  argvalues = cf->args->elts;

  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[1], &name))) {
    return res;
  }

  fmt.data = argvalues[2].data;
  fmt.len  = argvalues[2].len;

  buf = ngx_palloc(cf->pool, fmt.len + 1); /* zero tail */
  if (NULL == buf){
    return NGX_CONF_ERROR;
  }

  ngx_memcpy(buf, fmt.data, fmt.len);
  buf[fmt.len] = 0;

  v = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);

  v->get_handler = ngx_http_time_get_handler;


  ctx = ngx_palloc(cf->pool, sizeof(ngx_http_time_vctx_t));
  if (NULL == ctx){
    return NGX_CONF_ERROR;
  }

  ctx->format.data = buf;
  ctx->format.len = fmt.len;


  v->data = (uintptr_t)ctx;


  return NGX_CONF_OK;
}

