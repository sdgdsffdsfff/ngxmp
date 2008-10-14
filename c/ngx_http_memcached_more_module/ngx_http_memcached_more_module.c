/*
 * Copyright (C) Igor Sysoev
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_event.h>
#include <ngx_http.h>

#define log(r, args...)	ngx_log_debug(NGX_LOG_DEBUG_HTTP, (r)->connection->log, 0, ":::"args)
#define logd(r, d)	log(r, #d"=%d", (d))
#define logs(r, d)	log(r, #d"=%V", (d))
#define logv(r, d)	log(r, #d"=%v", (d))
#define logcs(r, d)	log(r, #d"=%s", (d))

typedef struct
{
  ngx_http_upstream_conf_t upstream;
  ngx_int_t prefix_index;
  ngx_int_t suffix_index;
  ngx_int_t key_index;
  ngx_int_t value_index;
} ngx_http_mcm_loc_conf_t;

typedef struct
{
  size_t rest;
  ngx_http_request_t *request;
  ngx_str_t key;
  ngx_uint_t times;
  ngx_str_t pre;
} ngx_http_mcm_ctx_t;

typedef struct mc_res_s
{
  ngx_str_t head;
  ngx_str_t key;
  ngx_str_t flag;
  ngx_str_t vlen;
  ngx_str_t val;
  ngx_str_t end;
} mc_res_t;

/* memcached response error  */
#define MC_PARSE_NO_HEAD -1
#define MC_PARSE_NO_BODY -2

static ngx_int_t  ngx_http_mcm_create_request                   (ngx_http_request_t *r                                              );
static ngx_int_t  ngx_http_mcm_reinit_request                   (ngx_http_request_t *r                                              );
static ngx_int_t  ngx_http_mcm_process_header                   (ngx_http_request_t *r                                              );
static ngx_int_t  ngx_http_mcm_filter_init                      (void *data                                                         );
static ngx_int_t  ngx_http_mcm_filter                           (void *data, ssize_t bytes                                          );
static void       ngx_http_mcm_abort_request                    (ngx_http_request_t *r                                              );
static void       ngx_http_mcm_finalize_request                 (ngx_http_request_t *r, ngx_int_t rc                                );
 
static void      *ngx_http_mcm_create_loc_conf                  (ngx_conf_t *cf                                                     );
static char      *ngx_http_mcm_merge_loc_conf                   (ngx_conf_t *cf, void *parent, void *child                          );
static ngx_int_t  ngx_http_mcm_pre_conf                         (ngx_conf_t *cf                                                     );
static ngx_int_t  ngx_http_mcm_post_conf                        (ngx_conf_t *cf                                                     );
 
