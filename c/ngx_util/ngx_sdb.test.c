#include "pn_sdb.h"


int test_ret_attr_cut(pn_engine_t *e) {
  pn_int rc = -1;
  sdb_ret_attr_t *av;
  pn_int i;


  av = (sdb_ret_attr_t *)e->data.data;

  i = 0;
  e->raw->buf->last -= 15;
  while (PN_OK != rc && 20 > ++i) {

    rc = pn_engine_parse(e);
    if (PN_OK != rc) {
      pnlogerr("parse failed with rc[%x]", rc);
    }

    ++e->raw->buf->last;

  }
  

  pnlog("uid:%.*s, domain:%.*s, itemname:%.*s, attrname:%.*s, size:%d, attrval_str:%.*s", 
      strp(av->uid), 
      strp(av->domain), 
      strp(av->itemname), 
      strp(av->attrname), 
      av->size, 
      strp(av->attrval_str));

}

int main(int argv, char **args){


  pn_engine_t    *engine = NULL;
  sdb_ret_attr_t  av;
  /* pn_str_t        raw    = pn_string("ATTR uid domain item1 color 3\r\nred_green\r\n"); */
  pn_str_t        raw    = pn_string("ATTR uid domain item1 color 3\r\nred\r\n");
  /* pn_str_t        raw    = pn_string("ATTR uid domain item1 colo"); */
  pn_chain_t      ch;
  pn_buf_t        buf;
  pn_int          rc;



  rc = pn_engine_create(NULL, &engine, its_attr);
  if (PN_OK != rc) {
    pnlogerr("create engine failed");
    return rc;
  }


  memset(&av, 0, sizeof(sdb_ret_attr_t));
  pn_engine_set_data(engine, &av, sizeof(sdb_ret_attr_t));


  buf.pos = raw.data;
  buf.last = raw.data + raw.len;

  ch.buf = &buf;
  ch.next = NULL;


  pn_engine_set_raw(engine, &ch);

  test_ret_attr_cut(engine);
  pnlog("test done");
  exit(0);







  rc = pn_engine_parse(engine);
  if (PN_OK != rc) {
    pnlogerr("parse failed with rc[%d]", rc);
    return rc;
  }


  pnlog("uid:%.*s, domain:%.*s, itemname:%.*s, attrname:%.*s, size:%d, attrval_str:%.*s", 
      strp(av.uid), 
      strp(av.domain), 
      strp(av.itemname), 
      strp(av.attrname), 
      av.size, 
      strp(av.attrval_str));
  
  
  return 0;
}
