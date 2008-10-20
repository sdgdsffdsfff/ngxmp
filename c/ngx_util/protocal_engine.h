
#include  <stdio.h>
#include  <stdint.h>
#include  <stdlib.h>


#define PN_POOL_SIZE 4096


#ifndef pn_string_t
#	define pn_string_t _pn_string_t
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

typedef struct pn_pool_s {
  pn_uchar         *last;
  pn_uchar         *end;

  struct pn_pool_s *current;
  pn_chain_t       *chain;
  struct pn_pool_s *next;

  void             *large;

  void             *cleanup;
  void             *log;
}
_pn_pool_t;


/* parser item */
typedef struct {
  pn_type_t    type;                            /* type */
  pn_string_t  expect;                          /* what to look for */

  pn_type_t    action;                          /* what to do when found */

  void        *where;                           /* where to store */

}
pn_item_t;

typedef struct {
  pn_uint *nstat;

  pn_status_t *cur;
  pn_status_t *head;
  pn_status_t *end;




}
pn_engine_t;




#define pn_set_string(s,e,str_p)         \
    do {                                 \
      if ((e)<(s))                       \
        (str_p)->len = 0;                \
      else                               \
        (str_p)->len = (e) - (s);        \
      (str_p)->data = (s);               \
    } while(0) 




