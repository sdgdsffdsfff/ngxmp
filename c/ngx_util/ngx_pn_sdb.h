/**-------------------------/// nginx sdb protocal parser \\\---------------------------
 *
 * <b>sdb protocal parser</b>
 * @version : 1.0
 * @since : 2008 10 20 03:03:27 PM
 * 
 * @description :
 *   
 * @usage : 
 * 
 * @author : drdr.xp | drdr.xp@gmail.com
 * @copyright sina.com.cn 
 * @TODO : 
 *
 *--------------------------\\\ nginx sdb protocal parser ///---------------------------*/

#define pn_str_t    ngx_str_t
#define pn_buf_t    ngx_buf_t
#define pn_chain_t  ngx_chain_t
#define pn_pool_t   ngx_pool_t
#define pn_array_t   ngx_array_t


#include "protocal_engine.h"


#define S_RET "RET"


typedef struct {
  pn_str_t box_usage;
  pn_str_t code;
  pn_str_t message;
}
ngx_pn_sdb_ret_t;

typedef struct {
  pn_str_t accid;
  pn_str_t dom;
  pn_str_t itemname;
  pn_str_t attrname;
  pn_int   size;
}
ngx_pn_sdb_attr_t;

typedef struct {
  pn_str_t attrname;
  pn_int   bytes;
  pn_str_t data;
}
ngx_pn_sdb_attrv_t;

static pn_item_t[] its_get = {
  {PN_TP_EXPECT|PN_TP_STR , pn_string(S_RET)  , PN_ACT_PASS        , NULL}, 
  {PN_TP_IGNORE|PN_TP_CHAR, pn_string(S_SPACE), PN_ACT_PASS        , NULL}, 
  {PN_TP_UNTIL |PN_TP_CHAR, pn_string(S_SPACE), PN_ACT_CP|PN_DT_STR, NULL}, 
  {PN_TP_IGNORE|PN_TP_CHAR, pn_string(S_SPACE), PN_ACT_PASS        , NULL}, 
  {PN_TP_UNTIL |PN_TP_CHAR, pn_string(S_SPACE), PN_ACT_CP|PN_DT_STR, NULL}, 

  pn_item_null
}