static char      *ngx_http_mcm_pass                             (ngx_conf_t *cf, ngx_command_t *cmd, void *conf                     );
static ngx_int_t  value_get_handler                             (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static void       value_set_handler                             (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
 
static char      *ngx_http_mcm_upstream_max_fails_unsupported   (ngx_conf_t *cf, ngx_command_t *cmd, void *conf                     );
static char      *ngx_http_mcm_upstream_fail_timeout_unsupported(ngx_conf_t *cf, ngx_command_t *cmd, void *conf                     );

static ngx_conf_bitmask_t ngx_http_mcm_next_upstream_masks[] =
{
  { ngx_string("error"), NGX_HTTP_UPSTREAM_FT_ERROR },
  { ngx_string("timeout"), NGX_HTTP_UPSTREAM_FT_TIMEOUT },
  { ngx_string("invalid_response"), NGX_HTTP_UPSTREAM_FT_INVALID_HEADER },
  { ngx_string("not_found"), NGX_HTTP_UPSTREAM_FT_HTTP_404 },
  { ngx_string("off"), NGX_HTTP_UPSTREAM_FT_OFF },
  { ngx_null_string, 0 } };

static ngx_command_t ngx_http_mcm_commands[] =
{

  { ngx_string("memcached_pass"), NGX_HTTP_LOC_CONF | NGX_HTTP_LIF_CONF
    | NGX_CONF_TAKE1, ngx_http_mcm_pass, NGX_HTTP_LOC_CONF_OFFSET, 0,
    NULL },

  { ngx_string("memcached_connect_timeout"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1, ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_mcm_loc_conf_t, upstream.connect_timeout), NULL },

  { ngx_string("memcached_send_timeout"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1, ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_mcm_loc_conf_t, upstream.send_timeout), NULL },

  { ngx_string("memcached_buffer_size"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1, ngx_conf_set_size_slot,
    NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_mcm_loc_conf_t, upstream.buffer_size), NULL },

  { ngx_string("memcached_read_timeout"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1, ngx_conf_set_msec_slot,
    NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_mcm_loc_conf_t, upstream.read_timeout), NULL },

  { ngx_string("memcached_next_upstream"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_1MORE, ngx_conf_set_bitmask_slot,
    NGX_HTTP_LOC_CONF_OFFSET, offsetof(ngx_http_mcm_loc_conf_t, upstream.next_upstream),
    &ngx_http_mcm_next_upstream_masks },

  { ngx_string("memcached_upstream_max_fails"), NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF
    | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_http_mcm_upstream_max_fails_unsupported, 0, 0, NULL },

  { ngx_string("memcached_upstream_fail_timeout"), NGX_HTTP_MAIN_CONF
    | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
    ngx_http_mcm_upstream_fail_timeout_unsupported, 0, 0, NULL },

  ngx_null_command };

static ngx_http_module_t ngx_http_mcm_module_ctx =
{

  ngx_http_mcm_pre_conf,                        /* preconfiguration */
  ngx_http_mcm_post_conf,                       /* postconfiguration */

  NULL,                                         /* create main configuration */
  NULL,                                         /* init main configuration */

  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */

  ngx_http_mcm_create_loc_conf,                 /* create location configration */
  ngx_http_mcm_merge_loc_conf                   /* merge location configration */
};

ngx_module_t ngx_http_memcached_more_module =
{ NGX_MODULE_V1, &ngx_http_mcm_module_ctx,      /* module context */
  ngx_http_mcm_commands,                        /* module directives */
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

static ngx_str_t ngx_http_mcm_prefix = ngx_string("memcached_prefix");
static ngx_str_t ngx_http_mcm_suffix = ngx_string("memcached_suffix");
static ngx_str_t ngx_http_mcm_key = ngx_string("memcached_key");

/* static ngx_str_t ngx_http_mcm_value  = ngx_string("memcached_value"); */

#define __VALUE "VALUE"
#define __END   "END"

#define VALUE __VALUE" "
#define END CRLF __END CRLF

#define MC_END_LEN   (sizeof(mc_end) - 1)
static u_char mc_end[] = END;

#define MC_VALUE_LEN   (sizeof(mc_value) - 1)
/* static u_char  mc_value[] = VALUE;  */

  static ngx_http_mcm_ctx_t *
ngx_http_mcm_get_ctx(ngx_http_request_t *r)
{
  ngx_http_mcm_ctx_t *ctx;

  ctx = ngx_http_get_module_ctx(r, ngx_http_memcached_more_module);

  if (NULL == ctx) {

    ctx = ngx_palloc(r->pool, sizeof(ngx_http_mcm_ctx_t));
    if (ctx == NULL) { return NULL; }

    ngx_http_set_ctx(r, ctx, ngx_http_memcached_more_module);

  }

  return ctx;
}

  static ngx_int_t
ngx_http_mcm_handler(ngx_http_request_t *r)
{
  ngx_int_t rc;
  ngx_http_upstream_t *u;
  ngx_http_mcm_ctx_t *ctx;
  ngx_http_mcm_loc_conf_t *mlcf;

  if (!(r->method & (NGX_HTTP_GET | NGX_HTTP_HEAD))) {
    return NGX_HTTP_NOT_ALLOWED;
  }

  rc = ngx_http_discard_request_body(r);

  if (rc != NGX_OK) {
    return rc;
  }

  if (ngx_http_set_content_type(r) != NGX_OK) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  mlcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_more_module);

  u = ngx_pcalloc(r->pool, sizeof(ngx_http_upstream_t));
  if (u == NULL) {
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  u->schema = mlcf->upstream.schema;

  u->peer.log = r->connection->log;
  u->peer.log_error = NGX_ERROR_ERR;
#if (NGX_THREADS)
  u->peer.lock = &r->connection->lock;
#endif

  u->output.tag = (ngx_buf_tag_t) & ngx_http_memcached_more_module;

  u->conf = &mlcf->upstream;

  u->create_request = ngx_http_mcm_create_request;
  u->reinit_request = ngx_http_mcm_reinit_request;
  u->process_header = ngx_http_mcm_process_header;
  u->abort_request = ngx_http_mcm_abort_request;
  u->finalize_request = ngx_http_mcm_finalize_request;

  r->upstream = u;

  /* ctx = ngx_palloc(r->pool, sizeof(ngx_http_mcm_ctx_t)); */
  /* if (ctx == NULL) { */
  /* return NGX_HTTP_INTERNAL_SERVER_ERROR; */
  /* } */
  ctx = ngx_http_mcm_get_ctx(r);

  ctx->times = 2;
  ctx->rest = MC_END_LEN;
  ctx->request = r;

  ngx_http_set_ctx(r, ctx, ngx_http_memcached_more_module);

  u->input_filter_init = ngx_http_mcm_filter_init;
  u->input_filter = ngx_http_mcm_filter;
  u->input_filter_ctx = ctx;

  ngx_http_upstream_init(r);

  return NGX_DONE;
}

  static ngx_int_t
ngx_http_mcm_create_request(ngx_http_request_t *r)
{
  size_t len;
  uintptr_t escape;
  ngx_buf_t *b;
  ngx_chain_t *cl;
  ngx_http_mcm_ctx_t *ctx;
  ngx_http_variable_value_t *vv;
  ngx_http_mcm_loc_conf_t *mlcf;

  mlcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_more_module);

  logd(r, mlcf->prefix_index);

  vv = ngx_http_get_indexed_variable(r, mlcf->key_index);

  if (vv == NULL || vv->not_found || vv->len == 0) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
        "the \"$memcached_key\" variable is not set");
    return NGX_ERROR;
  }

  escape = 2 * ngx_escape_uri(NULL, vv->data, vv->len, NGX_ESCAPE_MEMCACHED);

  len = sizeof("get ") - 1 + vv->len + escape + sizeof(CRLF) - 1;

  b = ngx_create_temp_buf(r->pool, len);
  if (b == NULL) {
    return NGX_ERROR;
  }

  cl = ngx_alloc_chain_link(r->pool);
  if (cl == NULL) {
    return NGX_ERROR;
  }

  cl->buf = b;
  cl->next = NULL;

  r->upstream->request_bufs = cl;

  *b->last++ = 'g'; *b->last++ = 'e'; *b->last++ = 't'; *b->last++ = ' ';

  ctx = ngx_http_get_module_ctx(r, ngx_http_memcached_more_module);

  ctx->key.data = b->last;

  if (escape == 0) {
    b->last = ngx_copy(b->last, vv->data, vv->len);

  }
  else {
    b->last = (u_char *) ngx_escape_uri(b->last, vv->data, vv->len,
        NGX_ESCAPE_MEMCACHED);
  }

  ctx->key.len = b->last - ctx->key.data;

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "http memcached request: \"%V\"", &ctx->key);

  *b->last++ = CR; *b->last++ = LF;

  return NGX_OK;
}

  static ngx_int_t
