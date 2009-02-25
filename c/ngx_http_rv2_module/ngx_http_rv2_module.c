/**
 * TODO default value
 *
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
 * rv_get $value $varname $rc;                # rc is optional, value can be
 *    [success|timeout|notfound|invalid_us|nosuch_us|internal|error|connection]
 *
 * rv_set    	     $rv "$value" $rc;
 * rv_incr   $newval $rv "$value" $rc;
 * rv_decr   $newval $rv "$value" $rc;
 * rv_add            $rv "$value" $rc;
 * rv_delete         $rv          $rc;
 * rv_replace        $rv "$value" $rc;
 *
 */
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <libmemcached/memcached.h>

#include "ngx_util.h"
#include "ngx_http_rv2_module.h"


#define THIS  ngx_http_rv2_module

#define mcf_t ngx_http_rv2_main_conf_t
#define lcf_t ngx_http_rv2_loc_conf_t

#define rMCF() ngx_http_get_module_main_conf(r,THIS)
#define rLCF() ngx_http_get_module_loc_conf(r,THIS)

/* that is bad  */
typedef struct {
  ngx_array_t  *codes;        /* uintptr_t */
}
ngx_http_rewrite_loc_conf_pseudo_t;


static rv2_map_cmd_t cmd_map[] =
{
  {ngx_string("get")   , RV_GET},
  {ngx_string("set")   , RV_SET},
  {ngx_string("incr")  , RV_INCR},
  {ngx_string("decr")  , RV_DECR},
  {ngx_string("add")   , RV_ADD},
  {ngx_string("delete"), RV_DELETE},

  {ngx_null_string, RV_NULL}
};



static void      *ngx_http_rv2_create_main_conf (ngx_conf_t *cf);
static char      *ngx_http_rv2_init_main_conf (ngx_conf_t *cf, void *conf);
static ngx_int_t  ngx_http_rv2_post_conf (ngx_conf_t *cf);
static ngx_int_t  ngx_http_rv2_init_proc (ngx_cycle_t *cycle);

