
#define MC_CMD_GET    "get"
#define MC_CMD_SET    "set"
#define MC_CMD_INCR   "incr"
#define MC_CMD_DECR   "decr"

#define MC_CMD_VALUE  "VALUE"
#define MC_CMD_END    "END"

#define MC_DELIM      ' '

#define MC_LINE_BREAK "\r\n"

#	define logmc(fmt ...)  do{fprintf(stderr, ":::"fmt);fprintf(stderr, "\n");}while(0)
#	define logmce(fmt ...) do{fprintf(stderr, ":::"fmt);fprintf(stderr, "\n");}while(0)

typedef enum {
  MC_ST_START,
  MC_ST_LINE_BREAK, 
  MC_ST_CMD,
  MC_ST_KEY,
  MC_ST_VAL,
  MC_ST_EXP,
  MC_ST_FLAG,
  MC_ST_LEN,

  MC_ST_DELIM,
  MC_ST_END,

  MC_ST_UNTIL_DELIM, 
  MC_ST_UNTIL_END, 
  MC_ST_UNTIL_LB, 

  MC_ST_NULL
}
mc_status_t;

static ngx_str_t st_txt[] = {
  ngx_string("MC_ST_START"), 
  ngx_string("MC_ST_LINE_BREAK"), 
  ngx_string("MC_ST_CMD"), 
  ngx_string("MC_ST_KEY"), 
  ngx_string("MC_ST_VAL"), 
  ngx_string("MC_ST_EXP"), 
  ngx_string("MC_ST_FLAG"), 
  ngx_string("MC_ST_LEN"), 

  ngx_string("MC_ST_DELIM"), 
  ngx_string("MC_ST_END"), 

  ngx_string("MC_ST_UNTIL_DELIM"), 
  ngx_string("MC_ST_UNTIL_END"), 
  ngx_string("MC_ST_UNTIL_LB"), 

  ngx_string("MC_ST_NULL")
};

typedef enum {
  MC_T_GET, 
  MC_T_SET, 
  MC_T_INCR, 
  MC_T_DECR, 
  MC_T_VALUE, 

  MC_T_NULL
}
mc_p_t;

typedef struct {
  mc_p_t       type;

  mc_status_t *sts;
  mc_status_t *st;

  ngx_chain_t *cur;
  u_char      *c;

  ngx_chain_t *pre_cl;
  u_char      *pre_c;

  ngx_str_t    cmd;
  ngx_str_t    key;
  ngx_str_t    val;
  ngx_str_t    exp;
  ngx_str_t    flag;
  ngx_str_t    len;
}
mc_data;

static mc_status_t mc_get_req[] = {
  MC_ST_START, 
  MC_ST_CMD, MC_ST_DELIM, MC_ST_UNTIL_LB, MC_ST_KEY, MC_ST_LINE_BREAK, 
  MC_ST_NULL
};
static mc_status_t mc_get_v_req[] = {
  MC_ST_START, 
  MC_ST_CMD, MC_ST_DELIM, 
  MC_ST_UNTIL_DELIM, MC_ST_KEY,  MC_ST_DELIM, 
  MC_ST_UNTIL_DELIM, MC_ST_FLAG, MC_ST_DELIM,
  MC_ST_UNTIL_DELIM, MC_ST_EXP,  MC_ST_DELIM,
  MC_ST_UNTIL_LB,    MC_ST_LEN,  MC_ST_LINE_BREAK, 
  MC_ST_UNTIL_LB,    MC_ST_VAL,  MC_ST_LINE_BREAK, 
  MC_ST_END,  MC_ST_LINE_BREAK, 

  MC_ST_NULL
};

static ngx_str_t mc_cmd_linebreak  = ngx_string(MC_LINE_BREAK);
static ngx_str_t mc_cmd_end  = ngx_string(MC_CMD_END);

static ngx_str_t mc_cmds[] = {
  ngx_string(MC_CMD_GET),  
  ngx_string(MC_CMD_SET),  
  ngx_string(MC_CMD_INCR), 
  ngx_string(MC_CMD_DECR), 
  ngx_string(MC_CMD_VALUE), 

  ngx_null_string
};



  static ngx_int_t
mc_parse_init(ngx_pool_t *p, mc_data **d, mc_status_t *sts, mc_p_t tp)
{

  mc_data *dt;

  if (NULL == *d) {
    /* TODO uncomment me */
    /* *d = ngx_pcalloc(p, sizeof(mc_data)); */
    if (NULL == *d){ return NGX_ERROR; }

  }

  dt = *d;
  memset(dt, 0, sizeof(mc_data));
  dt->sts  = sts;
  dt->st   = sts;
  dt->type = tp;
  dt->cmd  = mc_cmds[tp];

  return NGX_OK;
}

  static ngx_int_t
ngx_chain_char_move(ngx_chain_t **ch, u_char *c, ngx_int_t n) 
{
  c+=n;
  while (c >= (*ch)->buf->last) {
    if (NULL == (*ch)->next) { return NGX_ERROR; }

    *ch = (*ch)->next;
    c = (*ch)->buf->pos;
  } 
  return NGX_OK;
}

  static ngx_int_t
mc_remember_cur(mc_data *d) 
{
  if (NULL == d->pre_cl && NULL == d->pre_c) {
    d->pre_cl = d->cur;
    d->pre_c = d->c;
    return NGX_OK;
  }

  return NGX_ERROR;
}

  static ngx_int_t
mc_release_pre(mc_data *d) 
{
  if (NULL != d->pre_cl && NULL != d->pre_c) {
    d->pre_cl = NULL;
    d->pre_c  = NULL;
    return NGX_OK;
  }

  return NGX_ERROR;
}


  static ngx_int_t
