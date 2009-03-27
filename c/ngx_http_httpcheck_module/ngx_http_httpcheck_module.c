#define    lg(fmt...)  do {fprintf(stderr,""fmt); fprintf(stderr,"\n");} while(0)
/*
 * ngx_http_httpcheck_module.c
 *
 *  Created on: 2009-01-04
 *      Author: zhangyu4@staff.sina.com.cn
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
  ngx_http_upstream_conf_t upstream;

  ngx_int_t i_check_uri;
  ngx_int_t i_redir_uri;
  ngx_int_t i_code;
  ngx_int_t i_res;
} 
ngx_http_httpcheck_loc_conf_t;

typedef struct {

  ngx_http_request_t *request;
  ngx_str_t uid;
  ngx_str_t ip;

  ngx_int_t code;
  ngx_int_t content_len;
  ngx_int_t ischunk;

  char is_anony;
} 
ngx_http_httpcheck_ctx_t;


static ngx_int_t ngx_http_httpcheck_null_gethandler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
ngx_int_t ngx_http_httpcheck_getlen(ngx_http_request_t *r, u_char *s, u_char *e);
ngx_int_t ngx_http_httpcheck_ischunk(ngx_http_request_t *r, u_char *s, u_char *e);
ngx_int_t ngx_http_httpcheck_getcode (ngx_http_request_t *r);

static ngx_int_t  ngx_http_httpcheck_create_request (ngx_http_request_t *r);
static ngx_int_t  ngx_http_httpcheck_reinit_request (ngx_http_request_t *r);
static ngx_int_t  ngx_http_httpcheck_process_header (ngx_http_request_t *r);
static ngx_int_t  ngx_http_httpcheck_filter_init (void *data);
static ngx_int_t  ngx_http_httpcheck_filter (void *data, ssize_t bytes);
static void       ngx_http_httpcheck_abort_request (ngx_http_request_t *r);
static void       ngx_http_httpcheck_finalize_request (ngx_http_request_t *r, ngx_int_t rc);

static void      *ngx_http_httpcheck_create_loc_conf (ngx_conf_t *cf);
static char      *ngx_http_httpcheck_merge_loc_conf (ngx_conf_t *cf, void *parent, void *child);
static ngx_int_t  ngx_http_httpcheck_post_conf (ngx_conf_t *cf);

static char      *ngx_http_httpcheck_pass (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_str_t hc_code = ngx_string("hc_code");
static ngx_str_t hc_res = ngx_string("hc_res");

static ngx_str_t get_head = ngx_string("GET ");
static ngx_str_t uri_loc = ngx_string("/sass/interface/sass_data_input_output.php?");
static ngx_str_t sHTTP = ngx_string(" HTTP/1.1\r\n");
static ngx_str_t sHost = ngx_string("Host: localhost:4194\r\n");

static ngx_command_t ngx_http_httpcheck_commands[] =
{

  { ngx_string("httpcheck_pass"), NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF | NGX_CONF_TAKE3, 
    ngx_http_httpcheck_pass, NGX_HTTP_LOC_CONF_OFFSET, 0,
    NULL },

  ngx_null_command
};

static ngx_http_module_t ngx_http_httpcheck_module_ctx =
{

  NULL,                        /* preconfiguration */
  ngx_http_httpcheck_post_conf,                       /* postconfiguration */

  NULL,                                         /* create main configuration */
  NULL,                                         /* init main configuration */

  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */

  ngx_http_httpcheck_create_loc_conf,                 /* create location configration */
  ngx_http_httpcheck_merge_loc_conf                   /* merge location configration */
};

ngx_module_t ngx_http_httpcheck_module =
{ NGX_MODULE_V1, &ngx_http_httpcheck_module_ctx,      /* module context */
  ngx_http_httpcheck_commands,                        /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  NULL,                                         /* init process */
  NULL ,                                        /* init thread */
  NULL,                                         /* exit thread */
  NULL,                                         /* exit process */
  NULL  ,                                       /* exit master */
  NGX_MODULE_V1_PADDING
};


  static ngx_http_httpcheck_ctx_t *