ngx_http_mcm_reinit_request(ngx_http_request_t *r)
{
  return NGX_OK;
}

  static ngx_int_t
ngx_parse_mc_response(ngx_buf_t *b, mc_res_t *t)
{
  return 0;
}

  static ngx_int_t
ngx_http_mcm_process_header(ngx_http_request_t *r)
{
  /* TODO move body filter here */

  u_char *p, *len;
  ngx_str_t line;
  ngx_http_upstream_t *u;
  ngx_http_mcm_ctx_t *ctx;

  u = r->upstream;

  /* find the LF char */
  for (p = u->buffer.pos; p < u->buffer.last && LF != *p; p++)
    ;

  if (*p != LF) {
    return NGX_AGAIN;
  }

  *p = '\0';

  line.len = p - u->buffer.pos - 1;
  line.data = u->buffer.pos;

  ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "memcached: \"%V\"", &line);

  p = u->buffer.pos;

  ctx = ngx_http_get_module_ctx(r, ngx_http_memcached_more_module);

  if (ngx_strncmp(p, "VALUE ", sizeof("VALUE ") - 1) == 0) {

    p += sizeof("VALUE ") - 1;

    if (ngx_strncmp(p, ctx->key.data, ctx->key.len) != 0) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
          "memcached sent invalid key in response \"%V\" "
          "for key \"%V\"",
          &line, &ctx->key);

      return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    p += ctx->key.len;

    if (*p++ != ' ') {
      goto no_valid;
    }

    /* skip flags */

    while (*p) {
      if (*p++ == ' ') {
        goto length;
      }
    }

    goto no_valid;