mc_set_data_str(mc_data *d, size_t ofs)
{

  ngx_int_t rc;
  ngx_str_t *str;

  if (NULL == d->pre_cl || NULL == d->pre_c) {
    return NGX_ERROR;
  }

  if (d->pre_cl != d->cur) {
    return NGX_ERROR;
  }

  str = (ngx_str_t*)(((u_char*)d) + ofs);

  str->data = d->pre_c;
  str->len  = d->c - d->pre_c;

  rc = mc_release_pre(d);
  if (NGX_OK != rc){ return rc; }

  return NGX_OK;

}

  static ngx_int_t
mc_parse(mc_data *d, ngx_chain_t *ch)
{

  ngx_int_t  rc;
  ngx_int_t  found;

  if (NULL == ch || NULL == ch->buf) { return NGX_ERROR; }


  if (NULL == d->cur) {
    logmc("set d->cur");
    d->cur = ch;
  }

  if (NULL == d->c) {
    d->c = d->cur->buf->pos;
  }



  while (1) {
    logmc("status:%s", st_txt[*d->st].data);

    switch (*d->st) {
      case MC_ST_START:  {
        ++d->st;
        logmc("start -> next");
        break;
      }

      case MC_ST_LINE_BREAK: {
        if (d->cur->buf->last < d->c + 2) {
          logmc("not enought chars for line break:%p, %p", d->cur->buf->last, d->c);
          return NGX_AGAIN;
        }

        if (d->c[0] == mc_cmd_linebreak.data[0]
            && d->c[1] == mc_cmd_linebreak.data[1]) {
          d->c += 2;
          ++d->st;
        }
        else {
          logmc("cant find line break");
          return NGX_ERROR;
        }
        break;
      }

      case MC_ST_CMD: {
        if (NULL == d->cmd.data) {
          logmce("expect command but command is null");
          return NGX_ERROR;
        }

        if (d->cur->buf->last < d->c + d->cmd.len) {
          return NGX_AGAIN;
        }

        if (0 == ngx_strncmp(d->cmd.data, d->c, d->cmd.len)) {
          d->c += d->cmd.len;
          ++d->st;
        }
        else {
          return NGX_ERROR;
        }

        break;
      }

      case MC_ST_KEY : {
        rc = mc_set_data_str(d, offsetof(mc_data, key));
        if (NGX_OK != rc) { return rc; }

        ++d->st;

        break;
      }

      case MC_ST_VAL : {
        rc = mc_set_data_str(d, offsetof(mc_data, val));
        if (NGX_OK != rc) { return rc; }

        ++d->st;

        break;
      }

      case MC_ST_EXP : {
        rc = mc_set_data_str(d, offsetof(mc_data, exp));
        if (NGX_OK != rc) { return rc; }

        ++d->st;

        break;
      }

      case MC_ST_FLAG : {
        rc = mc_set_data_str(d, offsetof(mc_data, flag));
        if (NGX_OK != rc) { return rc; }

        ++d->st;

        break;
      }

      case MC_ST_LEN : {
        rc = mc_set_data_str(d, offsetof(mc_data, len));
        if (NGX_OK != rc) { return rc; }

        ++d->st;

        break;
      }

      case MC_ST_DELIM : {
        if (d->cur->buf->last < d->c + 1) {
          return NGX_AGAIN;
        }

        if (MC_DELIM == d->c[0]) {
          ++d->c;
          ++d->st;
          break;
        }
        else {
          return NGX_ERROR;
        }

        break;
      }

      case MC_ST_END : {
        if (d->cur->buf->last < d->c + mc_cmd_end.len) {
          return NGX_AGAIN;
        }

        if (0 == ngx_strncmp(mc_cmd_end.data, d->c, mc_cmd_end.len)) {
          d->c += mc_cmd_end.len;
          ++d->st;
        }
        else {
          return NGX_ERROR;
        }
        break;
      }

      case MC_ST_UNTIL_DELIM : {

        rc = mc_remember_cur(d);
        if (NGX_OK != rc) { return rc; }

        while (MC_DELIM != d->c[0] && d->c < d->cur->buf->last) {
          ++d->c;
        }

        if (MC_DELIM == d->c[0]) {
          ++d->st;
        }
        else {
          return NGX_AGAIN;
        }

        break;
      }

      case MC_ST_UNTIL_END : {
        rc = mc_remember_cur(d);
        if (NGX_OK != rc) { return rc; }

        found = 0;
        while (d->c < d->cur->buf->last) {

          if (d->c[0] == mc_cmd_end.data[0]) {
            if (0 == ngx_strncmp(d->c, mc_cmd_end.data, mc_cmd_end.len)) {
              /* d->c += mc_cmd_end.len; */
              ++d->st;
              found = 1;
              break;
            }
          }

          ++d->c;
        }

        if (!found) {
          return NGX_AGAIN;
        }

        break;
      }

      case MC_ST_UNTIL_LB : {
        rc = mc_remember_cur(d);
        if (NGX_OK != rc) { return rc; }

        found = 0;
        while (d->c < d->cur->buf->last) {

          if (d->c[0] == mc_cmd_linebreak.data[0]) {
            if (0 == ngx_strncmp(d->c, mc_cmd_linebreak.data, mc_cmd_linebreak.len)) {
              /* d->c += mc_cmd_linebreak.len; */
              ++d->st;
              found = 1;
              break;
            }
          }

          ++d->c;
        }

        if (!found) {
          return NGX_AGAIN;
        }

        break;
      }

      case MC_ST_NULL : {
        return NGX_OK;
        break;
      }

    }

  }

}