ngx_http_httpcheck_get_ctx(ngx_http_request_t *r)
{
  ngx_http_httpcheck_ctx_t *ctx;

  ctx = ngx_http_get_module_ctx(r, ngx_http_httpcheck_module);

  if (NULL == ctx) {

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_httpcheck_ctx_t));
    if (ctx == NULL) { return NULL; }

    ngx_http_set_ctx(r, ctx, ngx_http_httpcheck_module);

  }

  return ctx;
}

  static ngx_int_t
ngx_http_httpcheck_handler(ngx_http_request_t *r)
{
  ngx_int_t rc;
  ngx_http_upstream_t *u;
  ngx_http_httpcheck_ctx_t *ctx;
  ngx_http_httpcheck_loc_conf_t *lcf;

  if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  rc = ngx_http_discard_request_body(r);

  if (rc != NGX_OK) {
    return rc;
  }


  lcf = ngx_http_get_module_loc_conf(r, ngx_http_httpcheck_module);

  u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
  if (u == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }



  u->peer.log = r->connection->log;
  u->peer.log_error = NGX_ERROR_ERR;

  u->output.tag = (ngx_buf_tag_t) & ngx_http_httpcheck_module;

  u->conf = &lcf->upstream;

  u->create_request = ngx_http_httpcheck_create_request;
  u->reinit_request = ngx_http_httpcheck_reinit_request;
  u->process_header = ngx_http_httpcheck_process_header;
  u->abort_request = ngx_http_httpcheck_abort_request;
  u->finalize_request = ngx_http_httpcheck_finalize_request;

  r->upstream = u;


  ctx = ngx_http_httpcheck_get_ctx(r);


  ctx->request = r;

  ngx_http_set_ctx(r, ctx, ngx_http_httpcheck_module);

  u->input_filter_init = ngx_http_httpcheck_filter_init;
  u->input_filter = ngx_http_httpcheck_filter;
  u->input_filter_ctx = ctx;

  ngx_http_upstream_init(r);

  return NGX_DONE;
}

  static ngx_int_t
ngx_http_httpcheck_create_request(ngx_http_request_t *r)
{
  size_t len;
  ngx_buf_t *b;
  ngx_chain_t *cl;
  ngx_http_variable_value_t *vv;
  ngx_http_httpcheck_loc_conf_t *lcf;
  ngx_http_httpcheck_ctx_t *ctx;


  lcf = ngx_http_get_module_loc_conf(r, ngx_http_httpcheck_module);
  ctx = ngx_http_httpcheck_get_ctx(r);



  vv = ngx_http_get_indexed_variable(r, lcf->i_check_uri);

  if (vv == NULL || vv->not_found || vv->len == 0) {
    ngx_log_error(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "the \"cookie_uid \" variable is not set");
    return NGX_ERROR;
  }





  len = get_head.len + vv->len + uri_loc.len;


  len += 256;
  b = ngx_create_temp_buf(r->pool, len);
  if (b == NULL) {
    return NGX_ERROR;
  }

  b->last = ngx_copy(b->last, get_head.data, get_head.len);
  b->last = ngx_copy(b->last, vv->data, vv->len);

  b->last += sprintf((char*)b->last, "%.*s", sHTTP.len, sHTTP.data);
  b->last += sprintf((char*)b->last, "%.*s", sHost.len, sHost.data);
  b->last += sprintf((char*)b->last, "\r\n");


  cl = ngx_alloc_chain_link(r->pool);

  if (cl == NULL) {
    return NGX_ERROR;
  }

  lg("request buffer:%.*s", b->last - b->pos, b->pos);
  cl->buf = b;
  cl->next = NULL;


  b->last_buf = 1;
  r->upstream->request_bufs = cl;

  return NGX_OK;
}


  static ngx_int_t
