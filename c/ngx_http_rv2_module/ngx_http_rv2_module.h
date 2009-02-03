#ifndef __ngx_http_rv2_module_h__
#define __ngx_http_rv2_module_h__


typedef uint32_t (*ngx_http_rv2_hash_func_t)(u_char*, size_t);

typedef struct ngx_http_rv2_mcd_node_s {
  ngx_str_t     addr;
  memcached_st *mc;
}
ngx_http_rv2_mcd_node_t;

typedef struct {
  ngx_str_t    name;
  ngx_array_t *mcd_array;   /* ngx_http_rv2_mcd_node_t */
}
ngx_http_rv2_us_node_t;

typedef struct {
  ngx_array_t *lengths;
  ngx_array_t *values;
}
ngx_http_rv2_eval_t;

typedef struct {
  ngx_array_t *rvs;       /* ngx_http_rv2_t    */
  ngx_array_t *uss;       /* ngx_http_rv2_us_node_t */
  hash_t      *us_hash;
}
ngx_http_rv2_main_conf_t;

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
}
ngx_http_rv2_t;


typedef struct {
  ngx_int_t       vidx;
  ngx_int_t       rcidx;
  ngx_http_rv2_t *rv;
}
ngx_http_rv2_op_data_t;

typedef struct {
  ngx_http_script_code_pt code;
  ngx_http_rv2_op_data_t  data;
}
ngx_http_rv2_get_code_t;



#define HASH_CRC32         0x01
#define HASH_CRC32_SHORT   0x02
#define HASH_5381          0x03


static const ngx_str_t hash_crc32       = ngx_string("crc32");
static const ngx_str_t hash_crc32_short = ngx_string("crc32_short");
static const ngx_str_t hash_5381        = ngx_string("5381");

static const ngx_str_t rv2_default_val  = ngx_string("NULL");

static ngx_str_t rv2_err_success        = ngx_string("success");
static ngx_str_t rv2_err_notfound       = ngx_string("notfound");
static ngx_str_t rv2_err_error          = ngx_string("error");
static ngx_str_t rv2_err_invalid_us     = ngx_string("invalid_us");
static ngx_str_t rv2_err_nosuch_us      = ngx_string("nosuch_us");
static ngx_str_t rv2_err_internal       = ngx_string("internal");
static ngx_str_t rv2_err_hashkey        = ngx_string("hashkey");
static ngx_str_t rv2_err_key            = ngx_string("key");


ngx_http_rv2_t *ngx_http_rv2_find (ngx_conf_t *cf, ngx_str_t *name);
ngx_int_t       ngx_http_rv2_if_exists (ngx_conf_t *cf, ngx_str_t *name);

#endif /* __ngx_http_rv2_module_h__ */

