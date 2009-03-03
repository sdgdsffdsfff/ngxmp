#ifndef __ngx_memcached_util__
#define __ngx_memcached_util__

#include <libmemcached/memcached.h>

  static ngx_int_t
ngx_mcd_connect(ngx_cycle_t *cycle, ngx_str_t *addr, memcached_st **mc)
{

  memcached_st             *memc;
  memcached_server_st      *srv;
  memcached_return          rc;

  char                      buf[128];


  logy("connect to %V", addr);

  if (!addr->len) {
    logye("server addr not set");
    return NGX_ERROR;
  }

  ngx_memcpy(buf, addr->data, addr->len);
  buf[addr->len] = 0;

  memc = memcached_create(NULL);
  srv = memcached_servers_parse(buf);

  if ('/' == buf[0]) { /* unix socket */
    rc = memcached_server_add_unix_socket(memc, buf);
  }
  else {
    rc = memcached_server_push(memc, srv);
  }
  if (MEMCACHED_SUCCESS != rc) {
    logye("pushing server failed rc=[%d]", rc);
    return NGX_ERROR;
  }

  *mc = memc;

  ngx_uint_t vlen;
  ngx_uint_t flg;

  char *v = memcached_get(memc, "a", 1, &vlen, &flg, &rc);

  if (MEMCACHED_NOTFOUND == rc || MEMCACHED_SUCCESS == rc) {
    logy("mc %V connected rc=[%d]", addr, rc);
  }
  else {
    logye("connecting to mc %V failed rc=[%d]", addr, rc);
    return NGX_ERROR;
  }

  if (MEMCACHED_SUCCESS == rc) {
    free(v);
  }

  return NGX_OK;
}

/**
 * @param
 *    memcached_st *mc
 *    ngx_str_t    *sk
 *    ngx_str_t    *sv
 *
 * @return 
 *        NGX_OK
 */
#define mcd_set(mc, sk, sv, exp)                                                \
({                                                                              \
    ngx_uint_t        flag = 0;                                                 \
    memcached_return  rc;                                                       \
                                                                                \
    int               rrc  = NGX_OK;                                            \
                                                                                \
    rc = memcached_set((mc), (char*)(sk)->data, (sk)->len,                      \
      (char*)(sv)->data, (sv)->len, exp, flag);                                 \
                                                                                \
    if (MEMCACHED_SUCCESS != rc) {                                              \
      rrc = NGX_ERROR;                                                          \
    }                                                                           \
                                                                                \
    __return(rrc);                                                              \
})
/**
 * @param
 *    memcached_st *mc
 *    ngx_str_t    *sk
 *    ngx_str_t    *sv
 *
 * @return 
 *    NGX_OK
 */
#define mcd_get(mc, sk, sv)                                                     \
({                                                                              \
    char             *v = NULL;                                                 \
    size_t            vlen;                                                     \
    ngx_uint_t        flag;                                                     \
    memcached_return  rc;                                                       \
    int               rrc  = NGX_OK;                                           \
                                                                                \
    v = memcached_get((mc), (char*)(sk)->data, (sk)->len, &vlen, &flag, &rc);   \
    if (MEMCACHED_SUCCESS != rc) {                                              \
      rrc = NGX_ERROR;                                                      \
    }                                                                           \
    else if (vlen > (sv)->len) {                                                \
      rrc = InternalError;                                                      \
    }                                                                           \
    else {                                                                      \
      ngx_memcpy((sv)->data, v, vlen);                                          \
      (sv)->len = vlen;                                                         \
      rrc = 0;                                                                  \
    }                                                                           \
                                                                                \
    if (NULL != v) free(v);                                                     \
                                                                                \
    __return(rrc);                                                              \
})


#endif /* __ngx_memcached_util__ */