length:

    len = p;

    while (*p && *p++ != CR) { /* void */}

    r->headers_out.content_length_n = ngx_atoof(len, p - len - 1);
    if (r->headers_out.content_length_n == -1) {
      ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
          "memcached sent invalid length in response \"%V\" "
          "for key \"%V\"",
          &line, &ctx->key);
      return NGX_HTTP_UPSTREAM_INVALID_HEADER;
    }

    ngx_http_mcm_loc_conf_t *lcf;
    ngx_http_variable_value_t *pv;
    ngx_http_variable_value_t *sv;

    lcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_more_module);

    pv = ngx_http_get_flushed_variable(r, lcf->prefix_index);
    log(r, "got prefix:%v", pv);
    sv = ngx_http_get_indexed_variable(r, lcf->suffix_index);
    log(r, "got suffix:%v", sv);

    r->headers_out.content_length_n += pv->len + sv->len;
    /* r->headers_out.content_length_n += pv->len;// + sv->len; */

    logd(r, r->headers_out.content_length_n);

    u->headers_in.status_n = 200;
    u->state->status = 200;
    u->buffer.pos = p + 1;

    logcs(r, u->buffer.pos);

    return NGX_OK;
  }

  if (ngx_strcmp(p, "END\x0d") == 0) {
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
        "key: \"%V\" was not found by memcached", &ctx->key);

    u->headers_in.status_n = 404;
    u->state->status = 404;

    return NGX_OK;
  }

no_valid:

  ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
      "memcached sent invalid response: \"%V\"", &line);

  return NGX_HTTP_UPSTREAM_INVALID_HEADER;
}

  static ngx_chain_t *
ngx_get_free_chain(ngx_http_request_t *r, ngx_http_upstream_t *u)
{
  ngx_chain_t *cl;
  ngx_chain_t **ll;

  for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
    ll = &cl->next;
  }

  cl = ngx_chain_get_free_buf(r->pool, &u->free_bufs);
  if (cl == NULL) {
    /* return NGX_ERROR; */
    return NULL;
  }

  cl->buf->flush = 1;
  cl->buf->memory = 1;

  *ll = cl;

  return cl;
}

  static ngx_int_t
