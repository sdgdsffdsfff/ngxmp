
#include  <stdio.h>
#include  <stdint.h>
#include  <stdlib.h>


#define PN_POOL_SIZE 4096


#ifndef pn_str_t
#	define pn_str_t _pn_string_t
#endif

#ifndef pn_string
#	define pn_string(s) {sizeof(s)-1, (pn_uchar*)s}
#endif

#ifndef pn_buf_t
#	define pn_buf_t _pn_buf_t
#endif

#ifndef pn_chain_t
#	define pn_chain_t _pn_chain_t
#endif

#ifndef pn_pool_t
#	define pn_pool_t _pn_pool_t
#endif





typedef uintptr_t     pn_uint;
typedef intptr_t      pn_int;

typedef unsigned char pn_uchar;
typedef char          pn_char;





typedef pn_int        pn_status_t;
typedef pn_int        pn_type_t;






typedef struct {
  pn_uint len;
  pn_uchar *data;
}
_pn_string_t;

typedef struct {
  pn_uchar *start;
  pn_uchar *end;
  pn_uchar *pos;
  pn_uchar *last;
}
_pn_buf_t;

typedef struct _pn_chain_s {
  pn_buf_t *buf;
  struct _pn_chain_s *next;
}
_pn_chain_t;


typedef struct {
  pn_chain_t *ch;
  pn_uchar *c;
}
pn_pos_t;

typedef struct pn_pool_s pn_pool_t;
struct pn_pool_s {
  pn_uchar   *last;
  pn_uchar   *end;

  pn_pool_t  *current;
  pn_chain_t *chain;
  pn_pool_t  *next;

  void       *large;

  void       *cleanup;
  void       *log;
};


/**
 * parser item 
 * it defines what to do.
 */
typedef struct {
  pn_type_t  type;     /* type                  */
  pn_str_t   expect;   /* what to look for      */
  pn_type_t  action;   /* what to do when found */
  void      *offset;   /* where to store        */
}
pn_item_t;


#define pn_item_null {0, {0, NULL}, 0, NULL}

typedef struct {
  pn_uint   *nitem;       /* how many states */

  pn_item_t *item_cur;
  pn_item_t *items;

  pn_pos_t   cur;         /* cur position    */
  pn_pos_t   last;        /* another post    */

  pn_chain_t *raw;

  void *data;

}
pn_engine_t;



#define PN_TP_CHAR    (1<<0 )
#define PN_TP_STR     (1<<1 )
#define PN_TP_IGNORE  (1<<2 )
#define PN_TP_EXPECT  (1<<3 )
#define PN_TP_UNTIL   (1<<4 )
#define PN_ACT_REF    (1<<5 )
#define PN_ACT_CP     (1<<6 )
#define PN_ACT_PASS   (1<<7 )
#define PN_DT_INT     (1<<7 )
#define PN_DT_STR     (1<<7 )
/* #define pn_act_ignore (1<<8 ) */
/* #define pn_act_ignore (1<<9 ) */
/* #define pn_act_ignore (1<<10) */
/* #define pn_act_ignore (1<<11) */


#define S_SPACE " "
#define S_CRLF  "\r\n"

#define pn_set_string(s,e,str_p)         \
    do {                                 \
      if ((e)<(s))                       \
        (str_p)->len = 0;                \
      else                               \
        (str_p)->len = (e) - (s);        \
      (str_p)->data = (s);               \
    } while(0) 




