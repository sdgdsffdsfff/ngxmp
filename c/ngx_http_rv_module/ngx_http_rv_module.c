/**
 * TODO add check_mdb to other query too
 * TODO duplicate remote variable check
 * TODO crc32 support only for now.
 */

/**
 * 
 * remote_var $value $key upstream_name [operation] [on-empty];
 *    operation is optional
 *    operation can be : 
 *        incr 
 *        decr
 *    on-empty is optional: a number to initialize empty key.
 *        
 *    if operation parameter presented, the 'set' directive to $value works like 'incr' or 'decr' 
 *
 * set $value ***; # trigger remote set
 * set $** $value; # trigger remote get
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <memcache.h>

#define IS_FATAL 1
#define IS_ERROR 1
/* #define IS_DEBUG 1 */
/* #define IS_STEP  1 */
/* #define IS_TRACE 1 */

#include  "debug.h"
#include  "ngx_util.h"


typedef enum {
  RV_SET  = 0, 
  RV_INCR, 
  RV_DECR, 
  RV_UNKNOW
} 
ngx_http_rv_op_t;


typedef struct memcache memcache_t;

typedef struct {
  ngx_array_t *rvs;      /* ngx_http_variable_t*, remote variable */
}
ngx_http_rv_main_conf_t;


typedef struct {
  ngx_str_t                     us_name;        /* upstream name  */

  ngx_int_t                     us_index;       /* upstream index */
  ngx_http_upstream_srv_conf_t *us;

  ngx_array_t                  *mc_addrs;
  ngx_array_t                  *mcs;            /* memcache_t  */

  ngx_int_t                     mck_index;
  ngx_int_t                     mcv_index;

  ngx_int_t                     hashkey_index;  /* which varaible to use as hash string */

  void                         *hash_method;    /* not used yet for now */
  

  ngx_int_t                     op;             /* operation type [set|incr|decr] */
  ngx_int_t                     v0;             /* default value for incr/decr */
}
ngx_http_rv_vctx_t;


static void      *ngx_http_create_main_conf     (ngx_conf_t *cf                                                           );
static char      *ngx_http_init_main_conf       (ngx_conf_t *cf, void *conf                                               );
static ngx_int_t  ngx_http_rv_init_proc         (ngx_cycle_t *cycle                                                       );
static void       ngx_http_rv_exit              (ngx_cycle_t *cycle                                                       );

static char      *ngx_http_rv_remote_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf                           );
static char      *ngx_http_rv_hash_key_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf                           );
static ngx_int_t  ngx_http_rv_post_conf         (ngx_conf_t *cf                                                           );
 
static ngx_int_t  ngx_http_rv_init_var_mc       (ngx_conf_t *cf, ngx_http_variable_t *v                                   );
static ngx_int_t  ngx_http_rv_connet_mc         (ngx_cycle_t *cycle, ngx_array_t **mcs, ngx_array_t *addrs /* ngx_str_t */);






static const ngx_str_t rv_op_incr = ngx_string("incr");
static const ngx_str_t rv_op_decr = ngx_string("decr");





static ngx_command_t
ngx_http_rv_commands[] = {

  { ngx_string("remote_var"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_2MORE,
    ngx_http_rv_remote_var_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_hash_key"),
    NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE2,
    ngx_http_rv_hash_key_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_rv_module_ctx = {
  NULL,                                         /* preconfiguration */
  ngx_http_rv_post_conf,                        /* postconfiguration */
  ngx_http_create_main_conf,                    /* create main configuration */
  ngx_http_init_main_conf,                      /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};

ngx_module_t
ngx_http_rv_module = {
  NGX_MODULE_V1,
  &ngx_http_rv_module_ctx,                      /* module context */
  ngx_http_rv_commands,                         /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  ngx_http_rv_init_proc,                        /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  ngx_http_rv_exit,                             /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};


  static ngx_int_t   
ngx_http_rv_post_conf(ngx_conf_t *cf)
{
  fSTEP;

  ngx_http_rv_main_conf_t   *mcf;
  ngx_http_core_main_conf_t *cmcf;

  ngx_http_variable_t       *v;
  ngx_int_t                  rvn;
  ngx_http_variable_t                 **rvip;
  ngx_http_variable_t                  *rvi;

  ngx_int_t                  i;
  ngx_int_t                  rc;
  

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv_module);
  cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);


  rvip = mcf->rvs->elts;
  rvn = mcf->rvs->nelts;

  for (i= 0; i < rvn; ++i){

    rvi = rvip[i];

    v = rvi;

    /* logc("initialize remote variable [%V] ", &v->name); */

    rc = ngx_http_rv_init_var_mc(cf, v);
    if (NGX_OK != rc){
      return rc;
    }

  }

  return NGX_OK;
}

  static ngx_int_t
ngx_http_rv_init_var_mc(ngx_conf_t *cf, ngx_http_variable_t *v)
{
  fSTEP;

  ngx_http_rv_vctx_t             *ctx;
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




  ctx = (ngx_http_rv_vctx_t*)v->data;
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
ngx_http_rv_init_proc(ngx_cycle_t *cycle)
{
  ngx_http_core_main_conf_t     *cmcf;
  ngx_http_rv_main_conf_t       *mcf;

  ngx_http_rv_vctx_t *ctx;

  ngx_http_variable_t **vip;

  ngx_uint_t                     i;
  ngx_uint_t                     rc;


  cmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module);
  mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv_module);


  vip = mcf->rvs->elts;

  for (i= 0; i < mcf->rvs->nelts; ++i){
    ctx = (ngx_http_rv_vctx_t*)vip[i]->data;
    
    logy("connect mc for RV[%V]", &vip[i]->name);
    rc = ngx_http_rv_connet_mc(cycle, &ctx->mcs, ctx->mc_addrs);
    if (NGX_OK != rc){
      return rc;
    }
  }

  return NGX_OK;
}

  static ngx_int_t 
