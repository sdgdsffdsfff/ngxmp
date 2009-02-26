#ifndef __ngx_http_rv2_module_h__
#define __ngx_http_rv2_module_h__


typedef uint32_t (*ngx_http_rv2_hash_func_t)(u_char*, size_t);

typedef enum rv2_op_e {
  RV_UNKNOWN = 0, 
  RV_GET, 
  RV_SET, 
  RV_INCR, 
  RV_DECR, 
  RV_ADD, 
  RV_DELETE, 
  RV_NULL
}
rv_op_t;

typedef struct rv2_map_cmd_s {
  ngx_str_t cmd;
  rv_op_t op;

}
rv2_map_cmd_t;

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
  ngx_http_rv2_t      *rv;

  rv_op_t              optype;
  ngx_int_t            vidx;      /* varaible storing value       */
  ngx_int_t            rcidx;     /* variable storing return code */

  ngx_http_rv2_eval_t  v2set_e;   /* valule to set/add/incr/decr  */
}
ngx_http_rv2_op_data_t;

typedef struct {
  ngx_http_script_code_pt code;
  ngx_http_rv2_op_data_t  data;
}
ngx_http_rv2_getter_code_t;

typedef struct {
  ngx_http_script_code_pt code;
  ngx_http_rv2_op_data_t  data;
}
ngx_http_rv2_setter_code_t;



#define HASH_CRC32         0x01
#define HASH_CRC32_SHORT   0x02
#define HASH_5381          0x03


static const ngx_str_t hash_crc32       = ngx_string("crc32");
static const ngx_str_t hash_crc32_short = ngx_string("crc32_short");
static const ngx_str_t hash_5381        = ngx_string("5381");

static const ngx_str_t rv2_default_val  = ngx_string("NULL");

static ngx_str_t rv2_err_success        = ngx_string("SUCCESS");
static ngx_str_t rv2_err_notfound       = ngx_string("NOTFOUND");
static ngx_str_t rv2_err_notstored      = ngx_string("NOTSTORED");
static ngx_str_t rv2_err_error          = ngx_string("ERROR");
static ngx_str_t rv2_err_connection     = ngx_string("CONNECTION");
static ngx_str_t rv2_err_invalid_us     = ngx_string("INVALID_US");
static ngx_str_t rv2_err_nosuch_us      = ngx_string("NOSUCH_US");
static ngx_str_t rv2_err_internal       = ngx_string("INTERNAL");
static ngx_str_t rv2_err_hashkey        = ngx_string("HASHKEY");
static ngx_str_t rv2_err_key            = ngx_string("KEY");
static ngx_str_t rv2_err_invalid_val    = ngx_string("INVALID_VAL");


ngx_http_rv2_t *ngx_http_rv2_find (ngx_conf_t *cf, ngx_str_t *name);
ngx_int_t       ngx_http_rv2_if_exists (ngx_conf_t *cf, ngx_str_t *name);

#endif /* __ngx_http_rv2_module_h__ */