static char      *ngx_http_rv2_dec_command (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char      *ngx_http_rv2_getter_command (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static void       ngx_http_rv2_getter_code (ngx_http_script_engine_t *e);
static char      *ngx_http_rv2_code_set_gen (ngx_conf_t *cf, ngx_http_variable_t *v);


static ngx_int_t  ngx_http_rv2_eval_usname (ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t *v);

static ngx_int_t  ngx_http_rv2_add_var (ngx_conf_t *cf, ngx_str_t *name, ngx_http_variable_t **var);
static ngx_int_t  ngx_http_rv2_unreachable_get_handler (ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);

static ngx_http_rv2_mcd_node_t *rv2_get_mcd_node (ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_http_rv2_us_node_t *us, ngx_str_t **rcs);
static ngx_http_rv2_us_node_t  *rv2_get_us_node (ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t **rcs);
static char      *ngx_http_rv2_setter_command (ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t  ngx_http_rv2_eval_default_value (ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t *v);

/* FUNC_DEC_START */
/* FUNC_DEC_END */


extern ngx_module_t  ngx_http_rewrite_module;


static ngx_command_t
ngx_http_rv2_commands[] = {

  { ngx_string("rv_dec"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_1MORE,
    ngx_http_rv2_dec_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_get"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_2MORE,
    ngx_http_rv2_getter_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_incr"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_2MORE,
    ngx_http_rv2_setter_command,
    NGX_HTTP_LOC_CONF_OFFSET, 0, NULL },

  { ngx_string("rv_decr"),
    NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_2MORE,
    ngx_http_rv2_setter_command,
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

  ngx_http_rv2_us_node_t        *usnode;
  ngx_http_rv2_mcd_node_t       *mcdnode;

  ngx_uint_t                     i;
  ngx_uint_t                     j;


  mcf   = ngx_http_conf_get_module_main_conf(cf, ngx_http_rv2_module);
  usmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_upstream_module);


  mcf->uss = ngx_array_create(cf->pool, usmcf->upstreams.nelts, sizeof(ngx_http_rv2_us_node_t));
  if (NULL == mcf->uss) {
    logce("failed to allocate");
    return NGX_ERROR;
  }


  for (i= 0; i < usmcf->upstreams.nelts; ++i){
    usscf = *(ngx_http_upstream_srv_conf_t**)(ngx_array_get(&usmcf->upstreams, i));
    usnode = ngx_array_push(mcf->uss);

    usnode->name = usscf->host;
    logc("upstream %V added", &usnode->name);

    usnode->mcd_array = ngx_array_create(cf->pool, usscf->servers->nelts, sizeof(ngx_http_rv2_mcd_node_t));
    if (NULL == usnode->mcd_array) {
      logce("failed to allocate");
      return NGX_ERROR;
    }


    for (j= 0; j < usscf->servers->nelts; ++j){
      srv = ngx_array_get(usscf->servers, j);

      mcdnode = ngx_array_push(usnode->mcd_array);
      mcdnode->addr = srv->addrs->name;

      logc(" server %V added", &mcdnode->addr);
    }
  }



  /* create hash */
  mcf->us_hash = hash_create(cf->pool, mcf->uss->nelts*2);
  if (NULL == mcf->us_hash) {
    logce("creating upstream hash failed");
    return NGX_ERROR;
  }

  for (i= 0; i < mcf->uss->nelts; ++i){

    usnode = ngx_array_get(mcf->uss, i);

    logce("to add :%V", &usnode->name);

    if (NGX_OK != hash_padd(cf->pool, mcf->us_hash, &usnode->name, (intptr_t)usnode)) {
      logce("failed to add hash element %V", &usnode->name);
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
  ngx_http_rv2_us_node_t                  *us;
  ngx_http_rv2_mcd_node_t                 *mcd;

  ngx_uint_t                 i;
  ngx_uint_t                 j;
  ngx_int_t                  rc;


  /* cmcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_core_module); */
  mcf = ngx_http_cycle_get_module_main_conf(cycle, ngx_http_rv2_module);

  for (i= 0; i < mcf->uss->nelts; ++i){
    us = (ngx_http_rv2_us_node_t*)ngx_array_get(mcf->uss, i);

    for (j= 0; j < us->mcd_array->nelts; ++j){
      mcd = (ngx_http_rv2_mcd_node_t*)ngx_array_get(us->mcd_array, j);

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
  ngx_str_t                  default_val = rv2_default_val;

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
  if (ngx_http_rv2_if_exists(cf, &rv_name)) {
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
  sc.complete_lengths = 1;
  sc.complete_values = 1;

  rc = ngx_http_script_compile(&sc);
  if (NGX_OK != rc) {
    return "parsing default value expression failed";
  }

  return NGX_CONF_OK;
}

/**
 * get
 */
  static char *
ngx_http_rv2_getter_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
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

  ngx_http_rv2_getter_code_t            *code;

  ngx_http_script_compile_t  sc;
  ngx_http_variable_t                *v;
  char                               *res;


 rv2_map_cmd_t *tmp = cmd_map;
 logc("%x", tmp);



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


  valindex = ngx_http_rv2_add_var(cf, &val, &v);
  if (NGX_ERROR == valindex) {
    return NGX_CONF_ERROR;
  }


  code = ngx_http_script_start_code(cf->pool, &rlcf->codes, sizeof(ngx_http_rv2_getter_code_t));
  if (NULL == code) {
    logce("failed to create 'get' code");
    return NGX_CONF_ERROR;
  }


  code->code = ngx_http_rv2_getter_code;

  rvod = &code->data;

  rvod->vidx = valindex;


  rvod->rv = ngx_http_rv2_find(cf, &rvname);
  if (NULL == rvod->rv){
    logce("invalid rv name : %V", &rvname);
    return NGX_CONF_ERROR;
  }

  logce("rc name:%V", &rcname);

  if (0 < rcname.len) {
    rvod->rcidx = ngx_http_rv2_add_var(cf, &rcname, &rcv);
    if (NGX_ERROR == rvod->rcidx) {
      logce("failed to add varaible %V", &rcname);
      return NGX_CONF_ERROR;
    }
  }
  else {
    rvod->rcidx = NGX_CONF_UNSET;
  }



  if (NGX_CONF_OK != ngx_http_rv2_code_set_gen(cf, v)) {
    return NGX_CONF_ERROR;
  }

  if (NGX_CONF_UNSET != rvod->rcidx 
      && NGX_CONF_OK != ngx_http_rv2_code_set_gen(cf, rcv)) {
    return NGX_CONF_ERROR;
  }

  return NGX_CONF_OK;
}

/**
 * set
 * incr
 * decr
 * add
 * delete
 * replace
 */
  static char *
ngx_http_rv2_setter_command(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
  ngx_http_rewrite_loc_conf_pseudo_t *rlcf;

  ngx_http_rv2_op_data_t             *rvod;
  ngx_str_t                          *vals;
  ngx_uint_t                          nval;
  ngx_uint_t                          ival;

  ngx_str_t                           rvname;
  rv_op_t                             optype;

  ngx_http_rv2_setter_code_t         *code;

  ngx_http_variable_t                *newval;
  char                               *res;


  /* debug */
  /* rv2_map_cmd_t *tmp = cmd_map; */
  /* logc("%x", tmp); */



  rlcf   = ngx_http_conf_get_module_loc_conf(cf, ngx_http_rewrite_module);



  optype = rv2_get_cmd_type(cf, &cmd->name);
  if (RV_UNKNOWN == optype) {
    return "illegal command name";
  }



  code = ngx_http_script_start_code(cf->pool, &rlcf->codes, sizeof(ngx_http_rv2_setter_code_t));
  if (NULL == code) {
    logce("failed to create 'setter' code");
    return NGX_CONF_ERROR;
  }

  code->code = ngx_http_rv2_setter_code;

  rvod = &code->data;

  rvod->optype = optype;
  rvod->vidx = NGX_CONF_UNSET;
  rvod->rcidx = NGX_CONF_UNSET;


  vals = cf->args->elts;
  nval = cf->args->nelts;
  ival = 1; /* the first argument */



  /* need returned value */
  if (RV_INCR == optype || RV_DECR == optype) {

    ngx_str_t            newvalname;
    ngx_http_variable_t *newval;

    if (NULL != (
          res = ngx_http_get_var_name_str(&vals[ival], &newvalname))) {
      return res;
    }
    ++ival;

    rvod->vidx = ngx_http_rv2_add_var(cf, &newvalname, &newval);
    if (NGX_ERROR == rvod->vidx) {
      return NGX_CONF_ERROR;
    }


    if (NGX_CONF_OK != ngx_http_rv2_code_set_gen(cf, newval)) {
      return NGX_CONF_ERROR;
    }
  }



  /* rv */
  if (NULL != (
        res = ngx_http_get_var_name_str(&vals[ival], &rvname))) {
    return res;
  }
  ++ival;

  rvod->rv = ngx_http_rv2_find(cf, &rvname);
  if (NULL == rvod->rv){
    logce("invalid rv name : %V", &rvname);
    return NGX_CONF_ERROR;
  }


  /* no value needed to set */
  if (RV_DELETE != optype) {

    ngx_str_t v2set = ngx_null_string;

    if (NULL != (
          res = ngx_http_get_var_name_str(&vals[ival], &v2set))) {
      return res;
    }
    ++ival;


    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = cf;
    sc.source = &v2set;
    sc.lengths = &rvod->v2set.lengths;
    sc.values = &rvod->v2set.values;
    sc.complete_lengths = 1;
    sc.complete_values = 1;

    if (NGX_OK != ngx_http_script_compile(&sc)) {
      return "parse value_to_set expression failed";
    }

  }


  /* rc required? */
  if (ival < nval) {

    ngx_str_t            rcname;
    ngx_http_variable_t *rcv    = NULL;

    if (NULL != (
          res = ngx_http_get_var_name_str(&vals[3], &rcname))) {
      return res;
    }
    ++ival;

    logce("rc name:%V", &rcname);


    rvod->rcidx = ngx_http_rv2_add_var(cf, &rcname, &rcv);
    if (NGX_ERROR == rvod->rcidx) {
      logce("failed to add varaible %V", &rcname);
      return NGX_CONF_ERROR;
    }
    

    if (NGX_CONF_OK != ngx_http_rv2_code_set_gen(cf, rcv)) {
      return NGX_CONF_ERROR;
    }
  }


  return NGX_CONF_OK;
}

  static rv_op_t
rv2_get_cmd_type(ngx_conf_t *cf, ngx_str_t *cmd)
{
  ngx_str_t c;
  rv2_map_cmd_t *e;


  c = cmd;
  if (NULL == str_shift(c, sizeof("rv_"))) {
    logce("bad command format:%V", cmd);
    return RV_UNKNOWN;
  }

  for (e = cmd_map; RV_NULL != e.op; ++e) {
    if (ngx_str_eq(&c, &e.cmd)) {
      return e.op;
    }
  }

  logce("cant find corresponding command type:%V", cmd);
  return RV_UNKNOWN;
}

  static ngx_int_t
ngx_http_rv2_add_var(ngx_conf_t *cf, ngx_str_t *name, ngx_http_variable_t **var)
{
  ngx_http_variable_t *v;
  ngx_int_t            valindex;

  v = ngx_http_add_variable(cf, name, NGX_HTTP_VAR_CHANGEABLE);
  if (NULL == v) {
    logce("invalid variable name :%V", name);
    *var = NULL;
    return NGX_ERROR;
  }

  valindex = ngx_http_get_variable_index(cf, name);

  if (NULL == v->get_handler) { /* variable is not declared */
    /* copied from rewrite module */
    if (v->get_handler == NULL
        && ngx_strncasecmp(name->data, (u_char *) "http_", 5) != 0
        && ngx_strncasecmp(name->data, (u_char *) "sent_http_", 10) != 0
        && ngx_strncasecmp(name->data, (u_char *) "upstream_http_", 14) != 0)
    {
        v->get_handler = ngx_http_rv2_unreachable_get_handler;
        v->data = valindex;
    }
  }

  *var = v;

  return valindex;
}

  static char *
ngx_http_rv2_code_set_gen(ngx_conf_t *cf, ngx_http_variable_t *v)
{
  ngx_http_rewrite_loc_conf_pseudo_t *rlcf;
  ngx_http_script_var_handler_code_t *vhcode;
  ngx_http_script_var_code_t         *vcode;
  ngx_int_t                           valindex;




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


  ngx_int_t
ngx_http_rv2_if_exists(ngx_conf_t *cf, ngx_str_t *name)
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

  ngx_http_rv2_t*
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
ngx_http_rv2_getter_code(ngx_http_script_engine_t *e)
{
  ngx_http_rv2_main_conf_t  *mcf;
  ngx_http_request_t        *r      = e->request;

  ngx_http_rv2_us_node_t    *us;
  ngx_http_rv2_mcd_node_t   *rmcd;
  memcached_st              *mcd;


  ngx_http_rv2_getter_code_t   *code;

  ngx_http_rv2_op_data_t    *rvod;
  ngx_http_rv2_t            *rv;

  ngx_http_variable_value_t *kvv;

  char                      *val    = NULL;
  ngx_uint_t                 vlen;
  ngx_uint_t                 flag;
  memcached_return           rc;

  ngx_str_t                 *rcs    = &rv2_err_error;


  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv2_module);

  code = (ngx_http_rv2_getter_code_t*)e->ip;
  e->ip += sizeof(ngx_http_rv2_getter_code_t);

  rvod = &code->data;
  rv = rvod->rv;



  us = rv2_get_us_node(r, rv, &rcs);
  if (NULL == us) {
    goto error_return;
  }

  logr("got upstream : %V", &us->name);



  rmcd = rv2_get_mcd_node(r, rv, us, &rcs);
  if (NULL == rmcd) {
    goto error_return;
  }

  logr("memcache backend:%V", &rmcd->addr);


  mcd = rmcd->mc;


  kvv = ngx_http_get_indexed_variable(r, rv->key_idx);
  if (!variable_isok(kvv)) {
    rcs = &rv2_err_key;
    logre("key variable is not valid");

    goto error_return;
  }

  logr("rv2 key:%v", kvv);




  val = memcached_get(mcd, (char*)kvv->data, kvv->len, &vlen, &flag, &rc);

  logr("get from memcache : rc=%d", rc);

  if (NULL != val) {
    logr("get from memcache : value:%s", val);
  }


  if (MEMCACHED_SUCCESS == rc) {

    ngx_str_pbuf(vstr, r->pool, vlen);

    if (NULL == vstr.data){
      rcs = &rv2_err_internal;
      logre("allocating for value failed, size:%d", vlen);

      goto error_return;
    }

    ngx_memcpy(vstr.data, val, vlen);

    if (NULL != val) free(val);

    if (NGX_CONF_UNSET != rvod->rcidx) {
      stack_push(e->sp, &rv2_err_success);
    }


    stack_push(e->sp, &vstr);

    /* success return */
    return;
  }
  else if (MEMCACHED_NOTFOUND == rc) {
    rcs = &rv2_err_notfound;
    logr("value of the key[%v] is not found", kvv);

    goto error_return;
  }
  else if (MEMCACHED_ERRNO == rc && ECONNREFUSED == errno) {
    rcs = &rv2_err_connection;
    logr("connnection refused, errno:[%d]%s, rc:[%d]%s", errno, strerror(errno), rc, memcached_strerror(mcd, rc));

    goto error_return;
  }
  else { /* unknow error */
    rcs = &rv2_err_error;
    logr("unkown error, errno:[%d]%s, rc:[%d]%s", errno, strerror(errno), rc, memcached_strerror(mcd, rc));

    goto error_return;
  }


  return;




error_return:
  logr("error return, rcs:%V", rcs);

  if (NULL != val) free(val);

  if (NGX_CONF_UNSET != rvod->rcidx) {
    logr("set rcs:%V", rcs);
    stack_push(e->sp, rcs);
  }


  if  (&rv2_err_notfound == rcs) {
    ngx_str_t def;

    ngx_http_rv2_eval_default_value(r, rv, &def);
    stack_push(e->sp, &def);

    logr("set default value:%V", &def);
  }
  else {
    logr("set NULL value");
    e->sp->data = (u_char*)"";
    e->sp->len = 0;
    ++e->sp;
  }

  return;
}

  static void
ngx_http_rv2_setter_code(ngx_http_script_engine_t *e)
{
  ngx_http_rv2_main_conf_t   *mcf;
  ngx_http_request_t         *r    = e->request;

  memcached_st               *mcd;


  ngx_http_rv2_getter_code_t *code;

  ngx_http_rv2_op_data_t     *rvod;
  ngx_http_rv2_t             *rv;

  ngx_http_variable_value_t  *kvv;
  ngx_str_t v2set;
  ngx_int_t offset;

  char                       *val  = NULL;
  ngx_uint_t                  vlen;
  ngx_uint_t                  flag;
  memcached_return            rc;

  ngx_str_t                  *rcs  = &rv2_err_error;


  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv2_module);

  code = (ngx_http_rv2_getter_code_t*)e->ip;
  e->ip += sizeof(ngx_http_rv2_getter_code_t);

  rvod = &code->data;
  rv = rvod->rv;

  mcd = rv2_get_mcd(r, rv, &rcs);
  if (NULL == mcd) {
    goto error_return;
  }



  kvv = ngx_http_get_indexed_variable(r, rv->key_idx);
  if (!variable_isok(kvv)) {
    rcs = &rv2_err_key;
    logre("key variable is not valid");

    goto error_return;
  }

  logr("rv2 key:%v", kvv);



  if (NULL == ngx_http_script_run(r, v2set, rvod->v2set.lengths->elts, 0,
        rvod->v2set.values->elts)) {

    rcs = &rv2_err_internal;
    logre("evaluating value to set failed");

    goto error_return;
  }

  if (RV_INCR == rvod->optype || RV_DECR == rvod->optype) {
    offset = ngx_atoi(v2set.data, v2set.len);
    if (NGX_ERROR == offset) {
      rcs = &rv2_err_invalid_val;

      logre("value to incr/decr is not a valid number");
      goto error_return;
    }
  }


  switch (rvod->optype) {
    case RV_SET :
      rc = memcached_set(mcd, (char*)kvv->data, kvv->len, v2set.data, v2set.len, 0 /* exp */, 0 /* flag */);
    case RV_INCR :
    case RV_DECR :
    case RV_ADD :
    case RV_DELETE :
    default:
      rcs = &rv2_err_internal;
      logre("invalid optype[%d] for setter code", rvod->optype);
      goto error_return;
  }

  val = memcached_get(mcd, (char*)kvv->data, kvv->len, &vlen, &flag, &rc);

  logr("get from memcache : rc=%d", rc);

  if (NULL != val) {
    logr("get from memcache : value:%s", val);
  }


  if (MEMCACHED_SUCCESS == rc) {

    ngx_str_pbuf(vstr, r->pool, vlen);

    if (NULL == vstr.data){
      rcs = &rv2_err_internal;
      logre("allocating for value failed, size:%d", vlen);

      goto error_return;
    }

    ngx_memcpy(vstr.data, val, vlen);

    if (NULL != val) free(val);

    if (NGX_CONF_UNSET != rvod->rcidx) {
      stack_push(e->sp, &rv2_err_success);
    }


    stack_push(e->sp, &vstr);

    /* success return */
    return;
  }
  else if (MEMCACHED_NOTFOUND == rc) {
    rcs = &rv2_err_notfound;
    logr("value of the key[%v] is not found", kvv);

    goto error_return;
  }
  else if (MEMCACHED_ERRNO == rc && ECONNREFUSED == errno) {
    rcs = &rv2_err_connection;
    logr("connnection refused, errno:[%d]%s, rc:[%d]%s", errno, strerror(errno), rc, memcached_strerror(mcd, rc));

    goto error_return;
  }
  else { /* unknow error */
    rcs = &rv2_err_error;
    logr("unkown error, errno:[%d]%s, rc:[%d]%s", errno, strerror(errno), rc, memcached_strerror(mcd, rc));

    goto error_return;
  }


  return;




error_return:
  logr("error return, rcs:%V", rcs);

  if (NULL != val) free(val);

  if (NGX_CONF_UNSET != rvod->rcidx) {
    logr("set rcs:%V", rcs);
    stack_push(e->sp, rcs);
  }


  if  (&rv2_err_notfound == rcs) {
    ngx_str_t def;

    ngx_http_rv2_eval_default_value(r, rv, &def);
    stack_push(e->sp, &def);

    logr("set default value:%V", &def);
  }
  else {
    logr("set NULL value");
    e->sp->data = (u_char*)"";
    e->sp->len = 0;
    ++e->sp;
  }

  return;
}

  static memcached_st *
rv2_get_mcd(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t **rcs)
{
  ngx_http_rv2_us_node_t     *us;
  ngx_http_rv2_mcd_node_t    *rmcd;

  us = rv2_get_us_node(r, rv, &rcs);
  if (NULL == us) {
    goto return NULL;
  }

  logr("got upstream : %V", &us->name);



  rmcd = rv2_get_mcd_node(r, rv, us, &rcs);
  if (NULL == rmcd) {
    return NULL;
  }

  logr("memcache backend:%V", &rmcd->addr);


  return rmcd->mc;
}

  static ngx_http_rv2_us_node_t *
rv2_get_us_node(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t **rcs)
{
  ngx_http_rv2_main_conf_t *mcf;
  ngx_str_t                 usname;
  ngx_http_rv2_us_node_t   *us;



  mcf = ngx_http_get_module_main_conf(r, ngx_http_rv2_module);

  if (NGX_OK != ngx_http_rv2_eval_usname(r, rv, &usname)) {
    *rcs = &rv2_err_invalid_us;
    logre("evaluating upstream name failed");

    return NULL;
  }

  logr("upstream name of rv2:%V", &usname);


  us = (ngx_http_rv2_us_node_t*)hash_find(mcf->us_hash, &usname);
  if (HASH_ISUNSET(us)) {
    *rcs = &rv2_err_nosuch_us;
    logre("can not find upstream with name :%V", &usname);

    return NULL;
  }

  logr("got upstream : %V", &us->name);

  return us;
}

  static ngx_http_rv2_mcd_node_t *
rv2_get_mcd_node(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_http_rv2_us_node_t *us, ngx_str_t **rcs)
{
  uint32_t                   hash;
  ngx_http_variable_value_t *hvv;
  ngx_http_rv2_mcd_node_t   *rmcd;



  /* get from backend */
  hvv = ngx_http_get_indexed_variable(r, rv->hash_key_idx);
  if (!variable_isok(hvv)) {
    *rcs = &rv2_err_hashkey;
    logre("hash key is not valid");

    return NULL;
  }

  logr("rv2 hash key:%v", hvv);




  hash = rv->hash_func(hvv->data, hvv->len);

  rmcd = ngx_array_get(us->mcd_array, hash % us->mcd_array->nelts);

  return rmcd;
}

/**
 * for a variable set by rv2
 */
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


  static ngx_int_t
ngx_http_rv2_eval_default_value(ngx_http_request_t *r, ngx_http_rv2_t *rv, ngx_str_t *v)
{
  if (NULL == ngx_http_script_run(r, v, rv->default_val_e.lengths->elts, 0,
        rv->default_val_e.values->elts)) {
    return NGX_ERROR;
  }

  logr("evaluated default value:%V", v);
  return NGX_OK;

}