ngx_http_mcm_filter_init(void *data)
{

  ngx_http_mcm_ctx_t *ctx = data;
  ngx_http_request_t *r;
  ngx_http_upstream_t *u;
  ngx_chain_t *cl;
  /* ngx_chain_t                   **ll; */
  ngx_buf_t *b;

  ngx_http_variable_value_t *pv;
  ngx_http_variable_value_t *sv;
  ngx_http_mcm_loc_conf_t *lcf;

  r = ctx->request;
  u = ctx->request->upstream;
  b = &u->buffer;

  u->length += MC_END_LEN;

  lcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_more_module);

  logd(r, lcf->prefix_index);

  pv = ngx_http_get_indexed_variable(r, lcf->prefix_index);
  if (NULL == pv) {
    return NGX_ERROR;
  }
  sv = ngx_http_get_indexed_variable(r, lcf->suffix_index);
  if (NULL == sv) {
    return NGX_ERROR;
  }

  log(r, "filter init");
  log(r, "prefix:%v", pv);

  /* if (0) */
  { /* create the header part of respond. */

    if (pv->len) {

      cl = ngx_get_free_chain(ctx->request, u);
      if (NULL == cl) {
        return NGX_ERROR;
      }

      cl->buf->pos = pv->data;
      cl->buf->last = pv->data + pv->len;
      cl->buf->tag = u->output.tag;

    }

  }
  u->length -= pv->len;
  u->length -= sv->len;

  return NGX_OK;
}

  static ngx_int_t
ngx_http_mcm_add_suffix(ngx_http_mcm_ctx_t *ctx,
    ngx_http_upstream_t *u, ngx_http_request_t *r)
{

  ngx_chain_t *cl;
  ngx_chain_t **ll;
  ngx_http_mcm_loc_conf_t *lcf;
  ngx_http_variable_value_t *sv;

  lcf = ngx_http_get_module_loc_conf(r, ngx_http_memcached_more_module);

  sv = ngx_http_get_indexed_variable(r, lcf->suffix_index);
  log(r, "got suffix:%v", sv);

  if (NULL == sv || sv->not_found) {
    log(r, "not found");
    return NGX_ERROR;
  }

  if (0 == sv->len) {
    return NGX_OK;
  }

  for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
    ll = &cl->next;
  }

  cl = ngx_chain_get_free_buf(ctx->request->pool, &u->free_bufs);
  if (cl == NULL) {
    return NGX_ERROR;
  }

  cl->buf->flush = 1;
  cl->buf->memory = 1;

  *ll = cl;

  cl->buf->pos = sv->data;
  cl->buf->last = sv->data + sv->len;
  cl->buf->tag = u->output.tag;

  log(r, "create suffix done");

  return NGX_OK;
}

  static ngx_int_t
ngx_http_mcm_filter(void *data, ssize_t bytes)
{
  ngx_http_mcm_ctx_t *ctx = data;

  u_char *last;
  ngx_buf_t *b;
  ngx_chain_t *cl, **ll;
  ngx_http_upstream_t *u;
  ngx_http_request_t *r;

  r = ctx->request;
  u = ctx->request->upstream;
  b = &u->buffer;

  log(r, "filter start-----------------");

  logd(r, u->length);
  logd(r, ctx->rest);
  logd(r, bytes);

  if (u->length == ctx->rest) {

    if (ngx_strncmp(b->last,
          mc_end + MC_END_LEN - ctx->rest,
          ctx->rest)
        != 0)
    {
      ngx_log_error(NGX_LOG_ERR, ctx->request->connection->log, 0,
          "memcached sent invalid trailer");
    }

    u->length = 0;
    ctx->rest = 0;

    return ngx_http_mcm_add_suffix(ctx, u, r);
    /* return NGX_OK; */
  }

  for (cl = u->out_bufs, ll = &u->out_bufs; cl; cl = cl->next) {
    ll = &cl->next;
  }

  cl = ngx_chain_get_free_buf(ctx->request->pool, &u->free_bufs);
  if (cl == NULL) {
    return NGX_ERROR;
  }

  cl->buf->flush = 1;
  cl->buf->memory = 1;

  *ll = cl;

  last = b->last;
  cl->buf->pos = last;
  b->last += bytes;
  cl->buf->last = b->last;
  cl->buf->tag = u->output.tag;

  ngx_log_debug4(NGX_LOG_DEBUG_HTTP, ctx->request->connection->log, 0,
      "memcached filter bytes:%z size:%z length:%z rest:%z",
      bytes, b->last - b->pos, u->length, ctx->rest);

  if (bytes <= (ssize_t) (u->length - MC_END_LEN)) {
    u->length -= bytes;
    return NGX_OK;
  }

  last += u->length - MC_END_LEN;

  if (ngx_strncmp(last, mc_end, b->last - last) != 0) {
    ngx_log_error(NGX_LOG_ERR, ctx->request->connection->log, 0,
        "memcached sent invalid trailer");
  }

  ctx->rest -= b->last - last;
  b->last = last;
  cl->buf->last = last;
  u->length = ctx->rest;

  ngx_log_debug4(NGX_LOG_DEBUG_HTTP, ctx->request->connection->log, 0,
      "memcached filter bytes:%z size:%z length:%z rest:%z",
      bytes, b->last - b->pos, u->length, ctx->rest);

  if (u->length == 0)
    return ngx_http_mcm_add_suffix(ctx, u, r);
  return NGX_OK;
}

  static void
