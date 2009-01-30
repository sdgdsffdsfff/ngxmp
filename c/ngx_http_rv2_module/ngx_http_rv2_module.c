/**
 * TODO add check_mdb to other query too
 * TODO duplicate remote variable check
 * TODO crc32 support only for now.
 */

/**
 * rv_dec $varname_var 
 *        upstream="us_name_expr"             # optional   default = ${varname_var}_us
 *        key=$keyname_var                    # optional   default = ${varname_var}_key
 *        hash=hash_method_str                # optional   default = crc32
 *        hash_key=$hash_key_var              # optional   default = $keyname
 *        default_val="expr";                 # optional   default = $varname_var
 *
 *
 * rv_timeout_read varname timeout;
 *
 * rv_get $value $varname $rc;                # rc is optional, value can be [success|timeout|empty|invalid_us|nosuch_us|internal|error]
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

#define rMCF() ngx_http_get_module_main_conf(r,THIS)
#define rLCF() ngx_http_get_module_loc_conf(r,THIS)

typedef struct {
  ngx_array_t  *codes;        /* uintptr_t */
} 
ngx_http_rewrite_loc_conf_pseudo_t;

typedef enum {
  RV_SET  = 0, 
  RV_GET, 
  RV_INCR, 
  RV_DECR, 
  RV_UNKNOW
} 
ngx_http_rv2_op_t;

typedef struct {
  ngx_str_t addr;
  memcached_st *mc;
}
ngx_http_rv2_mcd_t;

typedef struct {
  ngx_str_t name;
  ngx_array_t *mcd_array;                         /* ngx_http_rv2_mcd_t */
}
ngx_http_rv2_us_t;

typedef struct {
  ngx_array_t *lengths;
  ngx_array_t *values;
}
ngx_http_rv2_eval_t;

typedef struct {
  ngx_array_t *rvs;       /* ngx_http_rv2_t    */
  ngx_array_t *uss;       /* ngx_http_rv2_us_t */
  hash_t      *us_hash;
}
ngx_http_rv2_main_conf_t;


typedef uint32_t (*ngx_http_rv2_hash_func_t)(u_char*, size_t);

typedef struct {
  ngx_str_t                name;

  ngx_int_t                key_idx;

  ngx_str_t                hash_str;
  ngx_int_t                hash_type;
  ngx_http_rv2_hash_func_t hash_func;

  ngx_int_t                hash_key_idx;


  ngx_http_rv2_eval_t      us_name_e;
  ngx_http_rv2_eval_t      default_val_e;


  /* TODO time outs */

  /* TODO rc */
}
ngx_http_rv2_t;

typedef struct {
  ngx_int_t vidx;
  ngx_http_rv2_t *rv;
  ngx_int_t rcidx;
}
ngx_http_rv2_op_data_t;

typedef struct {
  ngx_http_script_code_pt code;
  ngx_http_rv2_op_data_t data;
}
ngx_http_rv2_get_code_t;

static void           *ngx_http_rv2_create_main_conf       (ngx_conf_t *cf);
static char           *ngx_http_rv2_init_main_conf         (ngx_conf_t *cf, void *conf);
static ngx_int_t       ngx_http_rv2_init_proc              (ngx_cycle_t *cycle);
 
