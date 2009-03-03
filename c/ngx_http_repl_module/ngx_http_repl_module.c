/**
 * TODO add check_mdb to other query too
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



#define K_LIST           "list"
#define K_LIST_DELIMITER "list_delimiter"
#define K_REP_PRE        "replace_prefix"
#define K_REP_SUF        "replace_suffix"
#define K_REP_PTN        "replace_pattern"
#define K_REP_DELIMITER  "replace_delimiter"

#define MCK_ALL "c_send_all"
#define MCK_ALL_len (sizeof("c_send_all")-1)


#define MAX_OUT_LEN	      (1024*4)
#define MC_SERVER_NUM            1


#define APPEND_STR(c, s) strcpy((c), (s)); (c) += strlen(c);

typedef struct {

}
ngx_http_repl_main_conf_t;

typedef struct {
  ngx_int_t list_index;
  ngx_int_t list_dlm_index;
  ngx_int_t rep_pre_index;
  ngx_int_t rep_suf_index;
  ngx_int_t rep_ptn_index;
  ngx_int_t rep_dlm_index;
} ngx_http_repl_loc_conf_t;
static void      *ngx_http_create_main_conf(ngx_conf_t *cf                                );
static void      *ngx_http_create_loc_conf (ngx_conf_t *cf                                );
static char      *ngx_http_merge_loc_conf  (ngx_conf_t *cf, void *parent, void *child     );
static char      *ngx_http_repl_command   (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
       ngx_int_t  ngx_http_repl_init_proc (ngx_cycle_t *cycle                            );
static void       ngx_http_repl_exit       (ngx_cycle_t *cycle                            );


/* |+ TODO use pool +| */
/* static u_char out_buf[MAX_OUT_LEN] = "nothing "; */


static ngx_str_t klist     = ngx_string(K_LIST);
static ngx_str_t klist_dlm = ngx_string(K_LIST_DELIMITER);
static ngx_str_t krep_pre  = ngx_string(K_REP_PRE);
static ngx_str_t krep_suf  = ngx_string(K_REP_SUF);
static ngx_str_t krep_ptn  = ngx_string(K_REP_PTN);
static ngx_str_t krep_dlm  = ngx_string(K_REP_DELIMITER);



