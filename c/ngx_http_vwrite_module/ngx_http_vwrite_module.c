/**
 * TODO add check_mdb to other query too
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#define IS_FATAL 1
#define IS_ERROR 1
/* #define IS_DEBUG 1 */
/* #define IS_STEP  1 */
/* #define IS_TRACE 1 */


#include  "debug.h"



typedef struct {
  ngx_int_t key_index;
} 
ngx_http_vwrite_loc_conf_t;

static void      *ngx_http_create_main_conf(ngx_conf_t *cf                                );
static void      *ngx_http_create_loc_conf (ngx_conf_t *cf                                );
static char      *ngx_http_merge_loc_conf  (ngx_conf_t *cf, void *parent, void *child     );
static char      *ngx_http_vwrite_command  (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t  ngx_http_vwrite_init_proc(ngx_cycle_t *cycle                            );
static void       ngx_http_vwrite_exit     (ngx_cycle_t *cycle                            );




static ngx_command_t
ngx_http_vwrite_commands[] = {

  { ngx_string("write_variable"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_SIF_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE1,
    ngx_http_vwrite_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_vwrite_module_ctx = {
  NULL,                                         /* preconfiguration */
  NULL,                                         /* postconfiguration */
  NULL,                                         /* create main configuration */
  NULL,                                         /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  ngx_http_create_loc_conf,                     /* create location configuration */
  ngx_http_merge_loc_conf,                      /* merge location configuration */
};

ngx_module_t
ngx_http_vwrite_module = {
  NGX_MODULE_V1,
  &ngx_http_vwrite_module_ctx,                  /* module context */
  ngx_http_vwrite_commands,                     /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  ngx_http_vwrite_init_proc,                    /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  ngx_http_vwrite_exit,                         /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};




  ngx_int_t
ngx_http_vwrite_init_proc(ngx_cycle_t *cycle)
{
  return NGX_OK;
}

  static void
ngx_http_vwrite_exit(ngx_cycle_t *cycle)
{
  return;
}

  static ngx_int_t
make_http_header(ngx_http_request_t *r)
{
  ngx_uint_t        i;
  ngx_table_elt_t  *cc, **ccp;

  r->headers_out.content_type.len = sizeof("text/html") - 1;
  r->headers_out.content_type.data = (u_char *) "text/html";

  ccp = r->headers_out.cache_control.elts;
  if (ccp == NULL) {

    if (ngx_array_init(&r->headers_out.cache_control, r->pool,
	  1, sizeof(ngx_table_elt_t *))
	!= NGX_OK)
    {
      return NGX_ERROR;
    }

    ccp = ngx_array_push(&r->headers_out.cache_control);
    if (ccp == NULL) {
      return NGX_ERROR;
    }

    cc = ngx_list_push(&r->headers_out.headers);
    if (cc == NULL) {
      return NGX_ERROR;
    }

    cc->hash = 1;
    cc->key.len = sizeof("Cache-Control") - 1;
    cc->key.data = (u_char *) "Cache-Control";

    *ccp = cc;

  } else {
    for (i = 1; i < r->headers_out.cache_control.nelts; i++) {
      ccp[i]->hash = 0;
    }

    cc = ccp[0];
  }

  cc->value.len = sizeof("no-cache") - 1;
  cc->value.data = (u_char *) "no-cache";

  return NGX_OK;
}

  static ngx_int_t
ngx_http_vwrite_content_handler(ngx_http_request_t *r) 
{
  fSTEP;
  ngx_http_vwrite_loc_conf_t *lcf;
  ngx_http_variable_value_t  *vv;

  static ngx_chain_t out_chain;
   ngx_buf_t *out_ctl;

  ngx_int_t rc;

  rc = ngx_http_discard_request_body(r);
  if (NGX_OK != rc){
    return rc;
  }

  lcf = (ngx_http_vwrite_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_vwrite_module);

  vv = ngx_http_get_flushed_variable(r, lcf->key_index);

  TRACE("vv:%p", vv);
  dTRACE(vv->len);
  
  if (NULL == vv){
    ngx_log_debug(NGX_LOG_DEBUG, r->connection->log, 0, "variable not found with index : %d", lcf->key_index);
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }


  r->headers_out.content_type_len = sizeof("text/html") - 1;
  r->headers_out.content_type.len = sizeof("text/html") - 1;
  r->headers_out.content_type.data = (u_char *) "text/html";
  r->headers_out.content_length_n = vv->len;
  r->headers_out.status = 200;
  out_ctl = ngx_palloc(r->pool, sizeof(ngx_buf_t));

  out_chain.buf    = out_ctl;
  out_chain.next   = NULL;
  memset(out_ctl, 0, sizeof(ngx_buf_t));
  out_ctl->pos      = vv->data;
  out_ctl->last     = vv->data + vv->len;
  out_ctl->memory   = 1;
  out_ctl->last_buf = 1;

  ngx_http_send_header(r);

  return ngx_http_output_filter(r, &out_chain);
}

  static char *
ngx_http_vwrite_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t   *clcf;
  ngx_http_vwrite_loc_conf_t *lcf   = conf;

  ngx_str_t                  *value;
  ngx_str_t                  vname;

  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_vwrite_content_handler;


  value = cf->args->elts;

  if ('$' != value[1].data[0]) {
    return "found invalid variable name for output";
  }
  vname.data = value[1].data + 1;
  vname.len  = value[1].len - 1;

  lcf->key_index = ngx_http_get_variable_index(cf, &vname);

  ngx_conf_log_error(NGX_LOG_INFO,  cf,  0,  "key_index      : %d",  lcf->key_index      );

  return NGX_CONF_OK;
}

  static void *
ngx_http_create_main_conf (ngx_conf_t *cf)
{
  return NULL;
}

  static void *
ngx_http_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_vwrite_loc_conf_t *lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_vwrite_loc_conf_t));
  memset(lcf, 0, sizeof(ngx_http_vwrite_loc_conf_t));

  lcf->key_index      = NGX_CONF_UNSET;

  return lcf;
}

  static char*
ngx_http_merge_loc_conf (ngx_conf_t *cf, void *parent, void *child)
{

  ngx_http_vwrite_loc_conf_t *p = (ngx_http_vwrite_loc_conf_t *)parent;
  ngx_http_vwrite_loc_conf_t *c = (ngx_http_vwrite_loc_conf_t *)child;

  ngx_conf_merge_value(c->key_index    , p->key_index    , NGX_CONF_UNSET);

  return NGX_CONF_OK;
}
