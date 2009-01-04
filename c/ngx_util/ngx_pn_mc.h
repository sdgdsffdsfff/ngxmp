/**-------------------------/// nginx mc protocol parser \\\---------------------------
 *
 * <b>mc protocol parser</b>
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
 *--------------------------\\\ nginx mc protocol parser ///---------------------------*/

#define pn_str_t    ngx_str_t
#define pn_buf_t    ngx_buf_t
#define pn_chain_t  ngx_chain_t
#define pn_pool_t   ngx_pool_t


#include "protocol_engine.h"


static pn_item_t its_get[] = {
  {PN_TP_EXPECT|PN_TP_STR, pn_string("get"), PN_ACT_PASS, NULL}
}