ngx_http_httpcheck_process_header(ngx_http_request_t *r)
{
  ngx_http_httpcheck_ctx_t      *ctx;
  ngx_http_httpcheck_loc_conf_t *lcf;

  ngx_http_upstream_t           *u;
  u_char                        *p;
  ngx_str_t                      line;
  ngx_str_t                      redirect_uri;
  ngx_str_t            args;
  ngx_uint_t           flags;

  ngx_http_variable_value_t     *vv;



  ctx =  ngx_http_httpcheck_get_ctx(r);
  lcf = ngx_http_get_module_loc_conf(r, ngx_http_httpcheck_module);




  u = r->upstream;

  lg("\n\n---upstream buffer:%p,%.*s", u, u->buffer.last - u->buffer.pos,  u->buffer.pos);

  /* find the LF char */
  for (p = u->buffer.pos;
      p < u->buffer.last - 3 && 0 != ngx_strncmp("\r\n\r\n", p, 4);
      p++) ;



  if (p == u->buffer.last -4) {
    return NGX_AGAIN;
  }

  p+=4; /* skip the last \r\n\r\n in header part */


  if (0 == ctx->code){
    ctx->code = ngx_http_httpcheck_getcode(r);
    if (0 == ctx->code){
      lg("failed to get responsed http code");
      return NGX_ERROR;
    }

    lg("got code:%d", ctx->code);
  }


  ctx->ischunk = ngx_http_httpcheck_ischunk(r, u->buffer.pos, p);

  lg("ischunk:%d", ctx->ischunk);



  if (ctx->ischunk) {
    ngx_str_t chunkend = ngx_string("\r\n0\r\n\r\n");
    if (0 != ngx_strncmp(u->buffer.last - chunkend.len, chunkend.data, chunkend.len)) {
      return NGX_AGAIN;
    }

    u->buffer.last -= chunkend.len;

    u_char *s = u->buffer.last - 1;
    for (;s >=p && !(s[0] == '\r' && s[1] == '\n');--s) /* void */;

    s += 2; /* \r\n */

    line.data = s;
    line.len = u->buffer.last - s;

    lg("chunk line:%.*s", line.len, line.data);
  }
  else {
    if (0 == ctx->content_len) {
      ctx->content_len = ngx_http_httpcheck_getlen(r, u->buffer.pos, p);
      if (NGX_ERROR == ctx->content_len){
        lg("filaed to get content-length");
        return NGX_ERROR;
      }
      lg("got content length:%d", ctx->content_len);

    }


    if (ctx->content_len > u->buffer.last - p) {
    lg("need more data");
      return NGX_AGAIN;
    }


    line.data = p;
    line.len = u->buffer.last - p;

    lg("length line:%.*s", line.len, line.data);
  }



  /* remove last useless char */
  while (line.data[line.len - 1] == '\r' || line.data[line.len - 1] == '\n') 
    --line.len;



  /* set code */

  vv = &r->variables[lcf->i_code];
  vv->data = ngx_palloc(r->pool, 32);
  if (NULL == vv->data){
    lg("failled to alloc buffer of size 32");
    return NGX_ERROR;
  }

  vv->len = sprintf((char*)vv->data, "%d", ctx->code);
  vv->not_found = 0;
  vv->valid = 1;
      
  lg("code:%.*s", vv->len, vv->data);


  /* set response text */

  vv = &r->variables[lcf->i_res];
  vv->data = ngx_palloc(r->pool, line.len);
  if (NULL == vv->data){
    lg("failled to alloc buffer");
    return NGX_ERROR;
  }

  ngx_memcpy(vv->data, line.data, line.len);
  vv->len = line.len;
  vv->not_found = 0;
  vv->valid = 1;

  lg("res:[%d]%.*s", vv->len, vv->len, vv->data);




  /* redirect */

  vv = ngx_http_get_indexed_variable(r, lcf->i_redir_uri);
  if (NULL == vv || vv->not_found || !vv->valid){
    lg("redirect uri is not valid");
    return NGX_ERROR;
  }

  redirect_uri.data = vv->data;
  redirect_uri.len = vv->len;




  /* redirect_uri.len = rr_uri.len + r->uri.len; */
  /* redirect_uri.data = ngx_palloc(r->pool,redirect_uri.len); */
  /* if (redirect_uri.data == NULL) { */
    /* lg("redirect_uri err"); */
    /* return NGX_ERROR; */
  /* } */
  /* puri = redirect_uri.data; */
  /* puri += sprintf(puri, "%.*s", rr_uri.len, rr_uri.data); */
  /* puri += sprintf(puri, "%.*s", r->uri.len, r->uri.data); */


  args = r->args;
  flags = 0;

  if (ngx_http_parse_unsafe_uri(r, &redirect_uri, &args, &flags) != NGX_OK) {
    lg("parsing unsafe uri failed");
    ngx_http_finalize_request(r, NGX_HTTP_NOT_FOUND);
    return NGX_ERROR;
  }



  ngx_http_internal_redirect(r, &redirect_uri, &args);

  return NGX_DONE;

}

  ngx_int_t
