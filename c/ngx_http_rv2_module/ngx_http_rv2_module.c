/**
 * TODO add check_mdb to other query too
 * TODO duplicate remote variable check
 * TODO crc32 support only for now.
 */

/**
 * rv_dec $varname_var 
 *        upstream="us_name_expr"             # optional default ${varname_var}_us
 *        key=$keyname_var                    # optional default ${varname_var}_key
 *        hash=hash_method_str                # optional default crc32
 *        hash_key=$hash_key_var              # optional default $varname_var
 *        default_val="expr";                 # optional default $varname_var
 *
 *
 * rv_timeout_read varname timeout;
 *
 * rv_get $value $varname $rc;
 * rv_set $rv "$value" $rc;
 * rv_incr $value "$value" $rc;
 * rv_decr $value "$value" $rc;
 * rv_delete $varname $rc;
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <libmemcached/memcached.h>
#include "ngx_util.h"

#define THIS  ngx_http_rv2_module

#define mcf_t ngx_http_rv2_main_conf_t
#define lcf_t ngx_http_rv2_loc_conf_t
#define rv_t ngx_http_rv2_var_t

#define rMCF() ngx_http_get_module_main_conf(r,THIS)
#define rLCF() ngx_http_get_module_loc_conf(r,THIS)

typedef enum {
  RV_SET  = 0, 
  RV_INCR, 
  RV_DECR, 
  RV_UNKNOW
} 
ngx_http_rv2_op_t;


typedef struct memcached_st memcached_t;

typedef struct {
  ngx_array_t *rvs;      /* ngx_http_rv2_var_t* */
}
ngx_http_rv2_main_conf_t;

typedef uint32_t (*ngx_http_rv2_hash_func_t)(u_char*, size_t);

typedef struct {
  ngx_str_t name;

  ngx_int_t us_name_idx;

  ngx_str_t hash_str;
  ngx_int_t hash_type;
  ngx_http_rv2_hash_func_t *hash_func;

  ngx_int_t hash_key_idx;

  ngx_str_t default_val;

  /* TODO time outs */

  /* TODO rc */



  /* ngx_str_t                     us_name;        |+ upstream name  +| */

  /* ngx_int_t                     us_index;       |+ upstream index +| */
  /* ngx_http_upstream_srv_conf_t *us; */

  /* ngx_array_t                  *mc_addrs; */
  /* ngx_array_t                  *mcs;            |+ memcache_t  +| */

  /* ngx_int_t                     mck_index; */
  /* ngx_int_t                     mcv_index; */

  /* ngx_int_t                     hashkey_index;  |+ which varaible to use as hash string +| */

  /* void                         *hash_method;    |+ not used yet for now +| */
   /* */

  /* ngx_int_t                     op;             |+ operation type [set|incr|decr] +| */
  /* ngx_int_t                     v0;             |+ default value for incr/decr +| */
}
ngx_http_rv2_var_t;


static void      *ngx_http_create_main_conf      (ngx_conf_t *cf);
static char      *ngx_http_init_main_conf        (ngx_conf_t *cf, void *conf);
static ngx_int_t  ngx_http_rv2_init_proc         (ngx_cycle_t *cycle);
static void       ngx_http_rv2_exit              (ngx_cycle_t *cycle);
 