ngx_http_rv_connet_mc(ngx_cycle_t *cycle, ngx_array_t **mcs, ngx_array_t *addrs /* ngx_str_t */)
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
ngx_http_rv_exit(ngx_cycle_t *cycle)
{
  /* ngx_http_rv_main_conf_t *mcf; */

  /* mcf = (ngx_http_rv_main_conf_t *)ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv_module); */

  /* if (NULL != mcf->mc){ */
    /* mc_free(mcf->mc); */
  /* } */

  return;
}

  static ngx_int_t
ngx_http_rv_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  /**
   * TODO error handling
   * TODO cachable variable value?
   */
  fSTEP;

  ngx_http_rv_main_conf_t   *mcf;
  ngx_http_variable_value_t *mckvv;
  ngx_http_variable_value_t *mchvv;

  memcache_t *mc;

  ngx_http_rv_vctx_t *ctx;

  char                      *buf   = NULL;
  char                      *mcv   = NULL;
  ngx_int_t                  mcvl;


  mcf = (ngx_http_rv_main_conf_t*)ngx_http_get_module_main_conf(r, ngx_http_rv_module);


  ctx = (ngx_http_rv_vctx_t*)data;


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
ngx_http_rv_set_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data)
{
  /**
   * TODO error handling
   */
  fSTEP;

  ngx_http_rv_main_conf_t   *mcf;
  ngx_http_variable_value_t *mchvv; /* hash variable */
  ngx_http_variable_value_t *mckvv;
  ngx_http_variable_value_t *mcv_vv;

  ngx_http_rv_vctx_t        *ctx;

  u_char                    *buf;
  u_char                     val[32];

  memcache_t                *mc;

  ngx_int_t                  op      = RV_SET;
  ngx_int_t                  d       = 0;
  ngx_int_t                  rc      = -1;




  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv_module);
  ctx = (ngx_http_rv_vctx_t*)data;


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
ngx_http_rv_hash_key_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_main_conf_t  *cmcf;
  ngx_http_core_loc_conf_t   *clcf;
  ngx_http_rv_main_conf_t    *mcf;
  ngx_str_t                  *argvalues;
  ngx_str_t                   rvname;
  ngx_str_t                   hashname;

  ngx_http_variable_t        *rv;
  /* ngx_http_variable_t        *hash; */
  char* res;

  ngx_http_rv_vctx_t *ctx;


  if (cf->args->nelts < 3){
    return "arguments number must not be less than 3";
  }

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv_module);

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

  ctx = (ngx_http_rv_vctx_t*)rv->data;
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
ngx_http_rv_remote_var_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_core_main_conf_t  *cmcf;
  ngx_http_core_loc_conf_t   *clcf;
  ngx_http_rv_main_conf_t    *mcf;

  ngx_str_t                  *argvalues;
  ngx_str_t                   keyname;
  ngx_str_t                   valname;
  ngx_str_t                   usname;
  ngx_str_t                   ops = ngx_null_string;
  ngx_str_t                   v0s = ngx_null_string;
  ngx_int_t                   v0 = 0;
  ngx_http_rv_op_t            op = RV_SET;
  ngx_http_variable_t        *v;
  ngx_int_t                   vi;
  ngx_http_rv_vctx_t         *ctx;
  ngx_http_variable_t       **ip;
  ngx_uint_t i;
  char                       *res       = NULL;


  if (cf->args->nelts < 3){
    return "arguments number must not be less than 3";
  }

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv_module);

  cmcf  = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
  clcf  = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);

  argvalues = cf->args->elts;


  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[1], &valname))) {
    return res;
  }

  if (NULL != 
    (res = ngx_http_get_var_name_str(&argvalues[2], &keyname))) {
    return res;
  }

  usname = argvalues[3];

  if (cf->args->nelts > 4 /* including command */) {
    ops = argvalues[4];
  }

  if (cf->args->nelts > 5 /* including command */) {
    v0s = argvalues[4];
    v0 = ngx_atoi(v0s.data, v0s.len);
    if (NGX_ERROR == v0) v0 = 0;
    if (0 > v0) v0 = 0;
  }


  for (i= 0; i < mcf->rvs->nelts; ++i){
    ip = mcf->rvs->elts;
    v = ip[i];
    if (0 == ngx_strncmp(v->name.data, valname.data, valname.len)) {
      return "found duplicated RV name";
    }
  }


  if (NULL != ops.data){
    if (0 == ngx_strncmp(ops.data, rv_op_incr.data, rv_op_incr.len)) { op = RV_INCR; }
    if (0 == ngx_strncmp(ops.data, rv_op_decr.data, rv_op_decr.len)) { op = RV_DECR; }
  }



  ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_rv_vctx_t));
  if (NULL == ctx){
    return "create variable context failed";
  }

  ctx->mck_index = ngx_http_get_variable_index(cf, &keyname);
  ctx->us_name = usname;
  ctx->hashkey_index = ctx->mck_index;
  ctx->op = op;
  ctx->v0 = v0;


  v = ngx_http_add_variable(cf, &valname, NGX_HTTP_VAR_CHANGEABLE);
  vi = ngx_http_get_variable_index(cf, &valname);


  v->get_handler = ngx_http_rv_get_handler;
  v->set_handler = ngx_http_rv_set_handler;
  v->data = (uintptr_t)ctx;


  /* logc("RV[%V] ctx:[%p]", &v->name, v->data); */

  ctx->mcv_index = vi;


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

  ngx_http_rv_main_conf_t * mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_rv_main_conf_t));
  memset(mcf, 0, sizeof(ngx_http_rv_main_conf_t));

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