static ngx_command_t
ngx_http_apis_commands[] = {

  { ngx_string("repl"),
    NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
    ngx_http_repl_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_repl_module_ctx = {
  NULL,                                         /* preconfiguration */
  NULL,                                         /* postconfiguration */
  ngx_http_create_main_conf,                    /* create main configuration */
  NULL,                                         /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  ngx_http_create_loc_conf,                     /* create location configuration */
  ngx_http_merge_loc_conf,                      /* merge location configuration */
};

ngx_module_t
ngx_http_repl_module = {
  NGX_MODULE_V1,
  &ngx_http_repl_module_ctx,                    /* module context */
  ngx_http_apis_commands,                       /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  ngx_http_repl_init_proc,                      /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  ngx_http_repl_exit,                           /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};




  ngx_int_t
ngx_http_repl_init_proc(ngx_cycle_t *cycle)
{
  return NGX_OK;
}


  static void
ngx_http_repl_exit(ngx_cycle_t *cycle)
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
replace_pattern(u_char **oc, u_char *oe, ngx_str_t *li, ngx_http_variable_value_t *ptn)
{
  u_char *pc;
  u_char *pi;
  ngx_str_t part;


  for (pc = pi = ptn->data;
      pi < ptn->data + ptn->len - 1; /* the pattern to replace is %%, thus ignore last char */
      ++pi) {

    if ('%' == pi[0] && '%' == pi[1]) {
      part.data = pc;
      part.len = pi - pc;
      pc = pi + 2;
      ++pi; /* skip '%' */

      if (NULL == (*oc = append_str(*oc, oe, &part))){
        ERROR("output buffer is too small when append pattern part : %d", MAX_OUT_LEN);
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
      }

      if (NULL == (*oc = append_str(*oc, oe, li))){
        ERROR("output buffer is too small when append replacing item : %d", MAX_OUT_LEN);
        return  NGX_HTTP_INTERNAL_SERVER_ERROR;
      }
    }
  }

  ++pi;
  if (pc < pi) {
    part.data = pc;
    part.len = pi - pc;

    if (NULL == (*oc = append_str(*oc, oe, &part))){
      ERROR("output buffer is too small when append pattern part : %d", MAX_OUT_LEN);
      return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
  }

  return NGX_OK;
}

/* handler */
  static ngx_int_t
ngx_http_repl_handler(ngx_http_request_t* r)
{
  ngx_chain_t *out_chain;
  ngx_buf_t *out_ctl;

  u_char *out_buf;

  ngx_http_repl_loc_conf_t  *lcf;
  ngx_http_repl_main_conf_t *mcf;

  ngx_int_t res_code = NGX_OK;  /* default response */

  ngx_str_t li;

  u_char *oc;                                   /* I'm a cursor */
  u_char *oe;
  u_char *c;                                    /* I'm a cursor */
  u_char *e;                                    /* I'm a cursor */

  out_chain = ngx_palloc(r->pool, sizeof(ngx_chain_t));
  out_ctl = ngx_palloc(r->pool, sizeof(ngx_buf_t));
  out_buf = ngx_palloc(r->pool, MAX_OUT_LEN);

  memset(out_chain, 0, sizeof(ngx_chain_t));
  memset(out_ctl, 0, sizeof(ngx_buf_t));

  oc = out_buf;
  oe = out_buf + MAX_OUT_LEN - 1; /* for zero tail */

  lcf = (ngx_http_repl_loc_conf_t*)ngx_http_get_module_loc_conf(r, ngx_http_repl_module);
  mcf = (ngx_http_repl_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_repl_module);

  ngx_http_variable_value_t *vvlist     = ngx_http_get_flushed_variable(r, lcf->list_index    );
  ngx_http_variable_value_t *vvlist_dlm = ngx_http_get_flushed_variable(r, lcf->list_dlm_index);
  ngx_http_variable_value_t *vvrep_pre  = ngx_http_get_flushed_variable(r, lcf->rep_pre_index );
  ngx_http_variable_value_t *vvrep_suf  = ngx_http_get_flushed_variable(r, lcf->rep_suf_index );
  ngx_http_variable_value_t *vvrep_ptn  = ngx_http_get_flushed_variable(r, lcf->rep_ptn_index );
  ngx_http_variable_value_t *vvrep_dlm  = ngx_http_get_flushed_variable(r, lcf->rep_dlm_index );


  if (0 == vvlist->len || 0 == vvlist_dlm->len){
    ERROR("no list or delimiter specified");
    ERROR("list len:%d", vvlist->len);
    ERROR("delim len:%d", vvlist_dlm->len);
    res_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
    goto error_out;
  }

  /* prefix */
  if (NULL == (oc = append_vv(oc, oe, vvrep_pre))) {
    ERROR("output buffer is too small when append replacing prefix : %d", MAX_OUT_LEN);
    res_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
    goto error_out;
  }

  for (c = vvlist->data, e = c; e < vvlist->data + vvlist->len; ++e) {
    if (*e == vvlist_dlm->data[0]) { /* only support multi char delimiter yet */
      li.data = c;
      li.len = e-c;
      c = e + 1;

      if (NGX_OK != (res_code = replace_pattern(&oc, oe, &li, vvrep_ptn))){
        goto error_out;
      }

      if (NULL == (oc = append_vv(oc, oe, vvrep_dlm))) {
        ERROR("output buffer is too small when append replacing delimiter : %d", MAX_OUT_LEN);
        res_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
        goto error_out;
      }
    }
  }

  /* last item */
  if (e > c) {
    li.data = c;
    li.len = e-c;
    if (NGX_OK != (res_code = replace_pattern(&oc, oe, &li, vvrep_ptn))){
      goto error_out;
    }
  }

  /* suffix */
  if (NULL == (oc = append_vv(oc, oe, vvrep_suf))) {
    ERROR("output buffer is too small when append replacing prefix : %d", MAX_OUT_LEN);
    res_code = NGX_HTTP_INTERNAL_SERVER_ERROR;
    goto error_out;
  }

  *oc=0;


  r->headers_out.content_type_len = sizeof("text/html") - 1;
  r->headers_out.content_type.len = sizeof("text/html") - 1;
  r->headers_out.content_type.data = (u_char *) "text/html";
  r->headers_out.content_length_n = oc - out_buf;
  r->headers_out.status = 200;
  out_chain->buf    = out_ctl;
  out_chain->next   = NULL;
  out_ctl->pos      = out_buf;
  out_ctl->last     = oc;
  out_ctl->memory   = 1;

  /**
   * TODO fix me!! in ssi this shoud not be specified, but in main request it
   * should be specified
   */
  if (r == r->main)
  out_ctl->last_buf = 1;

  ngx_http_send_header(r);

  TRACE("sent header done");
  TRACE("what to output:%s", out_buf);

  res_code =  ngx_http_output_filter(r, out_chain);
  /* TRACE("res_code is : %d", res_code); */
  return res_code;

error_out:
  return res_code;
}

  static char *
ngx_http_repl_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_loc_conf_t  *clcf;
  ngx_http_repl_loc_conf_t *lcf = conf;

  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
  clcf->handler = ngx_http_repl_handler;

  lcf->list_index      = ngx_http_get_variable_index(cf, &klist     );
  lcf->list_dlm_index  = ngx_http_get_variable_index(cf, &klist_dlm );
  lcf->rep_pre_index   = ngx_http_get_variable_index(cf, &krep_pre  );
  lcf->rep_suf_index   = ngx_http_get_variable_index(cf, &krep_suf  );
  lcf->rep_ptn_index   = ngx_http_get_variable_index(cf, &krep_ptn  );
  lcf->rep_dlm_index   = ngx_http_get_variable_index(cf, &krep_dlm  );


  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "list_index      : %d",  lcf->list_index      );
  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "list_dlm_index  : %d",  lcf->list_dlm_index  );
  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "rep_pre_index   : %d",  lcf->rep_pre_index   );
  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "rep_suf_index   : %d",  lcf->rep_suf_index   );
  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "rep_ptn_index   : %d",  lcf->rep_ptn_index   );
  ngx_conf_log_error(NGX_LOG_ERR,  cf,  0,  "rep_dlm_index   : %d",  lcf->rep_dlm_index   );

  return NGX_CONF_OK;
}

  static void *