static char      *ngx_http_rv2_remote_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_rv2_hash_key_command  (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t  ngx_http_rv2_post_conf         (ngx_conf_t *cf);
 
static ngx_int_t  ngx_http_rv2_init_var_mc       (ngx_conf_t *cf, ngx_http_variable_t *v);
static ngx_int_t  ngx_http_rv2_connet_mc         (ngx_cycle_t *cycle, ngx_array_t **mcs, ngx_array_t *addrs /* ngx_str_t */);



#define HASH_CRC32         0x01
#define HASH_CRC32_SHORT   0x02
#define HASH_5381          0x03




static const ngx_str_t rv_op_incr = ngx_string("incr");
static const ngx_str_t rv_op_decr = ngx_string("decr");


static const ngx_str_t hash_crc32 = ngx_string("crc32");
static const ngx_str_t hash_crc32_short = ngx_string("crc32_short");
static const ngx_str_t hash_5381 = ngx_string("5381");




static ngx_command_t
ngx_http_rv2_commands[] = {

  { ngx_string("rv_dec"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
    ngx_http_rv2_remote_var_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_hash_key"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE2,
    ngx_http_rv2_hash_key_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_rv2_module_ctx = {
  NULL,                                         /* preconfiguration */
  ngx_http_rv2_post_conf,                        /* postconfiguration */
  ngx_http_create_main_conf,                    /* create main configuration */
  ngx_http_init_main_conf,                      /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};

ngx_module_t
ngx_http_rv2_module = {
  NGX_MODULE_V1,
  &ngx_http_rv2_module_ctx,                      /* module context */
  ngx_http_rv2_commands,                         /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  ngx_http_rv2_init_proc,                        /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  ngx_http_rv2_exit,                             /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};


  static ngx_int_t   
ngx_http_rv2_post_conf(ngx_conf_t *cf)
{
  fSTEP;

  ngx_http_rv2_main_conf_t   *mcf;
  ngx_http_core_main_conf_t *cmcf;

  ngx_http_variable_t       *v;
  ngx_int_t                  rvn;
  ngx_http_variable_t                 **rvip;
  ngx_http_variable_t                  *rvi;

  ngx_int_t                  i;
  ngx_int_t                  rc;
  

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);


  rvip = mcf->rvs->elts;
  rvn = mcf->rvs->nelts;

  for (i= 0; i < rvn; ++i){

    rvi = rvip[i];

    v = rvi;

    /* logc("initialize remote variable [%V] ", &v->name); */

    rc = ngx_http_rv2_init_var_mc(cf, v);
    if (NGX_OK != rc){
      return rc;
    }

  }

  return NGX_OK;
}

  static ngx_int_t
ngx_http_rv2_init_var_mc(ngx_conf_t *cf, ngx_http_variable_t *v)
{
  fSTEP;

  ngx_http_rv2_var_t             *ctx;
  ngx_str_t                      *vus;

  ngx_http_upstream_main_conf_t  *usmcf;
  ngx_http_upstream_srv_conf_t  **usscf;
  ngx_int_t                       usn;

  ngx_http_upstream_srv_conf_t   *ucf;
  ngx_str_t                      *usname;

  ngx_http_upstream_server_t     *srv;
  ngx_int_t                       srvn;
  ngx_str_t                      *srv_addr;

  ngx_str_t                      *mca;

  ngx_int_t                       i;
  ngx_int_t                       j;




  ctx = (ngx_http_rv2_var_t*)v->data;
  vus = &ctx->us_name;

  if (0 == vus->len) {
    logce("remote variable [%V] did not specify any upstream to use", &v->name);
    return NGX_ERROR;
  }

  /* logc("init mc address of [%V] which needs upstream [%V], v.ctx:[%p]", &v->name, vus, ctx); */


  usmcf = (ngx_http_upstream_main_conf_t*)
    ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);

  usscf = usmcf->upstreams.elts;
  usn   = usmcf->upstreams.nelts;

  /* logc("upstream number:%d", usn); */

  for (i= 0; i < usn; ++i){
    ucf = usscf[i];
    usname = &ucf->host;
    /* logc("us host:[%V]", usname); */

    if (vus->len != usname->len 
      || 0 != ngx_strncmp(vus->data, usname->data, vus->len)) { continue; }

    /* logc("RV[%V] uses upstream : [%V]", &v->name, usname); */

    srvn = ucf->servers->nelts;
    srv  = ucf->servers->elts;

    ctx->mc_addrs = ngx_array_create(cf->pool, srvn, sizeof(ngx_str_t));
    if (NULL == ctx->mc_addrs) {
      logce("create array of mc addresses failed");
      return NGX_ERROR;
    }


    for (j= 0; j < srvn; ++j){

      srv_addr = &srv[j].addrs->name;

      mca = ngx_array_push(ctx->mc_addrs);
      if (NULL == mca){
        return NGX_ERROR;
      }


      *mca = *srv_addr;

      /* logc("    added server addr:port[%V]", srv_addr); */
    }

    return NGX_OK;
  }

  logce("could not find upstream with name [%V] for remote variable [%V]", vus, &v->name);

  return NGX_ERROR;
}

  ngx_int_t
ngx_http_rv2_init_proc(ngx_cycle_t *cycle)
{
  ngx_http_core_main_conf_t     *cmcf;
  ngx_http_rv2_main_conf_t       *mcf;

  ngx_http_rv2_var_t *ctx;

  ngx_http_variable_t **vip;

  ngx_uint_t                     i;
  ngx_uint_t                     rc;


  cmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module);
  mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv2_module);


  vip = mcf->rvs->elts;

  for (i= 0; i < mcf->rvs->nelts; ++i){
    ctx = (ngx_http_rv2_var_t*)vip[i]->data;
    
    logy("connect mc for RV[%V]", &vip[i]->name);
    rc = ngx_http_rv2_connet_mc(cycle, &ctx->mcs, ctx->mc_addrs);
    if (NGX_OK != rc){
      return rc;
    }
  }

  return NGX_OK;
}

  static ngx_int_t 
