/*
 * Copyright (C) Igor Sysoev
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static ngx_int_t
ngx_http_postpone_filter_output_postponed_request(ngx_http_request_t *r);
static ngx_int_t
ngx_http_postpone_filter_init(ngx_conf_t *cf);

static ngx_http_module_t ngx_http_postpone_filter_module_ctx =
{ NULL, /* preconfiguration */
  ngx_http_postpone_filter_init, /* postconfiguration */

  NULL, /* create main configuration */
  NULL, /* init main configuration */

  NULL, /* create server configuration */
  NULL, /* merge server configuration */

  NULL, /* create location configuration */
  NULL /* merge location configuration */
};

ngx_module_t ngx_http_postpone_filter_module =
{ NGX_MODULE_V1, &ngx_http_postpone_filter_module_ctx, /* module context */
  NULL, /* module directives */
  NGX_HTTP_MODULE, /* module type */
  NULL, /* init master */
  NULL, /* init module */
  NULL, /* init process */
  NULL , /* init thread */
  NULL  , /* exit thread */
  NULL, /* exit process */
  NULL, /* exit master */
  NGX_MODULE_V1_PADDING
};

static ngx_http_output_body_filter_pt ngx_http_next_filter;

  static void
print_chain(ngx_chain_t *t)
{
  char buf[4096*4];
  while (t) {
    if (t->buf) {
      int len = t->buf->last - t->buf->pos;
      memcpy(buf, t->buf->pos, len);
      buf[len] = 0;

      /* fprintf(stderr, "		    ***:%s\n", buf); */

    }
    t = t->next;
  }
}

  static void
print_post(ngx_http_request_t *r)
{
  ngx_http_request_t *rr;
  ngx_http_postponed_request_t *pp;

  /* fprintf(stderr, "---------------------------*****************************------------------------------\n"); */
  for (pp=r->postponed; pp; pp=pp->next) {
    rr = pp->request;

    if (rr) {
      fprintf(stderr, "req,%p\n",rr);
    }
    else {
      fprintf(stderr, "noreq\n");
    }

    if (pp->out) {
      print_chain(pp->out);
    } else {
      fprintf(stderr,"\n");
    }

  }

}

  static ngx_int_t
ngx_http_postpone_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
  ngx_int_t                      rc;
  ngx_chain_t                   *out;
  /* ngx_http_request_t            *rr; */
  ngx_http_postponed_request_t  *pr;
  ngx_http_postponed_request_t **ppr;
  /* ngx_http_postponed_request_t  *pp; */
  /* ngx_chain_t                   *t   = NULL; */
  /* ngx_chain_t                   *t0  = NULL; */
  /* ngx_chain_t                  **tp  = &t; */
  /* ngx_chain_t                   *in2; */


  /* fprintf(stderr, "\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\ \n"); */
  /* fprintf(stderr, "connection->request:%p, r:%p, parent:%p\n", r->connection->data, r, r->parent); */
  /* print_chain(in); */
  /* fprintf(stderr, "////////////// \n"); */

  ngx_log_debug3(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
      "http postpone filter \"%V?%V\" %p", &r->uri, &r->args, in);

  /* |+* */
   /* *  Cache Output */
   /* *  for active request, sub request, not the current request. */
   /* +| */
  /* if (r == r->connection->data */
      /* && r->parent */
      /* && r->parent->postponed) { */

    /* pp = r->parent->postponed; */
    /* if (pp->request != r) { |+ not the first +| */
      /* |+ rc = ngx_http_postpone_filter(r->parent, NULL); +| */
      /* for (;pp && pp->request != r;pp=pp->next) {} */
      /* if (pp->request == r) */
        /* pp->out = in; */
      /* return NGX_AGAIN; */
    /* } */
  /* } */





  if (r != r->connection->data || (r->postponed && in)) {

    if (r->postponed) {
      for (pr = r->postponed; pr->next; pr = pr->next) { /* void */}

      ppr = pr->request ? &pr->next : NULL;

    }
    else {
      ppr = &r->postponed;
#if (NGX_SUPPRESS_WARN)
      pr = NULL;
#endif
    }

    if (ppr) {
      pr = ngx_palloc(r->pool, sizeof(ngx_http_postponed_request_t));
      if (pr == NULL) {
        return NGX_ERROR;
      }

      *ppr = pr;

      pr->request = NULL;
      pr->out = NULL;
      pr->next = NULL;
    }

    if (ngx_chain_add_copy(r->pool, &pr->out, in) == NGX_ERROR) {
      return NGX_ERROR;
    }

#if 1
    {
      ngx_chain_t *cl;
      ngx_buf_t *b = NULL;
      for (cl = pr->out; cl; cl = cl->next) {
        if (cl->buf == b) {
          ngx_log_error(NGX_LOG_ALERT, r->connection->log, 0,
              "the same buf was used in postponed %p %p",
              b, b->pos);
          ngx_debug_point();
          return NGX_ERROR;
        }
        b = cl->buf;
      }
    }
#endif

    if (r != r->connection->data || r->postponed->request) {
      return NGX_AGAIN;
    }
  }

  if (r->postponed) {
    out = r->postponed->out;
    /* fprintf(stderr, "==============output chain from postponed : \n"); */
    /* print_chain(out); */
    if (out) {
      r->postponed = r->postponed->next;
    }

  }
  else {
    out = in;
  }

  rc = NGX_OK;

  if (out
      || (r->connection->buffered
        & (NGX_HTTP_LOWLEVEL_BUFFERED|NGX_LOWLEVEL_BUFFERED)))
  {
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
        "http postpone filter out \"%V?%V\"", &r->uri, &r->args);

    if (!(out && out->next == NULL && ngx_buf_sync_only(out->buf))) {

      rc = ngx_http_next_filter(r->main, out);

      if (rc == NGX_ERROR) {
        /* NGX_ERROR may be returned by any filter */
        r->connection->error = 1;
      }
    }
  }

  if (r->postponed == NULL) {
    return rc;
  }

  rc = ngx_http_postpone_filter_output_postponed_request(r);
  /* fprintf(stderr, "after : connection->request:%p, r:%p, parent:%p\n", r->connection->data, r, r->parent); */

  if (rc == NGX_ERROR) {
    /* NGX_ERROR may be returned by any filter */
    r->connection->error = 1;
    return rc;
  }

  /* rr = r->connection->data; */

  /* rr->write_event_handler(rr); */

  return rc;
}

  static ngx_int_t