ngx_http_mcm_abort_request(ngx_http_request_t *r)
{
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "abort http memcached request");
  return;
}

  static void
ngx_http_mcm_finalize_request(ngx_http_request_t *r, ngx_int_t rc)
{
  ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "finalize http memcached request");
  return;
}

  static void *
ngx_http_mcm_create_loc_conf(ngx_conf_t *cf)
{
  ngx_http_mcm_loc_conf_t *conf;

  conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_mcm_loc_conf_t));
  if (conf == NULL) {
    return NGX_CONF_ERROR;
  }

  {
    /*
     * set by ngx_pcalloc():
     *
     *     conf->upstream.bufs.num = 0;
     *     conf->upstream.next_upstream = 0;
     *     conf->upstream.temp_path = NULL;
     *     conf->upstream.schema = { 0, NULL };
     *     conf->upstream.uri = { 0, NULL };
     *     conf->upstream.location = NULL;
     */

    conf->upstream.connect_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.send_timeout = NGX_CONF_UNSET_MSEC;
    conf->upstream.read_timeout = NGX_CONF_UNSET_MSEC;

    conf->upstream.buffer_size = NGX_CONF_UNSET_SIZE;

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

    conf->prefix_index = NGX_CONF_UNSET;
    conf->suffix_index = NGX_CONF_UNSET;
    conf->key_index = NGX_CONF_UNSET;
    conf->value_index = NGX_CONF_UNSET;
  }

  return conf;
}

  static char *
ngx_http_mcm_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
  ngx_http_mcm_loc_conf_t *prev = parent;
  ngx_http_mcm_loc_conf_t *conf = child;

  ngx_conf_merge_msec_value(conf->upstream.connect_timeout,
      prev->upstream.connect_timeout, 60000);

  ngx_conf_merge_msec_value(conf->upstream.send_timeout,
      prev->upstream.send_timeout, 60000);

  ngx_conf_merge_msec_value(conf->upstream.read_timeout,
      prev->upstream.read_timeout, 60000);

  ngx_conf_merge_size_value(conf->upstream.buffer_size,
      prev->upstream.buffer_size,
      (size_t) ngx_pagesize);
  //(size_t) 100);

  /* ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "buffer_size : %d", ngx_pagesize); */

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

  if (conf->key_index == NGX_CONF_UNSET) {
    conf->key_index = prev->key_index;
  }
  if (conf->value_index == NGX_CONF_UNSET) {
    conf->value_index = prev->value_index;
  }
  if (conf->prefix_index == NGX_CONF_UNSET) {
    conf->prefix_index = prev->prefix_index;
  }
  if (conf->suffix_index == NGX_CONF_UNSET) {
    conf->suffix_index = prev->suffix_index;
  }

  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "prefix_index : %d",
      conf->prefix_index);
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "suffix_index : %d",
      conf->suffix_index);

  return NGX_CONF_OK;
}

  static ngx_int_t
ngx_http_mcm_pre_conf(ngx_conf_t *cf)
{
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "pre conf");
  return NGX_OK;
}

  static ngx_int_t
ngx_http_mcm_post_conf(ngx_conf_t *cf)
{
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "post conf");
  return NGX_OK;
}

  static char *