ngx_http_rv2_connet_mc(ngx_cycle_t *cycle, ngx_array_t **mcs, ngx_array_t *addrs /* ngx_str_t */)
{
  /**
   * TODO thread safe?
   */

  ngx_uint_t     i;
  ngx_int_t      rc;
  ngx_str_t     *a;

  u_char  buf[0x100];

  memcache_t **mc;

  /* if (NULL == *mc){ */
    /* mc_free(*mc); */
  /* } */

  *mcs = ngx_array_create(cycle->pool, addrs->nelts, sizeof(memcache_t*));
  if (NULL == *mcs){
    return NGX_ERROR;
  }

  /* mc = (*mcs)->elts; */

  

  for (i= 0; i < addrs->nelts; ++i){
    mc = ngx_array_push(*mcs);
    *mc = mc_new();
    if (NULL == *mc){
      return NGX_ERROR;
    }
    a = addrs->elts;
    a = &a[i];

    memcpy(buf, a->data, a->len);
    buf[a->len] = 0;

    rc = mc_server_add4(*mc, (char*)buf);
    if (0 != rc){
      return NGX_ERROR;
    }

    if (NULL != cycle){
      logy("added addr[%s] to mc", buf);
    }

    /* TODO configurable timeout */
    mc_timeout(*mc, 5, 0);
  }

  return NGX_OK;
}

  static memcache_t *
get_mc(ngx_array_t *mcs, u_char *key, ngx_int_t len)
{
  ngx_uint_t hash = ngx_crc32_long(key, len);
  memcache_t **mc = mcs->elts;
  return mc[hash % mcs->nelts];
}

  static int
reconnect(ngx_array_t *mcs, u_char *key, ngx_int_t len)
{

  char buf[256];
  int l = 0;

  int rc = 0;

  ngx_uint_t hash = ngx_crc32_long(key, len);
  memcache_t **mc = mcs->elts;
  
  mc = &mc[hash % mcs->nelts];
  struct memcache_server *mcsvr = *(*mc)->servers;
  



  l = sprintf(buf, "%s:%s", mcsvr->hostname, mcsvr->port);
  buf[l] = 0;

  mc_free(*mc);

  *mc = mc_new();
  rc = mc_server_add4(*mc, (char*)buf);

  DEBUG("reconnect:%s, rc=%d", buf, rc);

  return 0;
}


/* TODO close all mc connection */
  static void
ngx_http_rv2_exit(ngx_cycle_t *cycle)
{
  /* ngx_http_rv2_main_conf_t *mcf; */

  /* mcf = (ngx_http_rv2_main_conf_t *)ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv2_module); */

  /* if (NULL != mcf->mc){ */
    /* mc_free(mcf->mc); */
  /* } */

  return;
}

  static ngx_int_t
ngx_http_rv2_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  /**
   * TODO error handling
   * TODO cachable variable value?
   */
  fSTEP;

  ngx_http_rv2_main_conf_t   *mcf;
  ngx_http_variable_value_t *mckvv;
  ngx_http_variable_value_t *mchvv;

  memcache_t *mc;

  ngx_http_rv2_var_t *ctx;

  char                      *buf   = NULL;
  char                      *mcv   = NULL;
  ngx_int_t                  mcvl;


  mcf = (ngx_http_rv2_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_rv2_module);


  ctx = (ngx_http_rv2_var_t*)data;


  mckvv = ngx_http_get_flushed_variable(r, ctx->mck_index);

  if (mckvv->not_found || !mckvv->valid) {
    return NGX_ERROR;
  }

  logr("the hashkey_index:%d", ctx->hashkey_index);

  mchvv = ngx_http_get_flushed_variable(r, ctx->hashkey_index);
  if (mchvv->not_found || !mchvv->valid) {
    return NGX_ERROR;
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP,  r->connection->log, 0,
    "mc key of remote variable:%v", mckvv);

  mc = get_mc(ctx->mcs, mchvv->data, mchvv->len);
  mcv = mc_aget(mc, (char*)mckvv->data, mckvv->len);
  if (NULL == mcv){
      reconnect(ctx->mcs, mckvv->data, mckvv->len);
    logr("get remote failed");
    *v = ngx_http_variable_null_value;

    return NGX_OK;
  }

  logr("got value from mc:[%s]", mcv);

  mcvl = ngx_strlen(mcv);

  buf = ngx_palloc(r->pool, sizeof(char) * (mcvl + 1));
  if (NULL == buf){
    return NGX_ERROR;
  }

  ngx_memcpy(buf, mcv, mcvl);
  buf[mcvl] = 0;
  free(mcv);

  v->data = (u_char*)buf;
  v->len  = mcvl;
  v->valid = 1;

  ngx_log_debug(NGX_LOG_DEBUG_HTTP,  r->connection->log, 0,
    "value of remote variable:%v", v);

  return NGX_OK;
}

  static void