ngx_http_create_main_conf (ngx_conf_t *cf)
{
  STEP("create main config");
  ngx_http_repl_main_conf_t * mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_repl_main_conf_t));
  memset(mcf, 0, sizeof(ngx_http_repl_main_conf_t));

  STEP("create main config done");
  return mcf;
}

  static void *
ngx_http_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_repl_loc_conf_t *lcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_repl_loc_conf_t));
  memset(lcf, 0, sizeof(ngx_http_repl_loc_conf_t));

  lcf->list_index      = NGX_CONF_UNSET;
  lcf->list_dlm_index  = NGX_CONF_UNSET;
  lcf->rep_pre_index   = NGX_CONF_UNSET;
  lcf->rep_suf_index   = NGX_CONF_UNSET;
  lcf->rep_ptn_index   = NGX_CONF_UNSET;
  lcf->rep_dlm_index   = NGX_CONF_UNSET;

  return lcf;
}

  static char*
ngx_http_merge_loc_conf (ngx_conf_t *cf, void *parent, void *child)
{

  ngx_http_repl_loc_conf_t *p = (ngx_http_repl_loc_conf_t *)parent;
  ngx_http_repl_loc_conf_t *c = (ngx_http_repl_loc_conf_t *)child;

  ngx_conf_merge_value(c->list_index    , p->list_index    , NGX_CONF_UNSET);
  ngx_conf_merge_value(c->list_dlm_index, p->list_dlm_index, NGX_CONF_UNSET);
  ngx_conf_merge_value(c->rep_pre_index , p->rep_pre_index , NGX_CONF_UNSET);
  ngx_conf_merge_value(c->rep_suf_index , p->rep_suf_index , NGX_CONF_UNSET);
  ngx_conf_merge_value(c->rep_ptn_index , p->rep_ptn_index , NGX_CONF_UNSET);
  ngx_conf_merge_value(c->rep_dlm_index , p->rep_dlm_index , NGX_CONF_UNSET);

  return NGX_CONF_OK;
}