static char           *ngx_http_rv2_dec_command            (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t       ngx_http_rv2_post_conf              (ngx_conf_t *cf);
static char           *ngx_http_rv2_get_command            (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_http_rv2_t *ngx_http_rv2_find                   (ngx_conf_t *cf, ngx_str_t *name);
static ngx_int_t       ngx_http_rv2_existed                (ngx_conf_t *cf, ngx_str_t *name);
static ngx_int_t       ngx_http_rv2_unreachable_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);
static void            ngx_http_rv2_get_code               (ngx_http_script_engine_t *e);
 

/* FUNC_DEC_START */
static char      *ngx_http_rv2_setter_code(ngx_conf_t *cf, ngx_http_variable_t *v);
static ngx_int_t  ngx_http_rv2_eval_usname(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t *v);
/* FUNC_DEC_END */


#define HASH_CRC32         0x01
#define HASH_CRC32_SHORT   0x02
#define HASH_5381          0x03


static const ngx_str_t hash_crc32       = ngx_string("crc32");
static const ngx_str_t hash_crc32_short = ngx_string("crc32_short");
static const ngx_str_t hash_5381        = ngx_string("5381");


static ngx_str_t rv2_err_success    = ngx_string("success");
static ngx_str_t rv2_err_error      = ngx_string("error");
static ngx_str_t rv2_err_invalid_us = ngx_string("invalid_us");
static ngx_str_t rv2_err_nosuch_us  = ngx_string("nosuch_us");
static ngx_str_t rv2_err_internal   = ngx_string("internal");
static ngx_str_t rv2_err_hashkey    = ngx_string("hashkey");
static ngx_str_t rv2_err_key        = ngx_string("key");


extern ngx_module_t  ngx_http_rewrite_module;


static ngx_command_t
ngx_http_rv2_commands[] = {

  { ngx_string("rv_dec"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
    ngx_http_rv2_dec_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_get"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_2MORE,
    ngx_http_rv2_get_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  ngx_null_command
};

static ngx_http_module_t
ngx_http_rv2_module_ctx = {
  NULL,                                         /* preconfiguration */
  ngx_http_rv2_post_conf,                       /* postconfiguration */
  ngx_http_rv2_create_main_conf,                    /* create main configuration */
  ngx_http_rv2_init_main_conf,                      /* init main configuration */
  NULL,                                         /* create server configuration */
  NULL,                                         /* merge server configuration */
  NULL,                                         /* create location configuration */
  NULL,                                         /* merge location configuration */
};

ngx_module_t
ngx_http_rv2_module = {
  NGX_MODULE_V1,
  &ngx_http_rv2_module_ctx,                     /* module context */
  ngx_http_rv2_commands,                        /* module directives */
  NGX_HTTP_MODULE,                              /* module type */
  NULL,                                         /* init master */
  NULL,                                         /* init module */
  ngx_http_rv2_init_proc,                       /* init process */
  NULL,                                         /* init thread */
  NULL,                                         /* exit thread */
  NULL,                            /* exit process */
  NULL,                                         /* exit master */
  NGX_MODULE_V1_PADDING
};





  static ngx_int_t   
ngx_http_rv2_post_conf(ngx_conf_t *cf)
{
  ngx_http_rv2_main_conf_t      *mcf;
  ngx_http_upstream_main_conf_t *usmcf;

  ngx_http_upstream_srv_conf_t  *usscf;
  ngx_http_upstream_server_t    *srv;

  ngx_http_rv2_us_t                      *us;
  ngx_http_rv2_mcd_t                     *mcd;

  ngx_uint_t                     i;
  ngx_uint_t                     j;
  

  mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  usmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);


  mcf->uss = ngx_array_create(cf->pool, usmcf->upstreams.nelts, sizeof(ngx_http_rv2_us_t));
  if (NULL == mcf->uss) {
    logce("failed to allocate");
    return NGX_ERROR;
  }

  for (i= 0; i < usmcf->upstreams.nelts; ++i){
    usscf = *(ngx_http_upstream_srv_conf_t**)(ngx_array_get(&usmcf->upstreams, i));
    us = ngx_array_push(mcf->uss);

    us->name = usscf->host;
    logc("upstream %V added", &us->name);

    us->mcd_array = ngx_array_create(cf->pool, usscf->servers->nelts, sizeof(ngx_http_rv2_mcd_t));
    if (NULL == us->mcd_array) {
      logce("failed to allocate");
      return NGX_ERROR;
    }

    for (j= 0; j < usscf->servers->nelts; ++j){
      srv = ngx_array_get(usscf->servers, j);
      mcd = ngx_array_push(us->mcd_array);
      mcd->addr = srv->addrs->name;
      logc(" server %V added", &mcd->addr);
    }
  }



  /* create hash */
  mcf->us_hash = hash_create(cf->pool, mcf->uss->nelts*2);
  if (NULL == mcf->us_hash) {
    logce("creating upstream hash failed");
    return NGX_ERROR;
  }

  for (i= 0; i < mcf->uss->nelts; ++i){

    us = ngx_array_get(mcf->uss, i);

    logce("to add :%V", &us->name);
    
    if (NGX_OK != hash_padd(cf->pool, mcf->us_hash, &us->name, (intptr_t)us)) {
      logce("failed to add hash element %V", &us->name);
      return NGX_ERROR;
    }
  }


  return NGX_OK;
}


  ngx_int_t
ngx_http_rv2_init_proc(ngx_cycle_t *cycle)
{
  /* ngx_http_core_main_conf_t *cmcf; */
  ngx_http_rv2_main_conf_t  *mcf;
  ngx_http_rv2_us_t                  *us;
  ngx_http_rv2_mcd_t                 *mcd;

  ngx_uint_t                 i;
  ngx_uint_t                 j;
  ngx_int_t                  rc;


  /* cmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module); */
  mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv2_module);

  for (i= 0; i < mcf->uss->nelts; ++i){
    us = (ngx_http_rv2_us_t*)ngx_array_get(mcf->uss, i);

    for (j= 0; j < us->mcd_array->nelts; ++j){
      mcd = (ngx_http_rv2_mcd_t*)ngx_array_get(us->mcd_array, j);

      rc = ngx_mcd_connect(cycle, &mcd->addr, &mcd->mc);
      if (NGX_ERROR == rc) {
        logye("failed to connect to mc : %V", &mcd->addr);
        /* return NGX_ERROR; */
      }
    }
  }

  return NGX_OK;
}

  static memcached_st *
get_mc(ngx_array_t *mcs, u_char *key, ngx_int_t len)
{
  ngx_uint_t hash = ngx_crc32_long(key, len);
  memcached_st **mc = mcs->elts;
  return mc[hash % mcs->nelts];
}


  static char *
ngx_http_rv2_dec_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  mcf_t                     *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);

  ngx_str_t                 *vals;
  ngx_uint_t                 nval;

  ngx_http_rv2_t                      *rv;
  ngx_str_t                  rv_name     = ngx_null_string;

  ngx_str_t                  usname      = ngx_null_string;
  ngx_str_t                  keyname     = ngx_null_string;
  ngx_str_t                  hash        = ngx_string("crc32");
  ngx_str_t                  hash_key    = ngx_null_string;
  ngx_str_t                  default_val = ngx_string("NULL");

  ngx_uint_t                 i;
  ngx_http_script_compile_t  sc;
  char                      *res         = NULL;
  ngx_int_t                  rc          = 0;


  nval = cf->args->nelts;
  if (nval < 2){
    return "require at least 1 argument";
  }

  vals = cf->args->elts;



  /* 1 required argument : var name */
  if (NULL != 
    (res = ngx_http_get_var_name_str(&vals[1], &rv_name))) {
    return res;
  }

  /* duplicate check */
  if (ngx_http_rv2_existed(cf, &rv_name)) {
    return "duplicate rv name declared";
  }

  rv = ngx_array_push(mcf->rvs);
  if (NULL == rv) {
    return "allocating rv context failed";
  }

  rv->name = rv_name;




  /* default values */
  /* hash_key set to keyname or user defined value */

  str_palloc(&usname, cf->pool, 64);
  str_palloc(&keyname, cf->pool, 64);
  str_palloc(&hash_key, cf->pool, 64);


  if (!usname.data || !keyname.data) {
    return "allocating buffers failed";
  }

  usname.len      = snprintf((char*)strpr(usname)     , "%.*s_us" , strp(rv->name));
  keyname.len     = snprintf((char*)strpr(keyname)    , "%.*s_key", strp(rv->name));
  hash_key.len     = snprintf((char*)strpr(hash_key)    , "%.*s_key", strp(rv->name));


  /* read other command arguments */
  for (i = 2; i < nval; ++i) {
    str_switch(&vals[i]) {
      str_case_pre("upstream=") {
        usname = *str_case_remainder();
        break;
      }

      str_case_pre("key=") {
        keyname = *str_case_remainder();
        if (NULL != (
              res = ngx_http_get_var_name_str(&keyname, &keyname))) {
          return res;
        }
        if (!hash_key.len) { /* take keyname as default value of hash_key */
          hash_key = keyname;
        }
        break;
      }

      str_case_pre("hash=") {
        hash = *str_case_remainder();
        break;
      }

      str_case_pre("hash_key=") {
        hash_key = *str_case_remainder();
        if (NULL != (
              res = ngx_http_get_var_name_str(&hash_key, &hash_key))) {
          return res;
        }
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


  logce("rv name:%V, us name:%V, key name:%V, hash_key:%V", &rv->name, &usname, &keyname, &hash_key);




  /* upstream name */

  ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

  sc.cf = cf;
  sc.source = &usname;
  sc.lengths = &rv->us_name_e.lengths;
  sc.values = &rv->us_name_e.values;
  sc.complete_lengths = 1;
  sc.complete_values = 1;

  rc = ngx_http_script_compile(&sc);
  if (NGX_OK != rc) {
    return "parse upstream name expression failed";
  }


  /* key */

  rv->key_idx = ngx_http_get_variable_index(cf, &keyname);
  if (NGX_ERROR == rv->key_idx) {
    return "key name variable is not set";
  }


  /* hash method */

  rv->hash_str = hash;
  if (ngx_str_eq(&hash, &hash_crc32)) {
    rv->hash_type = HASH_CRC32;
    rv->hash_func = ngx_crc32_long;
  }
  else if (ngx_str_eq(&hash, &hash_crc32_short)) {
    rv->hash_type = HASH_CRC32_SHORT;
    rv->hash_func = ngx_crc32_short;
  }
  /* else if (ngx_str_eq(&hash, &hash_5381)) { */
    /* rv->hash_type = HASH_5381; */
    /* rv->hash_func = ngx_crc32_short; */
  /* } */
  else {
    return "unsupported hash method";
  }


  /* hash key */

  rv->hash_key_idx = ngx_http_get_variable_index(cf, &hash_key);
  if (NGX_ERROR == rv->hash_key_idx) {
    return "hash key variable is not set";
  }


  /* default value */

  ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

  sc.cf = cf;
  sc.source = &default_val;
  sc.lengths = &rv->default_val_e.lengths;
  sc.values = &rv->default_val_e.values;

  rc = ngx_http_script_compile(&sc);
  if (NGX_OK != rc) {
    return "parsing default value expression failed";
  }

  return NGX_CONF_OK;
}

  static char *
ngx_http_rv2_get_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_rewrite_loc_conf_pseudo_t *rlcf;

  ngx_http_variable_t                *rcv = NULL;
  ngx_http_rv2_op_data_t             *rvod;
  ngx_str_t                          *vals;
  ngx_uint_t                          nval;

  ngx_str_t                           val;
  ngx_str_t                           rvname;
  ngx_str_t                           rcname   = ngx_null_string;

  ngx_int_t                           valindex;

  ngx_http_rv2_get_code_t            *code;

  ngx_http_variable_t                *v;
  char                               *res;

  rlcf   = ngx_http_conf_get_module_loc_conf(cf, ngx_http_rewrite_module);


  nval = cf->args->nelts;
  vals = cf->args->elts;

  if (NULL != (
        res = ngx_http_get_var_name_str(&vals[1], &val))) {
    return res;
  }

  if (NULL != (
        res = ngx_http_get_var_name_str(&vals[2], &rvname))) {
    return res;
  }

  if (4 == nval) {
    if (NULL != (
          res = ngx_http_get_var_name_str(&vals[3], &rcname))) {
      return res;
    }
  }



  v = ngx_http_add_variable(cf, &val, NGX_HTTP_VAR_CHANGEABLE);
  if (NULL == v) {
    logce("invalid variable name :%V", &val);
    return NGX_CONF_ERROR;
  }

  valindex = ngx_http_get_variable_index(cf, &val);

  if (NULL == v->get_handler) { /* variable is not declared */
    /* copied from rewrite module */
    if (v->get_handler == NULL
        && ngx_strncasecmp(val.data, (u_char *) "http_", 5) != 0
        && ngx_strncasecmp(val.data, (u_char *) "sent_http_", 10) != 0
        && ngx_strncasecmp(val.data, (u_char *) "upstream_http_", 14) != 0)
    {
        v->get_handler = ngx_http_rv2_unreachable_get_handler;
        v->data = valindex;
    }
  }


  code = ngx_http_script_start_code(cf->pool, &rlcf->codes, sizeof(ngx_http_rv2_get_code_t));
  if (NULL == code) {
    logce("failed to create 'get' code");
    return NGX_CONF_ERROR;
  }


  code->code = ngx_http_rv2_get_code;

  rvod = &code->data;

  rvod->vidx = valindex;


  rvod->rv = ngx_http_rv2_find(cf, &rvname);
  if (NULL == rvod->rv){
    logce("invalid rv name : %V", &rvname);
    return NGX_CONF_ERROR;
  }


  if (0 < rcname.len) {
    rcv = ngx_http_add_variable(cf, &rcname, NGX_HTTP_VAR_CHANGEABLE);
    if (NULL == rcv){
      logce("failed to add varaible %V", &rcname);
      return NGX_CONF_ERROR;
    }

    rvod->rcidx = ngx_http_get_variable_index(cf, &rcname);
  }
  else {
    rvod->rcidx = NGX_CONF_UNSET;
  }



  if (NGX_CONF_OK != ngx_http_rv2_setter_code(cf, v)) {
    return NGX_CONF_ERROR;
  }


  return NGX_CONF_OK;
}

  static char *
ngx_http_rv2_setter_code(ngx_conf_t *cf, ngx_http_variable_t *v)
{
  ngx_http_rewrite_loc_conf_pseudo_t *rlcf;
  ngx_http_script_var_handler_code_t *vhcode;
  ngx_http_script_var_code_t         *vcode;
  ngx_int_t valindex;




  rlcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_rewrite_module);
  valindex = ngx_http_get_variable_index(cf, &v->name);

  if (v->set_handler) {
    vhcode = ngx_http_script_start_code(cf->pool, &rlcf->codes,
        sizeof(ngx_http_script_var_handler_code_t));
    if (vhcode == NULL) {
      return NGX_CONF_ERROR;
    }

    vhcode->code = ngx_http_script_var_set_handler_code;
    vhcode->handler = v->set_handler;
    vhcode->data = v->data;

    return NGX_CONF_OK;
  }

  vcode = ngx_http_script_start_code(cf->pool, &rlcf->codes,
      sizeof(ngx_http_script_var_code_t));
  if (vcode == NULL) {
    return NGX_CONF_ERROR;
  }

  vcode->code = ngx_http_script_set_var_code;
  vcode->index = (uintptr_t)valindex;

  return NGX_CONF_OK;
}

  static void *