ngx_http_rv2_set_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  /**
   * TODO error handling
   */
  fSTEP;

  ngx_http_rv2_main_conf_t   *mcf;
  ngx_http_variable_value_t *mchvv; /* hash variable */
  ngx_http_variable_value_t *mckvv;
  ngx_http_variable_value_t *mcv_vv;

  ngx_http_rv2_var_t        *ctx;

  u_char                    *buf;
  u_char                     val[32];

  memcache_t                *mc;

  ngx_int_t                  op      = RV_SET;
  ngx_int_t                  d       = 0;
  ngx_int_t                  rc      = -1;




  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv2_module);
  ctx = (ngx_http_rv2_var_t*)data;


  mcv_vv = &r->variables[ctx->mcv_index];


  ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "key index:%d", ctx->mck_index);

  mckvv = ngx_http_get_flushed_variable(r, ctx->mck_index);
  if (mckvv->not_found || !mckvv->valid) {
    return;
  }

  mchvv = ngx_http_get_flushed_variable(r, ctx->hashkey_index);
  if (mchvv->not_found || !mchvv->valid) {
    return;
  }

  ngx_log_debug(NGX_LOG_DEBUG_HTTP,  r->connection->log, 0, "mc key of remote variable:%v", mckvv);
  ngx_log_debug(NGX_LOG_DEBUG_HTTP,  r->connection->log, 0, "value to set:%v", v);

  op=ctx->op;

  mc = get_mc(ctx->mcs, mchvv->data, mchvv->len);

  logr("got mc:%p", mc);

  if (0 == v->len) {
    /* delete */
    logr("to delete:[%v]", mckvv);
    rc = mc_delete(mc, (char*)mckvv->data, mckvv->len, 0);
    mcv_vv->data = NULL;
    mcv_vv->len = 0;
    mcv_vv->valid = 0;
    mcv_vv->not_found = 1;
    return;
  }

  /* extract value of incr or decr */
  switch (op) {
    case RV_INCR:
    case RV_DECR:

      if (1 == v->len) {
        d = *v->data - '0';
      }
      else {
        d = ngx_atoi(v->data, v->len);
      }

      if (0 == d){
        return;
      }
  }

  switch (op) {
    case RV_INCR:
      rc = mc_incr(mc, (char*)mckvv->data, mckvv->len, d);
      break;

    case RV_DECR:
      rc = mc_decr(mc, (char*)mckvv->data, mckvv->len, d);
      break;
  }

  if (RV_SET != op){
    if (0 != rc) {
      buf = ngx_palloc(r->pool, 32);
      if (NULL == buf){
        mcv_vv->data = NULL;
        mcv_vv->len = 0;
        mcv_vv->valid = 0;
        mcv_vv->not_found = 1;
        return;
      }

      mcv_vv->len = sprintf((char*)buf, "%d", rc);
      mcv_vv->data = buf;
      mcv_vv->valid = 1;
      mcv_vv->not_found = 0;
      buf[mcv_vv->len] = 0;

      return;
    }
    else {
      /* set default value */
      d *= RV_INCR == op ? 1 : -1;
      d += ctx->v0;
      d = d < 0 ? 0 : d;
      op = RV_SET;

      rc = sprintf((char*)val, "%d", d);
      val[rc] = 0;
      v->data = val;
      v->len = rc;
      logr("rc = %d", rc);
    }
  }

  if  (RV_SET == op) {
    rc = mc_set(mc, (char*)mckvv->data, mckvv->len, (char*)v->data, v->len, 0, 0);
    if (0 != rc){

      ngx_log_error(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "set remote var failed:rc=%d", rc);

      reconnect(ctx->mcs, mckvv->data, mckvv->len);
      mcv_vv->data = NULL;
      mcv_vv->len = 0;
      mcv_vv->valid = 0;
      mcv_vv->not_found = 1;
      return;
    }
    mcv_vv->data = v->data;
    mcv_vv->len = v->len;
    mcv_vv->valid = 1;
    mcv_vv->not_found = 0;
  }

}

  static char *