ngx_http_mcm_pass(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_mcm_loc_conf_t *lcf = conf;

  ngx_str_t *value;
  ngx_url_t u;
  ngx_http_core_loc_conf_t *clcf;
  ngx_http_variable_t *v;
  /* ngx_http_core_main_conf_t *cmcf; */
  /* ngx_uint_t                 i; */

  if (lcf->upstream.schema.len) {
    return "is duplicate";
  }

  value = cf->args->elts;

  ngx_memzero(&u, sizeof(ngx_url_t));

  u.url = value[1];
  u.no_resolve = 1;

  lcf->upstream.upstream = ngx_http_upstream_add(cf, &u, 0);
  if (lcf->upstream.upstream == NULL) {
    return NGX_CONF_ERROR;
  }

  lcf->upstream.schema.len = sizeof("memcached://") - 1;
  lcf->upstream.schema.data = (u_char *) "memcached://";

  clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

  clcf->handler = ngx_http_mcm_handler;

  if (clcf->name.data[clcf->name.len - 1] == '/') {
    clcf->auto_redirect = 1;
  }

  v = ngx_http_add_variable(cf, &ngx_http_mcm_prefix,
      NGX_HTTP_VAR_CHANGEABLE | NGX_HTTP_VAR_NOCACHEABLE);
  v->set_handler = value_set_handler;
  v->get_handler = value_get_handler;
  v->data = ngx_http_get_variable_index(cf, &ngx_http_mcm_prefix);
  ngx_http_add_variable(cf, &ngx_http_mcm_suffix, NGX_HTTP_VAR_CHANGEABLE
      | NGX_HTTP_VAR_NOCACHEABLE);

  if (NGX_ERROR == (lcf->key_index = ngx_http_get_variable_index(cf,
          &ngx_http_mcm_key))) {
    return NGX_CONF_ERROR;
  }

  if (NGX_ERROR == (lcf->prefix_index = ngx_http_get_variable_index(cf,
          &ngx_http_mcm_prefix))) {
    return NGX_CONF_ERROR;
  }

  if (NGX_ERROR == (lcf->suffix_index = ngx_http_get_variable_index(cf,
          &ngx_http_mcm_suffix))) {
    return NGX_CONF_ERROR;
  }

  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "mc_pass");
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "lcf->key_index: %d", lcf->key_index);
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "lcf->prefix_index : %d",
      lcf->prefix_index);
  ngx_conf_log_error(NGX_LOG_INFO, cf, 0, "lcf->suffix_index : %d",
      lcf->suffix_index);

  return NGX_CONF_OK;
}

  static ngx_int_t
value_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{
  fprintf(stderr, "value_get_handler\n");
  ngx_http_mcm_ctx_t *ctx;

  ctx = ngx_http_mcm_get_ctx(r);

  v->data = ctx->pre.data;
  v->len = ctx->pre.len;
  return NGX_OK;
}

  static void
value_set_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
    uintptr_t data)
{

  fprintf(stderr, "value_set_handler\n");
  ngx_http_variable_value_t *vv;
  ngx_int_t index = data;
  ngx_http_mcm_ctx_t *ctx;

  vv = &r->variables[index];

  /* vv->data = v->data; */
  /* vv->len = v->len; */
  vv->valid = 0;

  /* vv->no_cacheable = 0; */
  /* vv->not_found = 0; */

  ctx = ngx_http_mcm_get_ctx(r);
  ctx->pre.data = v->data;
  ctx->pre.len = v->len;

}

  static char *
ngx_http_mcm_upstream_max_fails_unsupported(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
  ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
      "\"memcached_upstream_max_fails\" is not supported, "
      "use the \"max_fails\" parameter of the \"server\" directive ",
      "inside the \"upstream\" block");

  return NGX_CONF_ERROR;
}

  static char *
ngx_http_mcm_upstream_fail_timeout_unsupported(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf)
{
  ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
      "\"memcached_upstream_fail_timeout\" is not supported, "
      "use the \"fail_timeout\" parameter of the \"server\" directive ",
      "inside the \"upstream\" block");

  return NGX_CONF_ERROR;
}