ngx_http_rv2_create_main_conf (ngx_conf_t *cf)
{
  fSTEPc();

  ngx_http_rv2_main_conf_t * mcf = ngx_pcalloc(cf->pool, sizeof(ngx_http_rv2_main_conf_t));
  memset(mcf, 0, sizeof(ngx_http_rv2_main_conf_t));

  mcf->rvs = ngx_array_create(cf->pool, 20, sizeof(ngx_http_rv2_t));
  if (NULL == mcf->rvs){
    return NULL;
  }

  return mcf;
}

  static char*
ngx_http_rv2_init_main_conf (ngx_conf_t *cf, void *conf)
{
  return NULL;
}


  static ngx_int_t
ngx_http_rv2_existed(ngx_conf_t *cf, ngx_str_t *name)
{
  mcf_t          *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  ngx_http_rv2_t *rv;
  ngx_uint_t      i;


  for (i= 0; i < mcf->rvs->nelts; ++i){
    rv = ngx_array_get(mcf->rvs, i);
    if (ngx_str_eq(name, &rv->name)) {
      return 1;
    }
  }

  return 0;
}

  static ngx_int_t
ngx_http_rv2_get_index(ngx_conf_t *cf, ngx_str_t *name)
{
  mcf_t              *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  ngx_http_rv2_t *rv;
  ngx_uint_t          i;


  for (i= 0; i < mcf->rvs->nelts; ++i){
    rv = (ngx_http_rv2_t*)ngx_array_get(mcf->rvs, i);
    if (ngx_str_eq(name, &rv->name)) {
      return i;
    }
  }

  return NGX_ERROR;

}
  static ngx_http_rv2_t*