ngx_http_rv2_hash_key_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_main_conf_t  *cmcf;
  ngx_http_core_loc_conf_t   *clcf;
  ngx_http_rv2_main_conf_t    *mcf;
  ngx_str_t                  *argvalues;
  ngx_str_t                   rvname;
  ngx_str_t                   hashname;

  ngx_http_variable_t        *rv;
  /* ngx_http_variable_t        *hash; */
  char* res;

  ngx_http_rv2_var_t *ctx;


  if (cf->args->nelts < 3){
    return "arguments number must not be less than 3";
  }

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);

  cmcf  = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

  argvalues = cf->args->elts;


  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[1], &rvname))) {
    return res;
  }

  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[2], &hashname))) {
    return res;
  }

  rv = ngx_http_add_variable(cf, &rvname, NGX_HTTP_VAR_CHANGEABLE);
  if (NULL == rv) {
    return "cant find remote variable";
  }

  ctx = (ngx_http_rv2_var_t*)rv->data;
  if (NULL == ctx) {
    return "remote variable has no context object";
  }

  /* logc("hashname:%V", &hashname); */
  ctx->hashkey_index = ngx_http_get_variable_index(cf, &hashname);
  if (NGX_ERROR == ctx->hashkey_index) {
    return "cant find hash key variable";
  }

  /* logc("key index:%d", ctx->mck_index); */
  /* logc("hash index:%d", ctx->hashkey_index); */

  return NGX_CONF_OK;
}
  static char *