ngx_http_postpone_filter_output_postponed_request(ngx_http_request_t *r)
{
  ngx_int_t rc;
  ngx_chain_t *out;
  ngx_http_log_ctx_t *ctx;
  ngx_http_postponed_request_t *pr;



  /* fprintf(stderr, "ngx_http_postpone_filter_output_postponed_request\n"); */
  for (;;) {
    pr = r->postponed;

    if (pr == NULL) {
      return NGX_OK;
    }

    if (pr->request) {

      ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
          "http postpone filter handle \"%V?%V\"",
          &pr->request->uri, &pr->request->args);

      ctx = r->connection->log->data;
      ctx->current_request = pr->request;

      if (!pr->request->done) {

        r->connection->data = pr->request;



        /* out = pr->out; */

        /* if (out) { */
          /* ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, */
              /* "http postpone filter out postponed \"%V?%V\"", */
              /* &r->uri, &r->args); */

          /* if (!(out && out->next == NULL && ngx_buf_sync_only(out->buf))) { */
            /* |+ fprintf(stderr, "to output posteponed:\n"); +| */
            /* |+ print_chain(out); +| */
            /* if (ngx_http_next_filter(r->main, out) == NGX_ERROR) { */
              /* return NGX_ERROR; */
            /* } */
          /* } */
        /* } */


        /* |+ xp +| */
        /* pr->request->write_event_handler(pr->request); */
        /* |+ xp +| */

        return NGX_AGAIN;
      }

      rc = ngx_http_postpone_filter_output_postponed_request(pr->request);

      if (rc == NGX_AGAIN || rc == NGX_ERROR) {
        return rc;
      }

      r->postponed = r->postponed->next;
      pr = r->postponed;
    }

    if (pr == NULL) {
      return NGX_OK;
    }

    out = pr->out;

    if (out) {
      ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
          "http postpone filter out postponed \"%V?%V\"",
          &r->uri, &r->args);

      if (!(out && out->next == NULL && ngx_buf_sync_only(out->buf))) {
        /* fprintf(stderr, "to output posteponed:\n"); */
        /* print_chain(out); */
        if (ngx_http_next_filter(r->main, out) == NGX_ERROR) {
          return NGX_ERROR;
        }
      }
    }

    r->postponed = r->postponed->next;
  }
}

  static ngx_int_t
ngx_http_postpone_filter_init(ngx_conf_t *cf)
{
  ngx_http_next_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_postpone_filter;

  return NGX_OK;
}
