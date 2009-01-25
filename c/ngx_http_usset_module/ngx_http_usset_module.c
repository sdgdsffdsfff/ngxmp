/**-------------------------/// runtime upstream access \\\---------------------------
 *
 * <b>runtime upstream access</b>
 * @version : 1.0
 * @since : 2009 01 24 06:04:59 PM
 * 
 * @description :
 *   
 * @usage : 
 * 
 * @author : drdr.xp | drdr.xp@gmail.com
 * @copyright sina.com.cn 
 * @TODO : 
 * 
 *--------------------------\\\ runtime upstream access ///---------------------------*/
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_util.h"

#include "ngx_http_usset_module.h"


typedef struct {
  ngx_int_t nus;
  hash_t *us_hash;
}
ngx_http_usset_main_conf_t;



/* FUNC_DEC_START */
static ngx_int_t  ngx_http_usset_create_us_hash  (ngx_conf_t *cf);
static ngx_int_t  ngx_http_usset_get_us          (ngx_conf_t *cf, hash_elt_t **hashes);
static ngx_int_t  ngx_http_usset_postconf        (ngx_conf_t *cf);
static void      *ngx_http_usset_create_main_conf(ngx_conf_t *cf);
/* FUNC_DEC_END */



static ngx_http_module_t
ngx_http_usset_module_ctx = {
  NULL,                                         /* preconfiguration */
  ngx_http_usset_postconf,                      /* postconfiguration */
  ngx_http_usset_create_main_conf,              /* create main configuration */
  NULL,                                         /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};


ngx_module_t
ngx_http_usset_module = {
  NGX_MODULE_V1,
  &ngx_http_usset_module_ctx,                   /* module context */
  NULL,// ngx_http_usset_commands,                      /* module directives */
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
ngx_http_usset_postconf(ngx_conf_t *cf)
{
  ngx_http_usset_create_us_hash(cf);

  return NGX_OK;
}

  static void *
ngx_http_usset_create_main_conf(ngx_conf_t *cf)
{
  ngx_http_usset_main_conf_t *mcf;

  mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_usset_main_conf_t));
  if (NULL == mcf) {
    logce("allocating main conf failed");
    return NULL;
  }

  return mcf;
}


  ngx_http_upstream_srv_conf_t *
ngx_http_usset_find_upstream_server_byname(ngx_http_request_t *r, ngx_str_t *name)
{
  ngx_http_usset_main_conf_t *mcf;
  hash_t *hash;
  hash_elt_t *b;
  ngx_uint_t h = 0;


  mcf = ngx_http_get_module_main_conf(r, ngx_http_usset_module);
  hash = mcf->us_hash;


  h = ngx_hash_key(name->data, name->len);

  b = &hash->buckets[h % hash->size];

  if (NULL == b->next) {
    return (ngx_http_upstream_srv_conf_t*)b->val;
  }

  for (;b;b = b->next) {
    if (ngx_str_eq(&b->name, name)) {
      return (ngx_http_upstream_srv_conf_t*)b->val;
    }
  }

  return NULL;



}

  static ngx_int_t
ngx_http_usset_create_us_hash(ngx_conf_t *cf)
{
  /* ngx_hash_init_t               hi; */
  ngx_http_usset_main_conf_t   *mcf;

  /* ngx_hash_key_t               *usnames; */
  /* ngx_http_upstream_srv_conf_t *uscnfs; */

  hash_elt_t *hash_elts;
  ngx_int_t                     nus;
  ngx_int_t i;

  /* ngx_int_t rc = 0; */



  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_usset_module);



  nus = ngx_http_usset_get_us(cf, &hash_elts);
  if (NGX_ERROR == nus) {
    logce("getting upstream server configs failed");
    return NGX_ERROR;
  }


  mcf->nus = nus;

  mcf->us_hash = hash_create(cf->pool, HASH_MAX_SIZE);
  if (NULL == mcf->us_hash) {
    logce("creating us_hash failed");
    return NGX_ERROR;
  }

  for (i= 0; i < nus; ++i){
    hash_add(mcf->us_hash, &hash_elts[i]);
  }

  return NGX_OK;
}



  static ngx_int_t
ngx_http_usset_get_us(ngx_conf_t *cf, hash_elt_t **hashes)
{
  ngx_http_upstream_main_conf_t *umcf;
  ngx_http_upstream_srv_conf_t **scnf;

  ngx_uint_t i;
  hash_elt_t *h;


  umcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

  h = ngx_palloc(cf->pool, sizeof(hash_elt_t)*umcf->upstreams.nelts);
  if (NULL == h) {
    logce("allocating memory for upstream hash elements failedl");
    return NGX_ERROR;
  }

  for (i= 0; i < umcf->upstreams.nelts; ++i){
    scnf = (ngx_http_upstream_srv_conf_t**)ngx_array_get(&umcf->upstreams, i);
    h[i].name = (*scnf)->host;
    h[i].hash = ngx_hash_key(h[i].name.data, h[i].name.len);
    h[i].val = (uintptr_t)*scnf;
  }

  *hashes = h;

  return i;
}