ngx_http_rv2_remote_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_main_conf_t  *cmcf;
  ngx_http_core_loc_conf_t   *clcf;
  mcf_t                      *mcf;

  ngx_str_t                  *vals;
  ngx_int_t                   nval;

  ngx_str_t                   rv_name     = ngx_null_string;

  ngx_str_t                   usname      = ngx_null_string;
  ngx_str_t                   key         = ngx_null_string;
  ngx_str_t                   hash        = ngx_string("crc32")
  ngx_str_t                   hash_key    = ngx_null_string;
  ngx_str_t                   default_val = ngx_string("NULL");

  ngx_str_t                   ops         = ngx_null_string;
  ngx_str_t                   v0s         = ngx_null_string;
  ngx_int_t                   v0          = 0;
  ngx_http_rv2_op_t           op          = RV_SET;
  ngx_http_variable_t        *v;
  ngx_int_t                   vi;
  ngx_int_t                   i;

  rv_t                       *rv;

  ngx_http_variable_t       **ip;
  ngx_uint_t                  i;
  char                       *res         = NULL;


  nval = cf->args->nelts;
  if (nval < 2){
    return "require at least 1 argument";
  }

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);

  cmcf  = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

  vals = cf->args->elts;



  /* 1 required argument : var name */
  if (NULL != 
    (res = ngx_http_get_var_name_str(&vals[1], &rv_name))) {
    return res;
  }

  /* duplicate check */
  for (i= 0; i < mcf->rvs->nelts; ++i){
    rv = (rv_t*)ngx_array_get(mcf->rvs, i);
    if (ngx_str_eq(&rv_name, &rv->name)) {
      return "duplicate rv name declared";
    }
  }


  /* default values */
  /* hash_key set to keyname or user defined value */

  str_palloc(&usname, cf->pool, 64);
  str_palloc(&keyname, cf->pool, 64);


  if (!usname.data || !keyname.data) {
    return "allocating buffers failed";
  }

  usname.len      = snprint(strpr(usname)     , "%.*s_us" , strp(rv->name));
  keyname.len     = snprint(strpr(keyname)    , "%.*s_key", strp(rv->name));


  /* read command arguments */

  for (i = 2; i < nval; ++i) {
    str_switch(&vals[i]) {
      str_case_pre("upstream=") {
        usname = *str_case_remainder();
        break;
      }

      str_case_pre("key=") {
        keyname = *str_case_remainder();
        if (!hash_key.len)
          hash_key = keyname;
        break;
      }

      str_case_pre("hash=") {
        hash = *str_case_remainder();
        break;
      }

      str_case_pre("hash_key=") {
        hash_key = *str_case_remainder();
        break;
      }

      str_case_pre("default_val=") {
        default_val = *str_case_remainder();
        break;
      }

      str_default() {
      }
    }
  }










  if (NULL != 
    (res = ngx_http_get_var_name_str(&vals[2], &us_var_name))) {
    return res;
  }



  rv = ngx_pcalloc(cf->pool, sizeof(rv_t));
  if (NULL == rv) {
    return "allocating rv context failed";
  }

  rv->name = rv_name;

  rv->us_name_idx = ngx_http_get_variable_index(cf, &us_var_name);
  if (NGX_ERROR == rv->us_name_idx) {
    return "variable for upstream name is not set";
  }


  if (nval == 3) {
    rv->hash_str = hash_crc32;
    rv->hash_type = HASH_CRC32;
    rv->hash_func = ngx_crc32_long;
  }

  if (nval >= 3) { /* hash method */
    rv->hash_str = vals[3];

    if (ngx_str_eq(&hash_crc32, &rv->hash_str)) {
      rv->hash_type = HASH_CRC32;
      rv->hash_func = ngx_crc32_long;
    }
    else if (ngx_str_eq(&hash_crc32_short, &rv->hash_str)) {
      rv->hash_type = HASH_CRC32_SHORT;
      rv->hash_func = ngx_crc32_short;
    }
    /* else if (ngx_str_eq(&hash_crc32_short, &rv->hash_str)) { */
      /* TODO */
    /* } */
    else {
      return "illegal hash method";
    }
  }

  

  if (nval >= 4) { /* hash key var */

  }


  usname = vals[3];

  if (cf->args->nelts > 4 /* including command */) {
    ops = vals[4];
  }

  if (cf->args->nelts > 5 /* including command */) {
    v0s = vals[4];
    v0 = ngx_atoi(v0s.data, v0s.len);
    if (NGX_ERROR == v0) v0 = 0;
    if (0 > v0) v0 = 0;
  }


  for (i= 0; i < mcf->rvs->nelts; ++i){
    ip = mcf->rvs->elts;
    v = ip[i];
    if (0 == ngx_strncmp(v->name.data, rv_name.data, rv_name.len)) {
      return "found duplicated RV name";
    }
  }


  if (NULL != ops.data){
    if (0 == ngx_strncmp(ops.data, rv_op_incr.data, rv_op_incr.len)) { op = RV_INCR; }
    if (0 == ngx_strncmp(ops.data, rv_op_decr.data, rv_op_decr.len)) { op = RV_DECR; }
  }



  rv = ngx_pcalloc(cf->pool, sizeof(ngx_http_rv2_var_t));
  if (NULL == rv){
    return "create variable context failed";
  }

  rv->mck_index = ngx_http_get_variable_index(cf, &us_var_name);
  rv->us_var_name = usname;
  rv->hashkey_index = rv->mck_index;
  rv->op = op;
  rv->v0 = v0;


  v = ngx_http_add_variable(cf, &rv_name, NGX_HTTP_VAR_CHANGEABLE);
  vi = ngx_http_get_variable_index(cf, &rv_name);


  v->get_handler = ngx_http_rv2_get_handler;
  v->set_handler = ngx_http_rv2_set_handler;
  v->data = (uintptr_t)rv;


  /* logc("RV[%V] rv:[%p]", &v->name, v->data); */

  rv->mcv_index = vi;


  ip = ngx_array_push(mcf->rvs);
  if (NULL == ip){
    return "pushing remote variable to main conf failed";
  }
  *ip = v;


  return NGX_CONF_OK;
}

  static void *
ngx_http_create_main_conf (ngx_conf_t *cf)
{
  fSTEP;

  ngx_http_rv2_main_conf_t * mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_rv2_main_conf_t));
  memset(mcf, 0, sizeof(ngx_http_rv2_main_conf_t));

  mcf->rvs = ngx_array_create(cf->pool, 20, sizeof(ngx_http_variable_t*));
  if (NULL == mcf->rvs){
    return NULL;
  }

  /* logc("create main config done"); */
  return mcf;
}

  static char*
ngx_http_init_main_conf (ngx_conf_t *cf, void *conf)
{
  return NULL;
}