ngx_http_httpcheck_getcode(ngx_http_request_t *r)
{
  ngx_http_upstream_t *u;
  ngx_buf_t *ub;

  u_char *s;
  u_char *c;


  u = r->upstream;
  ub = &u->buffer;



  /* looking for the 1st ' ' */
  for (c = ub->pos; c < ub->last && *c != ' '; ++c) /* void */;

  if (' ' != *c) {
    return 0;
  }

  /* found */
  s = ++c;

  for (; c < ub->last && *c != ' '; ++c) /* void */;
  return ngx_atoi(s, c-s);

} 

  ngx_int_t
ngx_http_httpcheck_ischunk(ngx_http_request_t *r, u_char *s, u_char *e) 
{
  ngx_http_upstream_t *u;
  ngx_buf_t *ub;

  u_char *c;


  u = r->upstream;
  ub = &u->buffer;

  ngx_str_t cl = ngx_string("Transfer-Encoding: chunked\r\n");
  for (c = s; c < e && ngx_strncmp(cl.data, c, cl.len); ++c) /* void */;

  if (c == e) return 0;
  else return 1;

}

  ngx_int_t
ngx_http_httpcheck_getlen(ngx_http_request_t *r, u_char *s, u_char *e)
{
  ngx_http_upstream_t *u;
  ngx_buf_t *ub;

  u_char *c;
  u_char *d;


  u = r->upstream;
  ub = &u->buffer;


  ngx_str_t cl = ngx_string("Content-Length: ");

  for (c = s; c < e && ngx_strncmp(cl.data, c, cl.len);++c) /* void */;

  if (c == e) {
    return NGX_ERROR;
  }

  c += cl.len;

  ngx_str_t crlf = ngx_string("\r\n");
  for (d = c; 
      d < e && ' ' != *d && ngx_strncmp(crlf.data, d, crlf.len);
      ++d) /* void */;

  return ngx_atoi(c, d-c);
  
}

  static ngx_int_t
ngx_http_httpcheck_reinit_request(ngx_http_request_t *r)
{
  return NGX_OK;
}

  static ngx_int_t
ngx_http_httpcheck_filter_init(void *data)
{

  ngx_http_httpcheck_ctx_t *ctx = data;
  ngx_http_upstream_t *u = ctx->request->upstream;

  u->length = 0;

  lg("filter init"); 
  return NGX_OK;
}



  static ngx_int_t
ngx_http_httpcheck_filter(void *data, ssize_t bytes)
{

  lg("filter----------"); 

  ngx_http_httpcheck_ctx_t *ctx   = data;
  ngx_http_upstream_t *u     = ctx->request->upstream;

  u->length = 0;

  return NGX_DONE;
}

  static void
ngx_http_httpcheck_abort_request(ngx_http_request_t *r)
{
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "abort rr upstream request");
  return;
}

  static void
ngx_http_httpcheck_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "finalize rr upstream request");
  return;
}

  static void *
ngx_http_httpcheck_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_httpcheck_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_httpcheck_loc_conf_t));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }

  {
    conf->i_check_uri  = NGX_CONF_UNSET;
    conf->i_redir_uri = NGX_CONF_UNSET;/* ip index */
    conf->i_code = NGX_CONF_UNSET;
    conf->i_res = NGX_CONF_UNSET;

    conf->upstream.connect_timeout = 60*1000;
    conf->upstream.send_timeout = 60*1000;
    conf->upstream.read_timeout = 60*1000;

    conf->upstream.buffer_size = ngx_pagesize;

    /* the hardcoded values */
    conf->upstream.cyclic_temp_file = 0;
    conf->upstream.buffering = 0;
    conf->upstream.ignore_client_abort = 0;
    conf->upstream.send_lowat = 0;
    conf->upstream.bufs.num = 0;
    conf->upstream.busy_buffers_size = 0;
    conf->upstream.max_temp_file_size = 0;
    conf->upstream.temp_file_write_size = 0;
    conf->upstream.intercept_errors = 1;
    conf->upstream.intercept_404 = 1;
    conf->upstream.pass_request_headers = 0;
    conf->upstream.pass_request_body = 0;


  }

  return conf;
}

  static char *