ngx_http_rv2_find(ngx_conf_t *cf, ngx_str_t *name)
{
  mcf_t              *mcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  ngx_http_rv2_t *rv;
  ngx_uint_t          i;


  for (i= 0; i < mcf->rvs->nelts; ++i){
    rv = (ngx_http_rv2_t*)ngx_array_get(mcf->rvs, i);
    if (ngx_str_eq(name, &rv->name)) {
      return rv;
    }
  }

  return NULL;

}



  static void
ngx_http_rv2_get_code(ngx_http_script_engine_t *e)
{
  ngx_http_rv2_main_conf_t  *mcf;
  ngx_http_request_t        *r      = e->request;

  ngx_http_rv2_us_t         *us;
  ngx_http_rv2_mcd_t        *rmcd;
  memcached_st              *mcd;

  uint32_t                   hash;
  ngx_http_variable_value_t *hvv;
  ngx_http_variable_value_t *kvv;

  ngx_http_rv2_get_code_t   *code;

  ngx_http_rv2_op_data_t    *rvod;
  ngx_http_rv2_t            *rv;
  ngx_str_t                 usname;

  char                      *val = NULL;
  ngx_uint_t                 vlen;
  ngx_uint_t                 flag;
  memcached_return           rc;

  ngx_str_t                 *rcs    = &rv2_err_error;


  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv2_module);

  code = (ngx_http_rv2_get_code_t*)e->ip;
  e->ip += sizeof(ngx_http_rv2_get_code_t);
  
  rvod = &code->data;

  rv = rvod->rv;

  if (NGX_OK != ngx_http_rv2_eval_usname(r, rv, &usname)) {
    rcs = &rv2_err_invalid_us;
    logre("evaluating upstream name failed");

    goto error_return;
  }

  logr("upstream name of rv2:%V", &usname);


  us = (ngx_http_rv2_us_t*)hash_find(mcf->us_hash, &usname);
  if (HASH_ISUNSET(us)) {
    rcs = &rv2_err_nosuch_us;
    logre("can not find upstream with name :%V", &usname);

    goto error_return;
  }

  logr("got upstream : %V", &us->name);


  /* get from backend */
  hvv = ngx_http_get_indexed_variable(r, rv->hash_key_idx);
  if (!variable_isok(hvv)) {
    rcs = &rv2_err_hashkey;
    logre("hash key is not valid");

    goto error_return;
  }

  logr("hash key:%v", hvv);

  kvv = ngx_http_get_indexed_variable(r, rv->key_idx);
  if (!variable_isok(kvv)) {
    rcs = &rv2_err_key;
    logre("key variable is not valid");
    goto error_return;
  }

  logr("key:%v", kvv);

  hash = rv->hash_func(hvv->data, hvv->len);

  rmcd = ngx_array_get(us->mcd_array, hash % us->mcd_array->nelts);
  mcd = rmcd->mc;

  logr("memcache backend:%V", &rmcd->addr);

  val = memcached_get(mcd, (char*)kvv->data, kvv->len, &vlen, &flag, &rc);

  logr("get from memcache : rc=%d", rc);

  if (NULL != val)
    logr("get from memcache : value:%s", val);


  if (MEMCACHED_SUCCESS == rc) {

    u_char *spbuf = ngx_palloc(r->pool, sizeof(vlen));
    if (NULL == spbuf){
      rcs = &rv2_err_internal;
      logre("allocating for value failed, size:%d", vlen);

      goto error_return;
    }

    ngx_memcpy(spbuf, val, vlen);

    if (NULL != val) free(val);

    if (NGX_CONF_UNSET != rvod->rcidx) {
      e->sp->data = rv2_err_success.data;
      e->sp->len =  rv2_err_success.len;
      ++e->sp;
    }



    e->sp->data = spbuf;
    e->sp->len = vlen;
    ++e->sp;

    return;

  }






error_return:
  if (NULL != val) free(val);

  if (NGX_CONF_UNSET != rvod->rcidx) {
    e->sp->data = rcs->data;
    e->sp->len = rcs->len;
    ++e->sp;
  }


  e->sp->data = (u_char*)"";
  e->sp->len = 0;
  ++e->sp;

  return;
}

  static ngx_int_t
ngx_http_rv2_unreachable_get_handler(ngx_http_request_t *r, ngx_http_variable_value_t *v,
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


  static ngx_int_t
ngx_http_rv2_eval_usname(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t *v)
{
  if (NULL == ngx_http_script_run(r, v, rv->us_name_e.lengths->elts, 0, 
        rv->us_name_e.values->elts)) {
    return NGX_ERROR;
  }

  logr("evaluated us name:%V", v);
  return NGX_OK;
}