ngx_http_httpcheck_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_httpcheck_loc_conf_t *prev = parent;
  ngx_http_httpcheck_loc_conf_t *conf = child;

  ngx_conf_merge_value(conf->i_check_uri, prev->i_check_uri, NGX_CONF_UNSET);
  ngx_conf_merge_value(conf->i_redir_uri , prev->i_redir_uri , NGX_CONF_UNSET);
  ngx_conf_merge_value(conf->i_code , prev->i_code, NGX_CONF_UNSET);
  ngx_conf_merge_value(conf->i_res , prev->i_res, NGX_CONF_UNSET);

  ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
      prev->upstream.connect_timeout, 60000);

  ngx_conf_merge_msec_value(conf->upstream.send_timeout,
      prev->upstream.send_timeout, 60000);

  ngx_conf_merge_msec_value(conf->upstream.read_timeout,
      prev->upstream.read_timeout, 60000);

  ngx_conf_merge_size_value(conf->upstream.buffer_size,
      prev->upstream.buffer_size,
      (size_t) ngx_pagesize);


  ngx_conf_merge_bitmask_value(conf->upstream.next_upstream,
      prev->upstream.next_upstream,
      (NGX_CONF_BITMASK_SET
        |NGX_HTTP_UPSTREAM_FT_ERROR
        |NGX_HTTP_UPSTREAM_FT_TIMEOUT));

  if (conf->upstream.next_upstream & NGX_HTTP_UPSTREAM_FT_OFF) {
    conf->upstream.next_upstream = NGX_CONF_BITMASK_SET
        | NGX_HTTP_UPSTREAM_FT_OFF;
  }

  if (conf->upstream.upstream == NULL) {
    conf->upstream.upstream = prev->upstream.upstream;
    conf->upstream.schema = prev->upstream.schema;
  }


  return NGX_CONF_OK;
}


  static ngx_int_t
ngx_http_httpcheck_post_conf(ngx_conf_t *cf)
{
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "post conf");
  return NGX_OK;
}

  static char *
ngx_http_httpcheck_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_httpcheck_loc_conf_t *lcf = conf;

  ngx_str_t *value;
  ngx_str_t v;
  ngx_url_t u;
  ngx_http_variable_t *hv;
  ngx_http_core_loc_conf_t *clcf;




  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);


  value = cf->args->elts;

  ngx_memzero(&u, sizeof(ngx_url_t));

  u.url = value[1];
  u.no_resolve = 1;

  lcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
  if (lcf->upstream.upstream == NULL) {
    return NGX_CONF_ERROR;
  }



  v = value[2];
  --v.len;
  ++v.data;

  lcf->i_check_uri = ngx_http_get_variable_index(cf, &v);
  if (0 == lcf->i_check_uri) {
    return "uri variable is not set";
  }



  v = value[3];
  --v.len;
  ++v.data;

  lcf->i_redir_uri = ngx_http_get_variable_index(cf, &v);
  if (0 == lcf->i_redir_uri) {
    return "redirect uri variable is not set";
  }






  hv = ngx_http_add_variable(cf, &hc_code, NGX_HTTP_VAR_CHANGEABLE);
  if (NULL == hv){
    return "failed to add ht_code variable";
  }

  hv->get_handler = ngx_http_httpcheck_null_gethandler;
  lcf->i_code = ngx_http_get_variable_index(cf, &hc_code);



  hv = ngx_http_add_variable(cf, &hc_res, NGX_HTTP_VAR_CHANGEABLE);
  if (NULL == hv){
    return "failed to add ht_res variable";
  }

  hv->get_handler = ngx_http_httpcheck_null_gethandler;
  lcf->i_res = ngx_http_get_variable_index(cf, &hc_res);



  clcf->handler = ngx_http_httpcheck_handler;

  if (clcf->name.data[clcf->name.len - 1] == '/') {
    clcf->auto_redirect = 1;
  }

  /* ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "httpcheck_pass"); */


  return NGX_CONF_OK;
}

  static ngx_int_t
ngx_http_httpcheck_null_gethandler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
  ngx_http_variable_t          *var;
  ngx_http_core_main_conf_t    *cmcf;


  cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

  var = cmcf->variables.elts;

  ngx_log_error(NGX_LOG_WARN, r->connection->log, 0,
      "using uninitialized \"%V\" variable", &var[data].name);

  *v = ngx_http_variable_null_value;

  return NGX_OK;
}

